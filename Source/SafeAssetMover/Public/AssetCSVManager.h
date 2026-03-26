// Copyright FB Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetDependencyAnalyzer.h"

/**
 * CSV 파일 기반 에셋 이동 목록 관리
 * 형식: SourcePath,TargetPath,AssigneeIndex
 * AssigneeIndex는 선택적 열 (-1 = 미배정)
 */
class SAFEASSETMOVER_API FAssetCSVManager
{
public:
	/**
	 * 현재 에셋 목록을 CSV 파일로 내보냅니다.
	 * 헤더 행: SourcePath,TargetPath,AssigneeIndex
	 * @return 성공 여부
	 */
	static bool ExportCSV(
		const FString& CSVFilePath,
		const TArray<TSharedPtr<FAssetMoverEntry>>& Entries
	);

	/**
	 * CSV 파일에서 TargetPath 및 AssigneeIndex를 가져와
	 * SourcePath로 기존 항목을 매칭하여 덮어씁니다.
	 * @return 매칭·업데이트된 행 수 (0 = 매칭 없음, -1 = 파일 읽기 실패)
	 */
	static int32 ImportCSV(
		const FString& CSVFilePath,
		TArray<TSharedPtr<FAssetMoverEntry>>& Entries
	);
};
