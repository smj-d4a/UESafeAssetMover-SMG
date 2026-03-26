// Copyright FB Team. All Rights Reserved.

#include "AssetWorkloadDistributor.h"
#include "AssetDependencyAnalyzer.h"
#include "Algo/MinElement.h"
#include "Algo/MaxElement.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "HAL/FileManager.h"

// ── Union-Find (경로 압축) ──────────────────────────────────────

static FName FindRoot(TMap<FName, FName>& Parent, const FName& X)
{
	FName* P = Parent.Find(X);
	if (!P || *P == X) return X;

	// 경로 압축
	const FName Root = FindRoot(Parent, *P);
	Parent.Add(X, Root);
	return Root;
}

static void UnionNodes(TMap<FName, FName>& Parent, const FName& A, const FName& B)
{
	const FName RootA = FindRoot(Parent, A);
	const FName RootB = FindRoot(Parent, B);
	if (RootA != RootB)
	{
		Parent.Add(RootA, RootB);
	}
}

// ───────────────────────────────────────────────────────────────

TArray<TSet<FName>> FAssetWorkloadDistributor::FindConnectedComponents(
	const TArray<TSharedPtr<FAssetMoverEntry>>& Entries,
	const FDependencyGraph& Graph)
{
	TMap<FName, FName> Parent;

	// 모든 에셋을 자기 자신의 루트로 초기화
	for (const TSharedPtr<FAssetMoverEntry>& Entry : Entries)
	{
		if (!Entry.IsValid()) continue;
		const FName Pkg = Entry->AssetData.PackageName;
		Parent.FindOrAdd(Pkg, Pkg);
	}

	// InternalDeps를 무방향으로 처리하여 Union
	for (const auto& Pair : Graph.InternalDeps)
	{
		const FName& A = Pair.Key;
		for (const FName& B : Pair.Value)
		{
			// 그래프에 없는 노드가 혹시 있으면 초기화
			Parent.FindOrAdd(A, A);
			Parent.FindOrAdd(B, B);
			UnionNodes(Parent, A, B);
		}
	}

	// 루트별로 그룹화
	TMap<FName, TSet<FName>> Groups;
	for (const TSharedPtr<FAssetMoverEntry>& Entry : Entries)
	{
		if (!Entry.IsValid()) continue;
		const FName Pkg  = Entry->AssetData.PackageName;
		const FName Root = FindRoot(Parent, Pkg);
		Groups.FindOrAdd(Root).Add(Pkg);
	}

	TArray<TSet<FName>> Result;
	Groups.GenerateValueArray(Result);
	return Result;
}

