// Copyright FB Team. All Rights Reserved.

#include "UI/SAssetTableRow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Styling/AppStyle.h"
#include "AssetThumbnail.h"
#include "Styling/SlateIconFinder.h"

#define LOCTEXT_NAMESPACE "SAssetTableRow"

void SAssetTableRow::Construct(
	const FArguments& InArgs,
	const TSharedRef<STableViewBase>& InOwnerTableView)
{
	Entry               = InArgs._Entry;
	OnCheckStateChanged = InArgs._OnCheckStateChanged;
	IsChecked           = InArgs._IsChecked;
	bIsAssignedToMe     = InArgs._bIsAssignedToMe.IsSet()
	                        ? InArgs._bIsAssignedToMe
	                        : TAttribute<bool>(true);
	OnTargetPathChanged = InArgs._OnTargetPathChanged;

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
				.IsEnabled(bIsAssignedToMe)
			];
	}

	if (ColumnName == SafeAssetMoverColumns::Name)
	{
		UClass* AssetClass = Entry->AssetData.GetClass();
		FSlateIcon SlateIcon = FSlateIconFinder::FindIconForClass(AssetClass);
		const FSlateBrush* AssetIcon = SlateIcon.GetOptionalIcon();
		if (!AssetIcon)
		{
			AssetIcon = FAppStyle::GetBrush("ClassIcon.Default");
		}

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
				// SEditableText read-only: 클릭/드래그로 텍스트 선택·복사 가능
				SNew(SEditableText)
				.Text(FText::FromName(Entry->AssetData.AssetName))
				.IsReadOnly(true)
				.ColorAndOpacity(this, &SAssetTableRow::GetEntryNameColor)
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
			.ToolTipText(this, &SAssetTableRow::GetEntryTargetTooltip)
			[
				SNew(STextBlock)
				.Text(this, &SAssetTableRow::GetEntryStatusText)
				.ColorAndOpacity(this, &SAssetTableRow::GetEntryStatusColor)
			];
	}

	if (ColumnName == SafeAssetMoverColumns::Target)
	{
		return SNew(SBox)
			.Padding(4.f, 0.f)
			.VAlign(VAlign_Center)
			[
				// 내 담당 행: 직접 편집 가능 / 비담당 행: 읽기 전용
				SNew(SEditableText)
				.Text(FText::FromString(Entry->TargetPath.IsEmpty() ? TEXT("-") : Entry->TargetPath))
				.IsReadOnly(this, &SAssetTableRow::IsTargetReadOnly)
				.OnTextCommitted(this, &SAssetTableRow::OnTargetTextCommitted)
				.ColorAndOpacity(this, &SAssetTableRow::GetRowForegroundColor)
			];
	}

	if (ColumnName == SafeAssetMoverColumns::Reason)
	{
		return SNew(SBox)
			.Padding(4.f, 0.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SAssetTableRow::GetEntryReasonText)
				.ColorAndOpacity(this, &SAssetTableRow::GetRowForegroundColor)
			];
	}

	if (ColumnName == SafeAssetMoverColumns::Assignee)
	{
		return SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(4.f, 0.f)
			[
				SNew(STextBlock)
				.Text(this, &SAssetTableRow::GetEntryAssigneeText)
				.ColorAndOpacity(this, &SAssetTableRow::GetRowForegroundColor)
			];
	}

	return SNew(STextBlock).Text(LOCTEXT("Unknown", "?"));
}

// ── 동적 바인딩 구현 ──────────────────────────────────────────────

FText SAssetTableRow::GetEntryStatusText() const
{
	return Entry.IsValid() ? GetStatusText(Entry->Status) : FText::GetEmpty();
}

FSlateColor SAssetTableRow::GetEntryStatusColor() const
{
	return Entry.IsValid() ? GetStatusColor(Entry->Status) : FSlateColor::UseForeground();
}

FText SAssetTableRow::GetEntryTargetText() const
{
	if (!Entry.IsValid()) return FText::GetEmpty();
	return FText::FromString(Entry->TargetPath.IsEmpty() ? TEXT("-") : Entry->TargetPath);
}

