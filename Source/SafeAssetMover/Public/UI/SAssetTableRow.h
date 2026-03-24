// Copyright FB Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Views/STableRow.h"
#include "AssetDependencyAnalyzer.h"

/** 칼럼 ID 상수 */
namespace SafeAssetMoverColumns
{
	static const FName Checkbox  = TEXT("Checkbox");
	static const FName Name      = TEXT("Name");
	static const FName Type      = TEXT("Type");
	static const FName Refs      = TEXT("References");
	static const FName Deps      = TEXT("Dependencies");
	static const FName Order     = TEXT("MoveOrder");
	static const FName Status    = TEXT("Status");
}

/**
 * 에셋 목록 테이블의 한 행 위젯
 * 체크박스, 이름, 타입, 레퍼런스 배지, 디펜던시 배지, 이동 순서, 상태를 표시합니다.
 */
class SAFEASSETMOVER_API SAssetTableRow : public SMultiColumnTableRow<TSharedPtr<FAssetMoverEntry>>
{
public:
	SLATE_BEGIN_ARGS(SAssetTableRow) {}
		SLATE_ARGUMENT(TSharedPtr<FAssetMoverEntry>, Entry)
		SLATE_EVENT(FOnCheckStateChanged, OnCheckStateChanged)
		SLATE_ATTRIBUTE(ECheckBoxState, IsChecked)
	SLATE_END_ARGS()

	void Construct(
		const FArguments& InArgs,
		const TSharedRef<STableViewBase>& InOwnerTableView
	);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
	TSharedPtr<FAssetMoverEntry> Entry;
	FOnCheckStateChanged OnCheckStateChanged;
	TAttribute<ECheckBoxState> IsChecked;

	// 수치에 따른 배지 색상 (녹색/노란색/빨간색)
	static FSlateColor GetCountColor(int32 Count);

	// 상태에 따른 텍스트 색상
	static FSlateColor GetStatusColor(EMoveStatus Status);

	// 상태 텍스트
	static FText GetStatusText(EMoveStatus Status);

	// 수치 배지 위젯 생성
	TSharedRef<SWidget> MakeCountBadge(int32 Count) const;
};
