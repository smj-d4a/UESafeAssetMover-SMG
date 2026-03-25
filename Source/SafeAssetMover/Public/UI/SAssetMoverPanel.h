// Copyright FB Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "AssetDependencyAnalyzer.h"
#include "AssetMoveExecutor.h"

/**
 * SafeAssetMover 메인 패널 위젯
 */
class SAFEASSETMOVER_API SAssetMoverPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAssetMoverPanel)
		: _InitialSourceFolder(TEXT(""))
	{}
		SLATE_ARGUMENT(FString, InitialSourceFolder)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SAssetMoverPanel() override;

	void SetSourceFolder(const FString& Folder);

private:
	// ── 데이터 ──
	TArray<TSharedPtr<FAssetMoverEntry>> EntryList;
	FDependencyGraph DependencyGraph;
	TSharedPtr<FAssetMoveExecutor> Executor;

	// ── UI 참조 ──
	TSharedPtr<SListView<TSharedPtr<FAssetMoverEntry>>> AssetListView;
	TSharedPtr<class SEditableTextBox> SourceFolderBox;
	TSharedPtr<class SEditableTextBox> TargetFolderBox;
	TSharedPtr<class SProgressBar>     ProgressBarWidget;
	TSharedPtr<class STextBlock>       ProgressTextWidget;
	TSharedPtr<class STextBlock>       CyclicWarningWidget;
	TSharedPtr<class SButton>          MoveButton;
	TSharedPtr<class SButton>          FixUpButton;
	TSharedPtr<class SButton>          CancelButton;
	TSharedPtr<class SButton>          UndoButton;

	// ── 상태 ──
	FString SourceFolder;
	FString TargetFolder;
	EColumnSortMode::Type SortMode   = EColumnSortMode::Ascending;
	FName SortColumn                 = FName("MoveOrder");
	float ProgressValue              = 0.f;
	FText ProgressText;
	bool  bIsMoving                  = false;
	int32 PendingRedirectorCount     = 0;
	TArray<FString> CreatedRedirectorPaths;
	int32 TotalToMove                = 0;

	// ── 이벤트 핸들러 ──
	FReply OnScanClicked();
	FReply OnMoveSelectedClicked();
	FReply OnFixUpRedirectorsClicked();
	FReply OnCancelClicked();
	FReply OnUndoClicked();
	FReply OnSelectAllClicked();
	FReply OnDeselectAllClicked();
	FReply OnBrowseSourceClicked();
	FReply OnBrowseTargetClicked();

	void OnSortColumnChanged(EColumnSortPriority::Type Priority, const FName& Column, EColumnSortMode::Type Mode);
	EColumnSortMode::Type GetColumnSortMode(const FName ColumnId) const;

	void OnSourceFolderTextChanged(const FText& Text);
	void OnTargetFolderTextChanged(const FText& Text);

	void OnAssetCheckboxChanged(ECheckBoxState NewState, TSharedPtr<FAssetMoverEntry> Entry);
	ECheckBoxState GetAssetCheckboxState(TSharedPtr<FAssetMoverEntry> Entry) const;

	// ── 쿼리 ──
	bool CanMoveSelected() const;
	bool CanFixUpRedirectors() const;
	bool CanCancelMove() const;
	bool CanUndo() const;
	int32 GetSelectedCount() const;
	int32 GetCyclicDepCount() const;
	FText GetStatusText() const;
	FText GetMoveButtonText() const;
	FText GetFixUpButtonText() const;
	FText GetSelectedCountText() const;
	FText GetCyclicWarningText() const;
	EVisibility GetCyclicWarningVisibility() const;
	TOptional<float> GetProgressValue() const;
	EVisibility GetProgressVisibility() const;
	FText GetProgressText() const;

	// ── 내부 유틸 ──
	void RefreshList();
	void SortEntries();
	void RecalculateTargetPaths();
	void BuildTableColumns(TSharedRef<class SHeaderRow>& OutHeaderRow);
	TSharedRef<ITableRow> GenerateAssetRow(
		TSharedPtr<FAssetMoverEntry> Entry,
		const TSharedRef<STableViewBase>& OwnerTable
	);

	void OnAssetMovedCallback(const FAssetMoverEntry& Entry, bool bSuccess);
	void OnMoveCompletedCallback(int32 MovedCount);
	void OnUndoCompletedCallback(int32 RestoredCount);
};
