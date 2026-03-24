// Copyright FB Team. All Rights Reserved.

#include "AssetMoveScheduler.h"
#include "AssetDependencyAnalyzer.h"

void FAssetMoveScheduler::ComputeMoveOrder(
	TArray<TSharedPtr<FAssetMoverEntry>>& InOutEntries,
	const FDependencyGraph& Graph)
{
	if (InOutEntries.Num() == 0) return;

	// 패키지명 -> 엔트리 인덱스 맵
	TMap<FName, int32> PkgToIndex;
	for (int32 i = 0; i < InOutEntries.Num(); ++i)
	{
		PkgToIndex.Add(InOutEntries[i]->AssetData.PackageName, i);
	}

	// InDegree 계산: 이 에셋이 폴더 내 다른 에셋에 의존하는 수
	TMap<FName, int32> InDegree;
	for (const TSharedPtr<FAssetMoverEntry>& Entry : InOutEntries)
	{
		const FName PkgName = Entry->AssetData.PackageName;
		InDegree.FindOrAdd(PkgName) = 0;
	}

	for (const TSharedPtr<FAssetMoverEntry>& Entry : InOutEntries)
	{
		const FName PkgName = Entry->AssetData.PackageName;
		if (const TSet<FName>* InternalDeps = Graph.InternalDeps.Find(PkgName))
		{
			for (const FName& Dep : *InternalDeps)
			{
				if (InDegree.Contains(Dep))
				{
					InDegree[PkgName]++;
				}
			}
		}
	}

	// Kahn's Algorithm (BFS 토폴로지 정렬)
	TQueue<FName> Queue;
	for (const auto& Pair : InDegree)
	{
		if (Pair.Value == 0)
		{
			Queue.Enqueue(Pair.Key);
		}
	}

	int32 OrderCounter = 0;
	TArray<FName> Sorted;

	while (!Queue.IsEmpty())
	{
		FName Current;
		Queue.Dequeue(Current);
		Sorted.Add(Current);

		int32* EntryIdx = PkgToIndex.Find(Current);
		if (EntryIdx && InOutEntries.IsValidIndex(*EntryIdx))
		{
			InOutEntries[*EntryIdx]->MoveOrder = OrderCounter++;
		}

		// 이 에셋을 참조하는 폴더 내 에셋의 InDegree 감소
		if (const TSet<FName>* Refs = Graph.Referencers.Find(Current))
		{
			for (const FName& Ref : *Refs)
			{
				if (int32* Deg = InDegree.Find(Ref))
				{
					(*Deg)--;
					if (*Deg == 0)
					{
						Queue.Enqueue(Ref);
					}
				}
			}
		}
	}

	// 순환 의존성이 있는 에셋 처리 (정렬에 포함되지 않은 나머지)
	if (Sorted.Num() < InOutEntries.Num())
	{
		UE_LOG(LogSafeAssetMover, Warning,
			TEXT("Cyclic dependencies detected. %d assets will be moved last."),
			InOutEntries.Num() - Sorted.Num());

		for (TSharedPtr<FAssetMoverEntry>& Entry : InOutEntries)
		{
			const FName PkgName = Entry->AssetData.PackageName;
			if (!Sorted.Contains(PkgName))
			{
				Entry->MoveOrder = OrderCounter++;
				Entry->bHasCyclicDep = true;
			}
		}
	}
}

void FAssetMoveScheduler::SortByMoveOrder(TArray<TSharedPtr<FAssetMoverEntry>>& InOutEntries)
{
	InOutEntries.Sort([](const TSharedPtr<FAssetMoverEntry>& A, const TSharedPtr<FAssetMoverEntry>& B)
	{
		return A->MoveOrder < B->MoveOrder;
	});
}
