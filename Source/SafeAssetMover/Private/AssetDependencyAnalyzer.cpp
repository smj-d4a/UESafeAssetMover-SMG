// Copyright FB Team. All Rights Reserved.

#include "AssetDependencyAnalyzer.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

DEFINE_LOG_CATEGORY(LogSafeAssetMover);

void FAssetDependencyAnalyzer::AnalyzeFolder(
	const FString& SourceFolder,
	TArray<TSharedPtr<FAssetMoverEntry>>& OutEntries,
	FDependencyGraph& OutGraph)
{
	OutEntries.Empty();
	OutGraph = FDependencyGraph();

	IAssetRegistry& Registry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	// 에셋 레지스트리 검색 완료까지 대기
	Registry.SearchAllAssets(true);

	// 폴더 내 에셋 전체 수집 (하위 폴더 포함)
	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(FName(*SourceFolder), Assets, /*bRecursive=*/true);

	if (Assets.Num() == 0)
	{
		UE_LOG(LogSafeAssetMover, Warning, TEXT("No assets found in folder: %s"), *SourceFolder);
		return;
	}

	// 의존성 그래프 구성
	BuildGraph(Assets, OutGraph);

	// 엔트리 생성
	for (const FAssetData& AssetData : Assets)
	{
		// 리다이렉터는 목록에서 제외 (별도 처리)
		if (AssetData.IsRedirector()) continue;

		TSharedPtr<FAssetMoverEntry> Entry = MakeShared<FAssetMoverEntry>();
		Entry->AssetData = AssetData;
		Entry->SourcePath = AssetData.GetObjectPathString();

		const FName PkgName = AssetData.PackageName;

		// 레퍼런스 카운트 (이 에셋을 참조하는 수)
		if (const TSet<FName>* Refs = OutGraph.Referencers.Find(PkgName))
		{
			Entry->ReferenceCount = Refs->Num();
		}

		// 디펜던시 카운트 (이 에셋이 의존하는 수)
		if (const TSet<FName>* Deps = OutGraph.Dependencies.Find(PkgName))
		{
			Entry->DependencyCount = Deps->Num();
		}

		OutEntries.Add(Entry);
	}

	UE_LOG(LogSafeAssetMover, Log, TEXT("Analyzed %d assets in folder: %s"),
		OutEntries.Num(), *SourceFolder);
}

void FAssetDependencyAnalyzer::ComputeCounts(
	const FName& PackageName,
	int32& OutRefCount,
	int32& OutDepCount)
{
	IAssetRegistry& Registry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	UE::AssetRegistry::FDependencyQuery Query;
	Query.Categories = UE::AssetRegistry::EDependencyCategory::Package;

	TArray<FName> Deps;
	Registry.GetDependencies(PackageName, Deps, Query);
	OutDepCount = Deps.Num();

	TArray<FName> Refs;
	Registry.GetReferencers(PackageName, Refs, Query);
	OutRefCount = Refs.Num();
}

void FAssetDependencyAnalyzer::BuildGraph(
	const TArray<FAssetData>& Assets,
	FDependencyGraph& OutGraph)
{
	IAssetRegistry& Registry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	UE::AssetRegistry::FDependencyQuery Query;
	Query.Categories = UE::AssetRegistry::EDependencyCategory::Package;

	// 폴더 내 패키지 집합
	TSet<FName> FolderPackages;
	for (const FAssetData& AD : Assets)
	{
		FolderPackages.Add(AD.PackageName);
	}

	for (const FAssetData& AD : Assets)
	{
		const FName PkgName = AD.PackageName;

		// 의존성 수집
		TArray<FName> Deps;
		Registry.GetDependencies(PkgName, Deps, Query);
		OutGraph.Dependencies.FindOrAdd(PkgName);
		for (const FName& Dep : Deps)
		{
			OutGraph.Dependencies[PkgName].Add(Dep);

			// 내부 의존성 (폴더 내 에셋끼리만)
			if (FolderPackages.Contains(Dep))
			{
				OutGraph.InternalDeps.FindOrAdd(PkgName).Add(Dep);
			}
		}

		// 레퍼런서 수집
		TArray<FName> Refs;
		Registry.GetReferencers(PkgName, Refs, Query);
		OutGraph.Referencers.FindOrAdd(PkgName);
		for (const FName& Ref : Refs)
		{
			OutGraph.Referencers[PkgName].Add(Ref);
		}
	}
}
