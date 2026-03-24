// Copyright FB Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "AssetDependencyAnalyzer.h"
#include "AssetMoveExecutor.h"

/**
 * SafeAssetMover 메인 패널 위젯
 *
 * 레이아웃:
 * - 소스/대상 폴더 입력 + 스캔 버튼
 * - 에셋 목록 테이블 (정렬 지원)
 * - 선택/해제 버튼 + 선택 카운트
 * - 이동 실행 / FixUpRedirectors / 취소 버튼
 * - 프로그레스 바
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

	// 외부에서 소스 폴더를 설정할 때 사용
	void SetSourceFolder(const FString& Folder);

private:
	// ────────────── 데이터 ──────────────
	TArray<TSharedPtr<FAssetMoverEntry>> EntryList;
	FDependencyGraph DependencyGraph;
	TSharedPtr<FAssetMoveExecutor> Executor;

	// ────────────── UI 참조 ──────────────
	TSharedPtr<SListView<TSharedPtr<FAssetMoverEntry>>> AssetListView;
	TSharedPtr<class SEditableTextBox> SourceFolderBox;
	TSharedPtr<class SEditableTextBox> TargetFolderBox;
	TSharedPtr<class SProgressBar> ProgressBarWidget;
	TSharedPtr<class STextBlock> ProgressTextWidget;
	TSharedPtr<class SButton> MoveButton;
	TSharedPtr<class SButton> FixUpButton;
	TSharedPtr<class SButton> CancelButton;

	// ────────────── 상태 ──────────────
	FString SourceFolder;
	FString TargetFolder;
	EColumnSortMode::Type SortMode = EColumnSortMode::Ascending;
	FName SortColumn = FName("MoveOrder");
	float ProgressValue = 0.f;
	FText ProgressText;
	bool bIsMoving = false;
	int32 PendingRedirectorCount = 0;
	TArray<FString> CreatedRedirectorPaths;

	// ────────────── 이벤트 핸들러 ──────────────
	FReply OnScanClicked();
	FReply OnMoveSelectedClicked();
	FReply OnFixUpRedirectorsClicked();
	FReply OnCancelClicked();
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

	// ────────────── 쿼리 함수 ──────────────
	bool CanMoveSelected() const;
	bool CanFixUpRedirectors() const;
	bool CanCancelMove() const;
	int32 GetSelectedCount() const;
	FText GetStatusText() const;
	FText GetMoveButtonText() const;
	FText GetFixUpButtonText() const;
	FText GetSelectedCountText() const;
	TOptional<float> GetProgressValue() const;
	EVisibility GetProgressVisibility() const;
	FText GetProgressText() const;

	// ────────────── 내부 유틸 ──────────────
	void RefreshList();
	void SortEntries();
	void BuildTableColumns(TSharedRef<class SHeaderRow>& OutHeaderRow);
	TSharedRef<ITableRow> GenerateAssetRow(
		TSharedPtr<FAssetMoverEntry> Entry,
		const TSharedRef<STableViewBase>& OwnerTable
	);

	void OnAssetMovedCallback(const FAssetMoverEntry& Entry, bool bSuccess);
	void OnMoveCompletedCallback(int32 MovedCount);
};
