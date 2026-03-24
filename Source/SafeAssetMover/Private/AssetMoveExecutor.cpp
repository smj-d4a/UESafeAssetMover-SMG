// Copyright FB Team. All Rights Reserved.

#include "AssetMoveExecutor.h"
#include "AssetDependencyAnalyzer.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/ScopedSlowTask.h"
#include "UObject/ObjectRedirector.h"

#define LOCTEXT_NAMESPACE "FAssetMoveExecutor"

int32 FAssetMoveExecutor::MoveAssets(
	TArray<TSharedPtr<FAssetMoverEntry>>& Entries,
	const FString& TargetFolder)
{
	bCancelRequested = false;
	CreatedRedirectorPaths_Internal.Empty();

	// 선택된 항목만 필터링, MoveOrder 순 정렬
	TArray<TSharedPtr<FAssetMoverEntry>> ToMove;
	for (TSharedPtr<FAssetMoverEntry>& Entry : Entries)
	{
		if (Entry->bSelected && Entry->Status == EMoveStatus::Pending)
		{
			ToMove.Add(Entry);
		}
	}
	ToMove.Sort([](const TSharedPtr<FAssetMoverEntry>& A, const TSharedPtr<FAssetMoverEntry>& B)
	{
		return A->MoveOrder < B->MoveOrder;
	});

	if (ToMove.Num() == 0)
	{
		UE_LOG(LogSafeAssetMover, Warning, TEXT("No assets selected for move."));
		return 0;
	}

	int32 MovedCount = 0;

	FScopedSlowTask SlowTask(
		static_cast<float>(ToMove.Num()),
		LOCTEXT("MovingAssets", "에셋 이동 중...")
	);
	SlowTask.MakeDialog(/*bShowCancelButton=*/true);

	for (TSharedPtr<FAssetMoverEntry>& Entry : ToMove)
	{
		if (SlowTask.ShouldCancel() || bCancelRequested)
		{
			UE_LOG(LogSafeAssetMover, Log, TEXT("Move cancelled by user."));
			break;
		}

		SlowTask.EnterProgressFrame(
			1.f,
			FText::Format(
				LOCTEXT("MovingAsset", "이동 중: {0}"),
				FText::FromName(Entry->AssetData.AssetName)
			)
		);

		Entry->Status = EMoveStatus::InProgress;

		bool bSuccess = MoveSingleAsset(*Entry, TargetFolder);
		Entry->Status = bSuccess ? EMoveStatus::Moved : EMoveStatus::Failed;

		OnAssetMoved.Broadcast(*Entry, bSuccess);

		if (bSuccess)
		{
			++MovedCount;
		}

		UE_LOG(LogSafeAssetMover, Log, TEXT("%s: %s -> %s"),
			bSuccess ? TEXT("Moved") : TEXT("Failed"),
			*Entry->SourcePath,
			*Entry->TargetPath
		);
	}

	OnMoveCompleted.Broadcast(MovedCount);
	return MovedCount;
}

bool FAssetMoveExecutor::MoveSingleAsset(
	FAssetMoverEntry& Entry,
	const FString& TargetFolder)
{
	UObject* Asset = Entry.AssetData.GetAsset();
	if (!Asset)
	{
		UE_LOG(LogSafeAssetMover, Warning,
			TEXT("Could not load asset: %s"), *Entry.AssetData.GetObjectPathString());
		return false;
	}

	IAssetTools& AssetTools =
		FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

	const FString AssetName = Entry.AssetData.AssetName.ToString();
	Entry.TargetPath = TargetFolder / AssetName;

	TArray<FAssetRenameData> RenameData;
	RenameData.Add(FAssetRenameData(Asset, TargetFolder, AssetName));

	return AssetTools.RenameAssets(RenameData);
}

void FAssetMoveExecutor::FixUpRedirectors(const TArray<FString>& RedirectorPaths)
{
	if (RedirectorPaths.Num() == 0) return;

	IAssetRegistry& Registry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	IAssetTools& AssetTools =
		FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

	TArray<UObjectRedirector*> Redirectors;

	for (const FString& Path : RedirectorPaths)
	{
		FAssetData AssetData = Registry.GetAssetByObjectPath(FSoftObjectPath(Path));
		if (AssetData.IsValid())
		{
			UObjectRedirector* Redirector =
				Cast<UObjectRedirector>(AssetData.GetAsset());
			if (Redirector)
			{
				Redirectors.Add(Redirector);
			}
		}
	}

	if (Redirectors.Num() > 0)
	{
		AssetTools.FixupReferencers(Redirectors, /*bCheckoutDialogPrompt=*/true);
		UE_LOG(LogSafeAssetMover, Log,
			TEXT("Fixed up %d redirectors."), Redirectors.Num());
	}
}

void FAssetMoveExecutor::CollectRedirectors(
	const FString& FolderPath,
	TArray<FString>& OutPaths)
{
	IAssetRegistry& Registry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(FName(*FolderPath), Assets, /*bRecursive=*/true);

	for (const FAssetData& AD : Assets)
	{
		if (AD.IsRedirector())
		{
			OutPaths.Add(AD.GetObjectPathString());
		}
	}
}

#undef LOCTEXT_NAMESPACE