FSlateColor SAssetTableRow::GetEntryNameColor() const
{
	if (!bIsAssignedToMe.Get(true))
	{
		return FSlateColor(FLinearColor(0.35f, 0.35f, 0.35f));
	}
	if (Entry.IsValid() && Entry->bHasCyclicDep)
	{
		return FSlateColor(FLinearColor(0.9f, 0.5f, 0.1f));
	}
	return FSlateColor::UseForeground();
}

FSlateColor SAssetTableRow::GetRowForegroundColor() const
{
	return bIsAssignedToMe.Get(true)
		? FSlateColor::UseForeground()
		: FSlateColor(FLinearColor(0.35f, 0.35f, 0.35f));
}

FText SAssetTableRow::GetEntryAssigneeText() const
{
	if (!Entry.IsValid()) return FText::GetEmpty();
	if (Entry->AssigneeIndex < 0)
	{
		return LOCTEXT("Unassigned", "-");
	}
	return FText::Format(LOCTEXT("AssigneeFmt", "작업자 {0}"), FText::AsNumber(Entry->AssigneeIndex + 1));
}

bool SAssetTableRow::IsTargetReadOnly() const
{
	// 배정이 완료된 경우 내 담당 행만 편집 허용
	return !bIsAssignedToMe.Get(true);
}

void SAssetTableRow::OnTargetTextCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	if (!Entry.IsValid()) return;
	if (CommitType == ETextCommit::OnCleared) return;

	const FString NewPath = NewText.ToString();
	if (NewPath == TEXT("-")) return; // placeholder — 편집 무시

	Entry->TargetPath = NewPath;
	OnTargetPathChanged.ExecuteIfBound();
}

FText SAssetTableRow::GetEntryReasonText() const
{
	if (!Entry.IsValid()) return FText::GetEmpty();
	return FText::FromString(Entry->FailureReason);
}

FText SAssetTableRow::GetEntryTargetTooltip() const
{
	if (!Entry.IsValid()) return FText::GetEmpty();
	if (Entry->Status == EMoveStatus::LockedByOther && !Entry->CheckedOutBy.IsEmpty())
	{
		return FText::Format(
			LOCTEXT("LockedByTooltip", "P4 Checkout 중인 사용자: {0}"),
			FText::FromString(Entry->CheckedOutBy));
	}
	return FText::GetEmpty();
}

// ── 정적 유틸 ────────────────────────────────────────────────────

FSlateColor SAssetTableRow::GetCountColor(int32 Count)
{
	if (Count <= 2)  return FSlateColor(FLinearColor(0.2f, 0.8f, 0.2f));
	if (Count <= 9)  return FSlateColor(FLinearColor(0.9f, 0.7f, 0.1f));
	return FSlateColor(FLinearColor(0.9f, 0.2f, 0.2f));
}

FSlateColor SAssetTableRow::GetStatusColor(EMoveStatus Status)
{
	switch (Status)
	{
	case EMoveStatus::Pending:       return FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f));
	case EMoveStatus::InProgress:    return FSlateColor(FLinearColor(0.3f, 0.7f, 1.0f));
	case EMoveStatus::Moved:         return FSlateColor(FLinearColor(0.2f, 0.8f, 0.2f));
	case EMoveStatus::Failed:        return FSlateColor(FLinearColor(0.9f, 0.2f, 0.2f));
	case EMoveStatus::Skipped:       return FSlateColor(FLinearColor(0.6f, 0.6f, 0.3f));
	case EMoveStatus::LockedByOther: return FSlateColor(FLinearColor(0.9f, 0.4f, 0.0f));
	}
	return FSlateColor::UseForeground();
}

FText SAssetTableRow::GetStatusText(EMoveStatus Status)
{
	switch (Status)
	{
	case EMoveStatus::Pending:       return LOCTEXT("Pending",       "대기");
	case EMoveStatus::InProgress:    return LOCTEXT("InProgress",    "이동 중");
	case EMoveStatus::Moved:         return LOCTEXT("Moved",         "완료");
	case EMoveStatus::Failed:        return LOCTEXT("Failed",        "실패");
	case EMoveStatus::Skipped:       return LOCTEXT("Skipped",       "건너뜀");
	case EMoveStatus::LockedByOther: return LOCTEXT("LockedByOther", "P4 잠금");
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
