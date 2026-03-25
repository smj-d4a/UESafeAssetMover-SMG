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
	static const FName Target    = TEXT("Target");
}

/**
 * 에셋 목록 테이블의 한 행 위젯
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

	// ── TAttribute 동적 바인딩 (Entry 상태 변경 시 자동 갱신) ──
	FText       GetEntryStatusText()    const;
	FSlateColor GetEntryStatusColor()   const;
	FText       GetEntryTargetText()    const;
	FSlateColor GetEntryNameColor()     const;
	FText       GetEntryTargetTooltip() const;

	static FSlateColor GetCountColor(int32 Count);
	static FSlateColor GetStatusColor(EMoveStatus Status);
	static FText       GetStatusText(EMoveStatus Status);

	TSharedRef<SWidget> MakeCountBadge(int32 Count) const;
};
