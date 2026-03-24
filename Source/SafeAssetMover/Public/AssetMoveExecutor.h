// Copyright FB Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetDependencyAnalyzer.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAssetMoved, const FAssetMoverEntry& /*Entry*/, bool /*bSuccess*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMoveCompleted, int32 /*MovedCount*/);

/**
 * 에셋 이동 및 리다이렉터 처리를 담당합니다.
 *
 * - IAssetTools::RenameAssets()를 사용하여 이동 (리다이렉터 자동 생성)
 * - FScopedSlowTask로 진행 표시 및 취소 지원
 * - FixupReferencers()로 리다이렉터 제거
 */
class SAFEASSETMOVER_API FAssetMoveExecutor
{
public:
	/** 이동 완료 시 호출 (에셋별) */
	FOnAssetMoved OnAssetMoved;

	/** 전체 배치 완료 시 호출 */
	FOnMoveCompleted OnMoveCompleted;

	/**
	 * 선택된 에셋을 MoveOrder 순서로 이동합니다.
	 * @param Entries      이동할 에셋 목록 (bSelected == true 인 항목만 처리)
	 * @param TargetFolder 대상 폴더 경로 (예: /Game/NewFolder)
	 * @return 성공적으로 이동된 에셋 수
	 */
	int32 MoveAssets(
		TArray<TSharedPtr<FAssetMoverEntry>>& Entries,
		const FString& TargetFolder
	);

	/**
	 * 이전 이동으로 생성된 리다이렉터를 수정합니다.
	 * @param RedirectorPaths 처리할 리다이렉터 패키지 경로 목록
	 */
	void FixUpRedirectors(const TArray<FString>& RedirectorPaths);

	/**
	 * 폴더 내 모든 리다이렉터를 수집합니다.
	 * @param FolderPath 검색할 폴더 경로
	 * @param OutPaths   발견된 리다이렉터 경로 목록
	 */
	static void CollectRedirectors(const FString& FolderPath, TArray<FString>& OutPaths);

	/** 진행 중인 이동 작업 취소 요청 */
	void RequestCancel() { bCancelRequested = true; }

	/** 현재 취소 요청 여부 */
	bool IsCancelRequested() const { return bCancelRequested; }

private:
	bool bCancelRequested = false;
	TArray<FString> CreatedRedirectorPaths_Internal;

	bool MoveSingleAsset(
		FAssetMoverEntry& Entry,
		const FString& TargetFolder
	);
};
