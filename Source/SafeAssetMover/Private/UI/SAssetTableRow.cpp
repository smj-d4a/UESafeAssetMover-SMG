// Copyright FB Team. All Rights Reserved.

#include "UI/SAssetTableRow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Styling/AppStyle.h"
#include "AssetThumbnail.h"
#include "ClassIconFinder.h"

#define LOCTEXT_NAMESPACE "SAssetTableRow"

void SAssetTableRow::Construct(
	const FArguments& InArgs,
	const TSharedRef<STableViewBase>& InOwnerTableView)
{
	Entry = InArgs._Entry;
	OnCheckStateChanged = InArgs._OnCheckStateChanged;
	IsChecked = InArgs._IsChecked;

	SMultiColumnTableRow<TSharedPtr<FAssetMoverEntry>>::Construct(
		SMultiColumnTableRow<TSharedPtr<FAssetMoverEntry>>::FArguments(),
		InOwnerTableView
	);
}

TSharedRef<SWidget> SAssetTableRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (!Entry.IsValid())
	{
		return SNew(STextBlock).Text(LOCTEXT("Invalid", "N/A"));
	}

	if (ColumnName == SafeAssetMoverColumns::Checkbox)
	{
		return SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(4.f, 0.f)
			[
				SNew(SCheckBox)
				.IsChecked(IsChecked)
				.OnCheckStateChanged(OnCheckStateChanged)
			];
	}

	if (ColumnName == SafeAssetMoverColumns::Name)
	{
		// 에셋 클래스 아이콘 + 이름
		const FSlateBrush* AssetIcon = FClassIconFinder::FindIconForAsset(Entry->AssetData);

		return SNew(SHorizontalBox)
			.ToolTipText(FText::FromString(Entry->AssetData.GetObjectPathString()))
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2.f, 0.f)
			[
				SNew(SImage)
				.Image(AssetIcon)
				.DesiredSizeOverride(FVector2D(16.f, 16.f))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			.Padding(4.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::FromName(Entry->AssetData.AssetName))
				.ColorAndOpacity(Entry->bHasCyclicDep
					? FSlateColor(FLinearColor(0.9f, 0.5f, 0.1f))  // 순환 의존성: 주황
					: FSlateColor::UseForeground())
			];
	}

	if (ColumnName == SafeAssetMoverColumns::Type)
	{
		return SNew(SBox)
			.Padding(4.f, 0.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromName(Entry->AssetData.AssetClassPath.GetAssetName()))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)))
			];
	}

	if (ColumnName == SafeAssetMoverColumns::Refs)
	{
		return SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(2.f, 0.f)
			[
				MakeCountBadge(Entry->ReferenceCount)
			];
	}

	if (ColumnName == SafeAssetMoverColumns::Deps)
	{
		return SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(2.f, 0.f)
			[
				MakeCountBadge(Entry->DependencyCount)
			];
	}

	if (ColumnName == SafeAssetMoverColumns::Order)
	{
		return SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(2.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::AsNumber(Entry->MoveOrder + 1))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.8f, 1.0f)))
			];
	}

	if (ColumnName == SafeAssetMoverColumns::Status)
	{
		return SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(4.f, 0.f)
			[
				SNew(STextBlock)
				.Text(GetStatusText(Entry->Status))
				.ColorAndOpacity(GetStatusColor(Entry->Status))
			];
	}

	return SNew(STextBlock).Text(LOCTEXT("Unknown", "?"));
}

FSlateColor SAssetTableRow::GetCountColor(int32 Count)
{
	if (Count <= 2)  return FSlateColor(FLinearColor(0.2f, 0.8f, 0.2f));   // 녹색
	if (Count <= 9)  return FSlateColor(FLinearColor(0.9f, 0.7f, 0.1f));   // 노란색
	return FSlateColor(FLinearColor(0.9f, 0.2f, 0.2f));                    // 빨간색
}

FSlateColor SAssetTableRow::GetStatusColor(EMoveStatus Status)
{
	switch (Status)
	{
	case EMoveStatus::Pending:    return FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f));
	case EMoveStatus::InProgress: return FSlateColor(FLinearColor(0.3f, 0.7f, 1.0f));
	case EMoveStatus::Moved:      return FSlateColor(FLinearColor(0.2f, 0.8f, 0.2f));
	case EMoveStatus::Failed:     return FSlateColor(FLinearColor(0.9f, 0.2f, 0.2f));
	case EMoveStatus::Skipped:    return FSlateColor(FLinearColor(0.6f, 0.6f, 0.3f));
	}
	return FSlateColor::UseForeground();
}

FText SAssetTableRow::GetStatusText(EMoveStatus Status)
{
	switch (Status)
	{
	case EMoveStatus::Pending:    return LOCTEXT("Pending",    "대기");
	case EMoveStatus::InProgress: return LOCTEXT("InProgress", "이동 중");
	case EMoveStatus::Moved:      return LOCTEXT("Moved",      "완료");
	case EMoveStatus::Failed:     return LOCTEXT("Failed",     "실패");
	case EMoveStatus::Skipped:    return LOCTEXT("Skipped",    "건너뜀");
	}
	return LOCTEXT("Unknown", "-");
}

TSharedRef<SWidget> SAssetTableRow::MakeCountBadge(int32 Count) const
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("RoundedSelection_AllCorners"))
		.BorderBackgroundColor(GetCountColor(Count))
		.Padding(FMargin(8.f, 2.f))
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::AsNumber(Count))
			.ColorAndOpacity(FSlateColor(FLinearColor::White))
		];
}

#undef LOCTEXT_NAMESPACE
