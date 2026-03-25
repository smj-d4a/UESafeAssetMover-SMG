// Copyright FB Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetDependencyAnalyzer.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAssetMoved, const FAssetMoverEntry& /*Entry*/, bool /*bSuccess*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMoveCompleted, int32 /*MovedCount*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnUndoCompleted, int32 /*RestoredCount*/);

/** 단일 에셋 이동의 Undo 데이터 */
struct FAssetMoveUndoRecord
{
	/** 이동 전 패키지 경로 (예: /Game/OldFolder/MyAsset) */
	FString OriginalPackagePath;

	/** 이동 후 패키지 경로 (예: /Game/NewFolder/MyAsset) */
	FString MovedPackagePath;

	/** 이동 전 디스크 파일 경로 (.uasset, P4 revert 용) */
	FString OriginalPackageFilename;

	/** 이동 후 디스크 파일 경로 (.uasset, P4 revert 용) */
	FString MovedPackageFilename;
};

/**
 * 에셋 이동 및 리다이렉터 처리를 담당합니다.
 *
 * - IAssetTools::RenameAssets()를 사용하여 이동 (리다이렉터 자동 생성)
 * - FScopedSlowTask로 진행 표시 및 취소 지원
 * - FixupReferencers()로 리다이렉터 제거
 * - P4 Checkout/LockedByOther 감지 지원
 * - 마지막 이동 세션 Undo 지원
 */
class SAFEASSETMOVER_API FAssetMoveExecutor
{
public:
	FOnAssetMoved    OnAssetMoved;
	FOnMoveCompleted OnMoveCompleted;
	FOnUndoCompleted OnUndoCompleted;

	int32 MoveAssets(
		TArray<TSharedPtr<FAssetMoverEntry>>& Entries,
		const FString& TargetFolder
	);

	void FixUpRedirectors(const TArray<FString>& RedirectorPaths);

	static void CollectRedirectors(const FString& FolderPath, TArray<FString>& OutPaths);

	void RequestCancel() { bCancelRequested = true; }
	bool IsCancelRequested() const { return bCancelRequested; }

	/**
	 * 마지막 이동 세션을 되돌립니다.
	 * 이동된 에셋을 원래 경로로 복원하고 P4 Pending 변경을 Revert합니다.
	 */
	int32 UndoLastMoveSession();

	bool CanUndo() const { return LastSessionUndoRecords.Num() > 0; }

private:
	bool bCancelRequested = false;

	TArray<FAssetMoveUndoRecord> LastSessionUndoRecords;

	bool MoveSingleAsset(FAssetMoverEntry& Entry);

	void RevertP4PendingChanges(const TArray<FString>& Filenames);
};