void FAssetWorkloadDistributor::AssignWorkers(
	TArray<TSharedPtr<FAssetMoverEntry>>& Entries,
	const FDependencyGraph& Graph,
	int32 MaxWorkers)
{
	if (MaxWorkers <= 0 || Entries.Num() == 0) return;

	// PackageName → Entry 빠른 조회 (로그·순환 의존성 판별용)
	TMap<FName, const FAssetMoverEntry*> PkgToEntry;
	for (const TSharedPtr<FAssetMoverEntry>& E : Entries)
	{
		if (E.IsValid())
		{
			PkgToEntry.Add(E->AssetData.PackageName, E.Get());
		}
	}

	TArray<TSet<FName>> Components = FindConnectedComponents(Entries, Graph);

	// 큰 섬부터 배정 (크기 내림차순)
	Components.Sort([](const TSet<FName>& A, const TSet<FName>& B)
	{
		return A.Num() > B.Num();
	});

	// 작업자별 현재 부하 / 섬 수 추적
	TArray<int32> WorkerLoad;
	TArray<int32> WorkerIslandCount;
	WorkerLoad.SetNumZeroed(MaxWorkers);
	WorkerIslandCount.SetNumZeroed(MaxWorkers);

	// PackageName → AssigneeIndex 맵
	TMap<FName, int32> PkgToAssignee;

	// ── 로그 파일 준비 ─────────────────────────────────────────
	const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	const FString LogDir    = FPaths::ProjectSavedDir() / TEXT("SafeAssetMover");
	const FString LogFile   = LogDir / FString::Printf(TEXT("Assignment_%s.log"), *Timestamp);

	IFileManager::Get().MakeDirectory(*LogDir, /*Tree=*/true);

	TArray<FString> FileLines;

	// Log 레벨: UE_LOG + 파일
	auto L = [&](const FString& Msg)
	{
		UE_LOG(LogSafeAssetMover, Log, TEXT("%s"), *Msg);
		FileLines.Add(Msg);
	};
	// Warning 레벨: UE_LOG Warning + 파일에 [WARNING] 접두어
	auto LW = [&](const FString& Msg)
	{
		UE_LOG(LogSafeAssetMover, Warning, TEXT("%s"), *Msg);
		FileLines.Add(TEXT("[WARNING] ") + Msg);
	};
	// Verbose 레벨: UE_LOG Verbose + 파일에는 항상 기록
	auto LV = [&](const FString& Msg)
	{
		UE_LOG(LogSafeAssetMover, Verbose, TEXT("%s"), *Msg);
		FileLines.Add(Msg);
	};

	// ── 헤더 로그 ──────────────────────────────────────────────
	L(TEXT("[Distributor] ═══════════════════════════════════════════"));
	L(TEXT("[Distributor] 담당자 배정 분석 시작"));
	L(FString::Printf(TEXT("[Distributor]   생성 시각: %s"), *FDateTime::Now().ToString()));
	L(FString::Printf(TEXT("[Distributor]   총 에셋: %d개 | 작업자: %d명 | 섬: %d개"),
		Entries.Num(), MaxWorkers, Components.Num()));
	L(TEXT("[Distributor] ───────────────────────────────────────────"));

	// ── 섬별 배정 ──────────────────────────────────────────────
	for (int32 IslandIdx = 0; IslandIdx < Components.Num(); ++IslandIdx)
	{
		const TSet<FName>& Component = Components[IslandIdx];

		// 순환 의존성 포함 여부 및 대표 경로 수집
		bool bHasCyclic = false;
		TArray<FString> AssetNames;
		for (const FName& Pkg : Component)
		{
			if (const FAssetMoverEntry* const* FoundEntry = PkgToEntry.Find(Pkg))
			{
				if ((*FoundEntry)->bHasCyclicDep)
				{
					bHasCyclic = true;
				}
				AssetNames.Add((*FoundEntry)->AssetData.AssetName.ToString());
			}
		}
		AssetNames.Sort();

		// 현재 부하가 가장 적은 작업자 선택
		int32 MinLoad      = TNumericLimits<int32>::Max();
		int32 TargetWorker = 0;
		for (int32 w = 0; w < MaxWorkers; ++w)
		{
			if (WorkerLoad[w] < MinLoad)
			{
				MinLoad      = WorkerLoad[w];
				TargetWorker = w;
			}
		}

		// 배정 전 부하 기록 (로그용)
		const int32 LoadBefore = WorkerLoad[TargetWorker];

		WorkerLoad[TargetWorker]        += Component.Num();
		WorkerIslandCount[TargetWorker] += 1;

		for (const FName& Pkg : Component)
		{
			PkgToAssignee.Add(Pkg, TargetWorker);
		}

		// 섬 요약 로그
		L(FString::Printf(TEXT("[Distributor] 섬 #%d  크기=%d%s"),
			IslandIdx + 1,
			Component.Num(),
			bHasCyclic ? TEXT("  ⚠ 순환의존성 포함") : TEXT("")));

		// 배정 이유: 배정 전 작업자별 부하 나열
		{
			FString LoadStr;
			for (int32 w = 0; w < MaxWorkers; ++w)
			{
				const int32 DisplayLoad = (w == TargetWorker) ? LoadBefore : WorkerLoad[w];
				LoadStr += FString::Printf(TEXT("  작업자%d:%d"), w + 1, DisplayLoad);
			}
			L(FString::Printf(
				TEXT("[Distributor]   배정 이유:%s  → 작업자%d 선택 (부하 최소: %d → %d)"),
				*LoadStr, TargetWorker + 1, LoadBefore, WorkerLoad[TargetWorker]));
		}

		// 에셋 목록 (파일에는 항상, UE_LOG는 Verbose)
		for (const FString& Name : AssetNames)
		{
			LV(FString::Printf(TEXT("[Distributor]     • %s"), *Name));
		}
	}

	// ── Entry에 AssigneeIndex 반영 ──────────────────────────────
	for (TSharedPtr<FAssetMoverEntry>& Entry : Entries)
	{
		if (!Entry.IsValid()) continue;
		const int32* Assigned = PkgToAssignee.Find(Entry->AssetData.PackageName);
		Entry->AssigneeIndex  = Assigned ? *Assigned : 0;
	}

	// ── 최종 요약 로그 ─────────────────────────────────────────
	L(TEXT("[Distributor] ───────────────────────────────────────────"));
	L(TEXT("[Distributor] 최종 배정 결과"));

	for (int32 w = 0; w < MaxWorkers; ++w)
	{
		L(FString::Printf(TEXT("[Distributor]   작업자 %d: %d개 에셋, %d개 섬"),
			w + 1, WorkerLoad[w], WorkerIslandCount[w]));
	}

	// 부하 불균형 경고 (최대-최소 > 평균의 20%)
	const int32 MaxLoad  = *Algo::MaxElement(WorkerLoad);
	const int32 MinLoad2 = *Algo::MinElement(WorkerLoad);
	const float Avg      = static_cast<float>(Entries.Num()) / MaxWorkers;
	if (Avg > 0.f && (MaxLoad - MinLoad2) > Avg * 0.2f)
	{
		LW(FString::Printf(
			TEXT("[Distributor]   ⚠ 부하 불균형 감지: 최대=%d, 최소=%d, 평균=%.1f")
			TEXT(" (섬 크기 차이로 인한 자연스러운 편차일 수 있음)"),
			MaxLoad, MinLoad2, Avg));
	}

	L(TEXT("[Distributor] ═══════════════════════════════════════════"));

	// ── 파일 저장 ──────────────────────────────────────────────
	const FString FileContent = FString::Join(FileLines, TEXT("\n"));
	if (FFileHelper::SaveStringToFile(
		FileContent, *LogFile,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogSafeAssetMover, Log,
			TEXT("[Distributor] 배정 로그 저장 완료: %s"), *LogFile);
	}
	else
	{
		UE_LOG(LogSafeAssetMover, Warning,
			TEXT("[Distributor] 배정 로그 파일 저장 실패: %s"), *LogFile);
	}
}
