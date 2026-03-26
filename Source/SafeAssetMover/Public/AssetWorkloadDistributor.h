// Copyright FB Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetDependencyAnalyzer.h"

/**
 * 연결된 컴포넌트(섬) 기반 작업량 균등 배분기
 *
 * 알고리즘:
 * 1. InternalDeps 그래프를 무방향으로 취급하여 Union-Find로 섬을 구합니다.
 * 2. 섬을 크기 내림차순으로 정렬합니다.
 * 3. 그리디 bin-packing: 각 섬을 현재 부하가 가장 작은 작업자에게 배정합니다.
 * 4. Entry의 AssigneeIndex (0-based)를 설정합니다.
 */
class SAFEASSETMOVER_API FAssetWorkloadDistributor
{
public:
	/**
	 * MaxWorkers 명에게 에셋을 균등 배분합니다.
	 * 실행 후 각 Entry의 AssigneeIndex가 0 ~ MaxWorkers-1로 설정됩니다.
	 */
	static void AssignWorkers(
		TArray<TSharedPtr<FAssetMoverEntry>>& Entries,
		const FDependencyGraph& Graph,
		int32 MaxWorkers
	);

	/**
	 * InternalDeps 기반 연결 컴포넌트 목록을 반환합니다.
	 * 반환값: 각 컴포넌트에 속한 PackageName 집합의 배열
	 */
	static TArray<TSet<FName>> FindConnectedComponents(
		const TArray<TSharedPtr<FAssetMoverEntry>>& Entries,
		const FDependencyGraph& Graph
	);
};
