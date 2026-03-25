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

	Registry.SearchAllAssets(true);

	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(FName(*SourceFolder), Assets, /*bRecursive=*/true);

	if (Assets.Num() == 0)
	{
		UE_LOG(LogSafeAssetMover, Warning, TEXT("No assets found in folder: %s"), *SourceFolder);
		return;
	}

	BuildGraph(Assets, OutGraph);

	for (const FAssetData& AssetData : Assets)
	{
		if (AssetData.IsRedirector()) continue;

		TSharedPtr<FAssetMoverEntry> Entry = MakeShared<FAssetMoverEntry>();
		Entry->AssetData = AssetData;
		Entry->SourcePath = AssetData.GetObjectPathString();

		const FName PkgName = AssetData.PackageName;

		if (const TSet<FName>* Refs = OutGraph.Referencers.Find(PkgName))
		{
			Entry->ReferenceCount = Refs->Num();
		}
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

	TArray<FName> Deps;
	Registry.GetDependencies(PackageName, Deps, UE::AssetRegistry::EDependencyCategory::Package);
	OutDepCount = Deps.Num();

	TArray<FName> Refs;
	Registry.GetReferencers(PackageName, Refs, UE::AssetRegistry::EDependencyCategory::Package);
	OutRefCount = Refs.Num();
}

void FAssetDependencyAnalyzer::BuildGraph(
	const TArray<FAssetData>& Assets,
	FDependencyGraph& OutGraph)
{
	IAssetRegistry& Registry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	// 폴더 내 패키지 집합
	TSet<FName> FolderPackages;
	for (const FAssetData& AD : Assets)
	{
		FolderPackages.Add(AD.PackageName);
	}

	for (const FAssetData& AD : Assets)
	{
		const FName PkgName = AD.PackageName;

		// 의존성 수집 (N 쿼리)
		TArray<FName> Deps;
		Registry.GetDependencies(PkgName, Deps, UE::AssetRegistry::EDependencyCategory::Package);

		OutGraph.Dependencies.FindOrAdd(PkgName);
		for (const FName& Dep : Deps)
		{
			OutGraph.Dependencies[PkgName].Add(Dep);

			// 내부 의존성
			if (FolderPackages.Contains(Dep))
			{
				OutGraph.InternalDeps.FindOrAdd(PkgName).Add(Dep);
			}

			// Referencers 역방향 파생 (별도 GetReferencers 쿼리 없이 N→2N 절감)
			OutGraph.Referencers.FindOrAdd(Dep).Add(PkgName);
		}

		// 키 초기화 보장
		OutGraph.Referencers.FindOrAdd(PkgName);
	}
}
