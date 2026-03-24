// Copyright FB Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetDependencyAnalyzer.h"

/**
 * Kahn's Algorithm (BFS 토폴로지 정렬)을 사용하여
 * 의존성이 가장 적은 에셋(리프)부터 이동 순서를 결정합니다.
 *
 * 순환 의존성이 있는 에셋은 경고 플래그를 설정하고 후순위에 배정합니다.
 */
class SAFEASSETMOVER_API FAssetMoveScheduler
{
public:
	/**
	 * 에셋 목록에 MoveOrder를 할당합니다.
	 * @param InOutEntries  MoveOrder가 채워질 에셋 목록 (In/Out)
	 * @param Graph         의존성 그래프
	 */
	static void ComputeMoveOrder(
		TArray<TSharedPtr<FAssetMoverEntry>>& InOutEntries,
		const FDependencyGraph& Graph
	);

	/**
	 * MoveOrder 기준으로 오름차순 정렬합니다 (낮은 값 = 먼저 이동).
	 */
	static void SortByMoveOrder(TArray<TSharedPtr<FAssetMoverEntry>>& InOutEntries);
};
