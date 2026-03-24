// Copyright FB Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSafeAssetMover, Log, All);

/** 이동할 에셋의 상태 */
enum class EMoveStatus : uint8
{
	Pending,
	InProgress,
	Moved,
	Failed,
	Skipped
};

/** 에셋 목록 테이블의 한 행 데이터 */
struct FAssetMoverEntry
{
	FAssetData AssetData;

	/** 소스 경로 (예: /Game/OldFolder/MyTexture) */
	FString SourcePath;

	/** 대상 경로 (예: /Game/NewFolder/MyTexture) */
	FString TargetPath;

	/** 이 에셋을 참조하는 에셋 수 (AssetRegistry 기준) */
	int32 ReferenceCount = 0;

	/** 이 에셋이 의존하는 에셋 수 (AssetRegistry 기준) */
	int32 DependencyCount = 0;

	/** 토폴로지 정렬 결과 이동 순서 (낮을수록 먼저) */
	int32 MoveOrder = 0;

	/** 현재 처리 상태 */
	EMoveStatus Status = EMoveStatus::Pending;

	/** UI 체크박스 선택 여부 */
	bool bSelected = true;

	/** 순환 의존성 경고 여부 */
	bool bHasCyclicDep = false;
};

/** 의존성 그래프 */
struct FDependencyGraph
{
	/** Key: 패키지명, Value: 이 에셋이 의존하는 패키지 집합 */
	TMap<FName, TSet<FName>> Dependencies;

	/** Key: 패키지명, Value: 이 에셋을 참조하는 패키지 집합 */
	TMap<FName, TSet<FName>> Referencers;

	/** 폴더 내부 에셋 간 의존성만 필터링 (이동 순서 결정용) */
	TMap<FName, TSet<FName>> InternalDeps;
};

/**
 * AssetRegistry를 사용하여 폴더 내 에셋의 레퍼런스/디펜던시를 분석합니다.
 */
class SAFEASSETMOVER_API FAssetDependencyAnalyzer
{
public:
	/**
	 * 소스 폴더를 스캔하여 에셋 목록과 의존성 그래프를 구성합니다.
	 * @param SourceFolder   스캔할 콘텐츠 폴더 경로 (예: /Game/OldFolder)
	 * @param OutEntries     분석 결과 에셋 목록
	 * @param OutGraph       의존성 그래프
	 */
	static void AnalyzeFolder(
		const FString& SourceFolder,
		TArray<TSharedPtr<FAssetMoverEntry>>& OutEntries,
		FDependencyGraph& OutGraph
	);

	/**
	 * 단일 에셋의 레퍼런스/디펜던시 카운트를 계산합니다.
	 */
	static void ComputeCounts(
		const FName& PackageName,
		int32& OutRefCount,
		int32& OutDepCount
	);

private:
	static void BuildGraph(
		const TArray<FAssetData>& Assets,
		FDependencyGraph& OutGraph
	);
};
