// Copyright FB Team. All Rights Reserved.

#include "AssetMoveExecutor.h"
#include "AssetDependencyAnalyzer.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/ScopedSlowTask.h"
#include "UObject/ObjectRedirector.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "ISourceControlState.h"
#include "SourceControlOperations.h"
#include "Misc/PackageName.h"

#define LOCTEXT_NAMESPACE "FAssetMoveExecutor"

int32 FAssetMoveExecutor::MoveAssets(
	TArray<TSharedPtr<FAssetMoverEntry>>& Entries,
	const FString& TargetFolder)
{
	bCancelRequested = false;
	LastSessionUndoRecords.Empty();

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

		bool bSuccess = MoveSingleAsset(*Entry);

		if (Entry->Status == EMoveStatus::InProgress)
		{
			Entry->Status = bSuccess ? EMoveStatus::Moved : EMoveStatus::Failed;
		}

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

bool FAssetMoveExecutor::MoveSingleAsset(FAssetMoverEntry& Entry)
{
	ISourceControlModule& SCModule = ISourceControlModule::Get();
	if (SCModule.IsEnabled())
	{
		ISourceControlProvider& Provider = SCModule.GetProvider();

		FString PackageFilename;
		if (FPackageName::TryConvertLongPackageNameToFilename(
			Entry.AssetData.PackageName.ToString(),
			PackageFilename,
			FPackageName::GetAssetPackageExtension()))
		{
			FSourceControlStatePtr State = Provider.GetState(
				PackageFilename, EStateCacheUsage::ForceUpdate);

			if (State.IsValid())
			{
				// 타인 잠금 확인
				FString LockedBy;
				if (State->IsCheckedOutOther(&LockedBy))
				{
					Entry.CheckedOutBy = LockedBy;
					Entry.Status = EMoveStatus::LockedByOther;
					UE_LOG(LogSafeAssetMover, Warning,
						TEXT("[SafeAssetMover] '%s' 이동 불가 — P4에서 '%s'이(가) Checkout 중입니다. "
							 "해당 사용자에게 파일 잠금 해제를 요청하세요."),
						*Entry.AssetData.AssetName.ToString(),
						*LockedBy);
					return false;
				}

				// Checkout 시도
				if (State->IsSourceControlled() && !State->IsCheckedOut() && State->CanCheckout())
				{
					TArray<FString> FilesToCheckOut = { PackageFilename };
					const ECommandResult::Type CheckOutResult = Provider.Execute(
						ISourceControlOperation::Create<FCheckOut>(),
						FilesToCheckOut
					);

					if (CheckOutResult == ECommandResult::Succeeded)
					{
						UE_LOG(LogSafeAssetMover, Log,
							TEXT("[SafeAssetMover] '%s' P4 Checkout 완료."),
							*Entry.AssetData.AssetName.ToString());
					}
					else
					{
						UE_LOG(LogSafeAssetMover, Warning,
							TEXT("[SafeAssetMover] '%s' P4 Checkout 실패. Move를 계속 시도합니다."),
							*Entry.AssetData.AssetName.ToString());
					}
				}
			}
		}
	}

	UObject* Asset = Entry.AssetData.GetAsset();
	if (!Asset)
	{
		UE_LOG(LogSafeAssetMover, Warning,
			TEXT("Could not load asset: %s"), *Entry.AssetData.GetObjectPathString());
		return false;
	}

	IAssetTools& AssetTools =
		FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

	const FString TargetPackageFolder = FPaths::GetPath(Entry.TargetPath);
	const FString AssetName = Entry.AssetData.AssetName.ToString();

	TArray<FAssetRenameData> RenameData;
	RenameData.Add(FAssetRenameData(Asset, TargetPackageFolder, AssetName));

	const bool bMoved = AssetTools.RenameAssets(RenameData);
	if (bMoved)
	{
		FAssetMoveUndoRecord UndoRec;
		UndoRec.OriginalPackagePath = Entry.AssetData.PackageName.ToString();
		UndoRec.MovedPackagePath    = Entry.TargetPath;

		FPackageName::TryConvertLongPackageNameToFilename(
			UndoRec.OriginalPackagePath,
			UndoRec.OriginalPackageFilename,
			FPackageName::GetAssetPackageExtension());

		FPackageName::TryConvertLongPackageNameToFilename(
			UndoRec.MovedPackagePath,
			UndoRec.MovedPackageFilename,
			FPackageName::GetAssetPackageExtension());

		LastSessionUndoRecords.Add(UndoRec);
	}
	return bMoved;
}

int32 FAssetMoveExecutor::UndoLastMoveSession()
{
	if (LastSessionUndoRecords.Num() == 0)
	{
		UE_LOG(LogSafeAssetMover, Warning, TEXT("[Undo] 복원할 이동 기록이 없습니다."));
		return 0;
	}

	IAssetTools& AssetTools =
		FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
	IAssetRegistry& Registry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	ISourceControlModule& SCModule = ISourceControlModule::Get();

	const int32 TotalRecords = LastSessionUndoRecords.Num();
	int32 RestoredCount = 0;
	TArray<FString> AllFilesToRevert;

	FScopedSlowTask SlowTask(
		static_cast<float>(TotalRecords),
		LOCTEXT("UndoMove", "이동 취소 중...")
	);
	SlowTask.MakeDialog(/*bShowCancelButton=*/false);

	for (int32 Idx = TotalRecords - 1; Idx >= 0; --Idx)
	{
		const FAssetMoveUndoRecord& Rec = LastSessionUndoRecords[Idx];

		SlowTask.EnterProgressFrame(
			1.f,
			FText::Format(
				LOCTEXT("UndoMoving", "복원 중: {0}"),
				FText::FromString(FPaths::GetBaseFilename(Rec.OriginalPackagePath))
			)
		);

		TArray<FAssetData> MovedAssets;
		Registry.GetAssetsByPackageName(FName(*Rec.MovedPackagePath), MovedAssets);
		UObject* Asset = MovedAssets.Num() > 0 ? MovedAssets[0].GetAsset() : nullptr;

		if (!Asset)
		{
			UE_LOG(LogSafeAssetMover, Warning,
				TEXT("[Undo] 에셋 로드 실패: %s"), *Rec.MovedPackagePath);
			continue;
		}

		// 이동된 파일 P4 Checkout
		if (SCModule.IsEnabled() && !Rec.MovedPackageFilename.IsEmpty())
		{
			ISourceControlProvider& Provider = SCModule.GetProvider();
			FSourceControlStatePtr State =
				Provider.GetState(Rec.MovedPackageFilename, EStateCacheUsage::ForceUpdate);
			if (State.IsValid() && State->IsSourceControlled()
				&& !State->IsCheckedOut() && State->CanCheckout())
			{
				TArray<FString> ToCheckout = { Rec.MovedPackageFilename };
				Provider.Execute(ISourceControlOperation::Create<FCheckOut>(), ToCheckout);
			}
		}

		const FString OriginalFolder = FPaths::GetPath(Rec.OriginalPackagePath);
		const FString AssetName      = FPaths::GetBaseFilename(Rec.OriginalPackagePath);

		TArray<FAssetRenameData> RenameData;
		RenameData.Add(FAssetRenameData(Asset, OriginalFolder, AssetName));

		if (AssetTools.RenameAssets(RenameData))
		{
			++RestoredCount;
			if (!Rec.OriginalPackageFilename.IsEmpty())
				AllFilesToRevert.AddUnique(Rec.OriginalPackageFilename);
			if (!Rec.MovedPackageFilename.IsEmpty())
				AllFilesToRevert.AddUnique(Rec.MovedPackageFilename);

			UE_LOG(LogSafeAssetMover, Log,
				TEXT("[Undo] 복원 완료: %s <- %s"),
				*Rec.OriginalPackagePath, *Rec.MovedPackagePath);
		}
		else
		{
			UE_LOG(LogSafeAssetMover, Warning,
				TEXT("[Undo] 복원 실패: %s <- %s"),
				*Rec.OriginalPackagePath, *Rec.MovedPackagePath);
		}
	}

	if (AllFilesToRevert.Num() > 0)
	{
		RevertP4PendingChanges(AllFilesToRevert);
	}

	LastSessionUndoRecords.Empty();
	OnUndoCompleted.Broadcast(RestoredCount);

	UE_LOG(LogSafeAssetMover, Log,
		TEXT("[Undo] 세션 복원 완료: %d / %d 에셋 복원됨"), RestoredCount, TotalRecords);

	return RestoredCount;
}

void FAssetMoveExecutor::RevertP4PendingChanges(const TArray<FString>& Filenames)
{
	ISourceControlModule& SCModule = ISourceControlModule::Get();
	if (!SCModule.IsEnabled()) return;

	ISourceControlProvider& Provider = SCModule.GetProvider();

	TArray<FString> FilesToRevert;
	for (const FString& Filename : Filenames)
	{
		if (Filename.IsEmpty()) continue;
		FSourceControlStatePtr State =
			Provider.GetState(Filename, EStateCacheUsage::ForceUpdate);
		if (State.IsValid()
			&& (State->IsCheckedOut() || State->IsAdded() || State->IsDeleted()))
		{
			FilesToRevert.Add(Filename);
		}
	}

	if (FilesToRevert.Num() == 0) return;

	const ECommandResult::Type Result = Provider.Execute(
		ISourceControlOperation::Create<FRevert>(),
		FilesToRevert
	);

	if (Result == ECommandResult::Succeeded)
	{
		UE_LOG(LogSafeAssetMover, Log,
			TEXT("[Undo] P4 Revert 완료: %d 파일 원복됨"), FilesToRevert.Num());
	}
	else
	{
		UE_LOG(LogSafeAssetMover, Warning,
			TEXT("[Undo] P4 Revert 일부 실패 (수동 확인 필요)"));
	}
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
			UObjectRedirector* Redirector = Cast<UObjectRedirector>(AssetData.GetAsset());
			if (Redirector)
			{
				Redirectors.Add(Redirector);
			}
		}
	}

	if (Redirectors.Num() > 0)
	{
		AssetTools.FixupReferencers(Redirectors, /*bCheckoutDialogPrompt=*/true);
		UE_LOG(LogSafeAssetMover, Log, TEXT("Fixed up %d redirectors."), Redirectors.Num());
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
