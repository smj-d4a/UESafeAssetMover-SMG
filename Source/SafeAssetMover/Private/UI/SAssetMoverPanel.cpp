// Copyright FB Team. All Rights Reserved.

#include "UI/SAssetMoverPanel.h"
#include "UI/SAssetTableRow.h"
#include "AssetDependencyAnalyzer.h"
#include "AssetMoveScheduler.h"
#include "AssetMoveExecutor.h"

#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Editor.h"
#include "UnrealEdGlobals.h"

#define LOCTEXT_NAMESPACE "SAssetMoverPanel"

void SAssetMoverPanel::Construct(const FArguments& InArgs)
{
	SourceFolder = InArgs._InitialSourceFolder;
	Executor = MakeShared<FAssetMoveExecutor>();

	Executor->OnAssetMoved.AddSP(this, &SAssetMoverPanel::OnAssetMovedCallback);
	Executor->OnMoveCompleted.AddSP(this, &SAssetMoverPanel::OnMoveCompletedCallback);

	// 헤더 행 구성
	TSharedRef<SHeaderRow> HeaderRow = SNew(SHeaderRow);
	BuildTableColumns(HeaderRow);

	ChildSlot
	[
		SNew(SVerticalBox)

		// ── 상단 폴더 입력 영역 ──
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.f, 8.f, 8.f, 4.f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SourceFolder", "소스 폴더:"))
					.MinDesiredWidth(80.f)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SAssignNew(SourceFolderBox, SEditableTextBox)
					.Text(FText::FromString(SourceFolder))
					.HintText(LOCTEXT("SourceHint", "/Game/OldFolder"))
					.OnTextChanged(this, &SAssetMoverPanel::OnSourceFolderTextChanged)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Browse", "..."))
					.ToolTipText(LOCTEXT("BrowseSource", "콘텐츠 브라우저에서 소스 폴더 선택"))
					.OnClicked(this, &SAssetMoverPanel::OnBrowseSourceClicked)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Scan", "스캔"))
					.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
					.ToolTipText(LOCTEXT("ScanTooltip", "소스 폴더의 에셋을 스캔합니다"))
					.OnClicked(this, &SAssetMoverPanel::OnScanClicked)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("TargetFolder", "대상 폴더:"))
					.MinDesiredWidth(80.f)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SAssignNew(TargetFolderBox, SEditableTextBox)
					.HintText(LOCTEXT("TargetHint", "/Game/NewFolder"))
					.OnTextChanged(this, &SAssetMoverPanel::OnTargetFolderTextChanged)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Browse", "..."))
					.ToolTipText(LOCTEXT("BrowseTarget", "콘텐츠 브라우저에서 대상 폴더 선택"))
					.OnClicked(this, &SAssetMoverPanel::OnBrowseTargetClicked)
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSeparator)
		]

		// ── 에셋 목록 테이블 ──
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		.Padding(8.f, 4.f)
		[
			SAssignNew(AssetListView, SListView<TSharedPtr<FAssetMoverEntry>>)
			.ListItemsSource(&EntryList)
			.OnGenerateRow(this, &SAssetMoverPanel::GenerateAssetRow)
			.HeaderRow(HeaderRow)
			.SelectionMode(ESelectionMode::None)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSeparator)
		]

		// ── 선택 버튼 + 카운트 ──
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.f, 4.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("SelectAll", "전체 선택"))
				.OnClicked(this, &SAssetMoverPanel::OnSelectAllClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("DeselectAll", "전체 해제"))
				.OnClicked(this, &SAssetMoverPanel::OnDeselectAllClicked)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SAssetMoverPanel::GetSelectedCountText)
				.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.8f, 1.0f)))
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSeparator)
		]

		// ── 실행 버튼 영역 ──
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.f, 4.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 4.f, 0.f)
			[
				SAssignNew(MoveButton, SButton)
				.Text(this, &SAssetMoverPanel::GetMoveButtonText)
				.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
				.ToolTipText(LOCTEXT("MoveTooltip", "선택된 에셋을 의존성 순서로 이동합니다"))
				.IsEnabled(this, &SAssetMoverPanel::CanMoveSelected)
				.OnClicked(this, &SAssetMoverPanel::OnMoveSelectedClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 4.f, 0.f)
			[
				SAssignNew(FixUpButton, SButton)
				.Text(this, &SAssetMoverPanel::GetFixUpButtonText)
				.ToolTipText(LOCTEXT("FixUpTooltip", "생성된 리다이렉터를 수정합니다"))
				.IsEnabled(this, &SAssetMoverPanel::CanFixUpRedirectors)
				.OnClicked(this, &SAssetMoverPanel::OnFixUpRedirectorsClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew(CancelButton, SButton)
				.Text(LOCTEXT("Cancel", "취소"))
				.ToolTipText(LOCTEXT("CancelTooltip", "진행 중인 이동 작업을 취소합니다"))
				.IsEnabled(this, &SAssetMoverPanel::CanCancelMove)
				.OnClicked(this, &SAssetMoverPanel::OnCancelClicked)
			]
		]

		// ── 프로그레스 바 ──
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.f, 2.f, 8.f, 4.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(ProgressBarWidget, SProgressBar)
				.Percent(this, &SAssetMoverPanel::GetProgressValue)
				.Visibility(this, &SAssetMoverPanel::GetProgressVisibility)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SAssignNew(ProgressTextWidget, STextBlock)
				.Text(this, &SAssetMoverPanel::GetProgressText)
				.Visibility(this, &SAssetMoverPanel::GetProgressVisibility)
			]
		]

		// ── 상태 표시줄 ──
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.f, 0.f, 8.f, 6.f)
		[
			SNew(STextBlock)
			.Text(this, &SAssetMoverPanel::GetStatusText)
			.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
		]
	];

	// 초기 소스 폴더가 있으면 자동 스캔
	if (!SourceFolder.IsEmpty())
	{
		OnScanClicked();
	}
}

SAssetMoverPanel::~SAssetMoverPanel()
{
	if (Executor.IsValid())
	{
		Executor->OnAssetMoved.RemoveAll(this);
		Executor->OnMoveCompleted.RemoveAll(this);
	}
}

void SAssetMoverPanel::SetSourceFolder(const FString& Folder)
{
	SourceFolder = Folder;
	if (SourceFolderBox.IsValid())
	{
		SourceFolderBox->SetText(FText::FromString(Folder));
	}
}

// ────────────── 이벤트 핸들러 ──────────────

FReply SAssetMoverPanel::OnScanClicked()
{
	if (SourceFolder.IsEmpty()) return FReply::Handled();

	EntryList.Empty();
	DependencyGraph = FDependencyGraph();
	CreatedRedirectorPaths.Empty();
	PendingRedirectorCount = 0;

	FAssetDependencyAnalyzer::AnalyzeFolder(SourceFolder, EntryList, DependencyGraph);
	FAssetMoveScheduler::ComputeMoveOrder(EntryList, DependencyGraph);
	SortEntries();

	RefreshList();

	UE_LOG(LogSafeAssetMover, Log, TEXT("Scan complete: %d assets found."), EntryList.Num());
	return FReply::Handled();
}

FReply SAssetMoverPanel::OnMoveSelectedClicked()
{
	if (TargetFolder.IsEmpty())
	{
		UE_LOG(LogSafeAssetMover, Warning, TEXT("Target folder not specified."));
		return FReply::Handled();
	}

	bIsMoving = true;
	ProgressValue = 0.f;

	Executor->MoveAssets(EntryList, TargetFolder);
	return FReply::Handled();
}

FReply SAssetMoverPanel::OnFixUpRedirectorsClicked()
{
	// 소스/대상 폴더 양쪽에서 리다이렉터 수집
	TArray<FString> AllRedirectors = CreatedRedirectorPaths;

	TArray<FString> SourceRedirectors;
	FAssetMoveExecutor::CollectRedirectors(SourceFolder, SourceRedirectors);
	AllRedirectors.Append(SourceRedirectors);

	// 중복 제거
	TSet<FString> UniqueSet(AllRedirectors);
	AllRedirectors = UniqueSet.Array();

	Executor->FixUpRedirectors(AllRedirectors);

	CreatedRedirectorPaths.Empty();
	PendingRedirectorCount = 0;
	return FReply::Handled();
}

FReply SAssetMoverPanel::OnCancelClicked()
{
	if (Executor.IsValid())
	{
		Executor->RequestCancel();
	}
	return FReply::Handled();
}

FReply SAssetMoverPanel::OnSelectAllClicked()
{
	for (TSharedPtr<FAssetMoverEntry>& Entry : EntryList)
	{
		Entry->bSelected = true;
	}
	RefreshList();
	return FReply::Handled();
}

FReply SAssetMoverPanel::OnDeselectAllClicked()
{
	for (TSharedPtr<FAssetMoverEntry>& Entry : EntryList)
	{
		Entry->bSelected = false;
	}
	RefreshList();
	return FReply::Handled();
}

FReply SAssetMoverPanel::OnBrowseSourceClicked()
{
	FContentBrowserModule& CBModule =
		FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	FOpenAssetDialogConfig DialogConfig;
	DialogConfig.bAllowMultipleSelection = false;
	DialogConfig.DefaultPath = SourceFolder.IsEmpty() ? TEXT("/Game") : SourceFolder;

	// 폴더 선택 다이얼로그 (간단히 텍스트 입력 활용)
	// 실제 폴더 선택은 콘텐츠 브라우저 API가 제한적이므로 PathPicker 사용
	TSharedRef<SWindow> PickerWindow = SNew(SWindow)
		.Title(LOCTEXT("PickSourceFolder", "소스 폴더 선택"))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(400.f, 500.f));

	TSharedPtr<FString> SelectedPath = MakeShared<FString>(SourceFolder);

	FPathPickerConfig PathConfig;
	PathConfig.DefaultPath = SourceFolder.IsEmpty() ? TEXT("/Game") : SourceFolder;
	PathConfig.OnPathSelected = FOnPathSelected::CreateLambda(
		[this, &PickerWindow, SelectedPath](const FString& Path)
		{
			*SelectedPath = Path;
		}
	);

	TSharedPtr<SWidget> PathPicker = CBModule.Get().CreatePathPicker(PathConfig);

	PickerWindow->SetContent(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			PathPicker.IsValid() ? PathPicker.ToSharedRef() : SNullWidget::NullWidget
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.Padding(8.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("Select", "선택"))
			.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
			.OnClicked(FOnClicked::CreateLambda([&PickerWindow]()
			{
				PickerWindow->RequestDestroyWindow();
				return FReply::Handled();
			}))
		]
	);

	GEditor->EditorAddModalWindow(PickerWindow);

	if (!SelectedPath->IsEmpty())
	{
		SourceFolder = *SelectedPath;
		SourceFolderBox->SetText(FText::FromString(SourceFolder));
	}

	return FReply::Handled();
}

FReply SAssetMoverPanel::OnBrowseTargetClicked()
{
	FContentBrowserModule& CBModule =
		FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	TSharedRef<SWindow> PickerWindow = SNew(SWindow)
		.Title(LOCTEXT("PickTargetFolder", "대상 폴더 선택"))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(400.f, 500.f));

	TSharedPtr<FString> SelectedPath = MakeShared<FString>(TargetFolder);

	FPathPickerConfig PathConfig;
	PathConfig.DefaultPath = TargetFolder.IsEmpty() ? TEXT("/Game") : TargetFolder;
	PathConfig.OnPathSelected = FOnPathSelected::CreateLambda(
		[SelectedPath](const FString& Path)
		{
			*SelectedPath = Path;
		}
	);

	TSharedPtr<SWidget> PathPicker = CBModule.Get().CreatePathPicker(PathConfig);

	PickerWindow->SetContent(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			PathPicker.IsValid() ? PathPicker.ToSharedRef() : SNullWidget::NullWidget
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.Padding(8.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("Select", "선택"))
			.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
			.OnClicked(FOnClicked::CreateLambda([&PickerWindow]()
			{
				PickerWindow->RequestDestroyWindow();
				return FReply::Handled();
			}))
		]
	);

	GEditor->EditorAddModalWindow(PickerWindow);

	if (!SelectedPath->IsEmpty())
	{
		TargetFolder = *SelectedPath;
		TargetFolderBox->SetText(FText::FromString(TargetFolder));
	}

	return FReply::Handled();
}

void SAssetMoverPanel::OnSortColumnChanged(
	EColumnSortPriority::Type Priority,
	const FName& Column,
	EColumnSortMode::Type Mode)
{
	SortColumn = Column;
	SortMode = Mode;
	SortEntries();
	RefreshList();
}

EColumnSortMode::Type SAssetMoverPanel::GetColumnSortMode(const FName ColumnId) const
{
	return (SortColumn == ColumnId) ? SortMode : EColumnSortMode::None;
}

void SAssetMoverPanel::OnSourceFolderTextChanged(const FText& Text)
{
	SourceFolder = Text.ToString();
}

void SAssetMoverPanel::OnTargetFolderTextChanged(const FText& Text)
{
	TargetFolder = Text.ToString();
}

void SAssetMoverPanel::OnAssetCheckboxChanged(ECheckBoxState NewState, TSharedPtr<FAssetMoverEntry> Entry)
{
	if (Entry.IsValid())
	{
		Entry->bSelected = (NewState == ECheckBoxState::Checked);
	}
}

ECheckBoxState SAssetMoverPanel::GetAssetCheckboxState(TSharedPtr<FAssetMoverEntry> Entry) const
{
	return (Entry.IsValid() && Entry->bSelected)
		? ECheckBoxState::Checked
		: ECheckBoxState::Unchecked;
}

// ────────────── 쿼리 함수 ──────────────

bool SAssetMoverPanel::CanMoveSelected() const
{
	if (bIsMoving || TargetFolder.IsEmpty()) return false;
	return GetSelectedCount() > 0;
}

bool SAssetMoverPanel::CanFixUpRedirectors() const
{
	return !bIsMoving && PendingRedirectorCount > 0;
}

bool SAssetMoverPanel::CanCancelMove() const
{
	return bIsMoving;
}

int32 SAssetMoverPanel::GetSelectedCount() const
{
	int32 Count = 0;
	for (const TSharedPtr<FAssetMoverEntry>& Entry : EntryList)
	{
		if (Entry->bSelected) ++Count;
	}
	return Count;
}

FText SAssetMoverPanel::GetStatusText() const
{
	if (EntryList.Num() == 0)
	{
		return LOCTEXT("NoAssets", "소스 폴더를 선택하고 스캔하세요.");
	}
	return FText::Format(
		LOCTEXT("AssetCount", "에셋 {0}개 | 선택 {1}개"),
		FText::AsNumber(EntryList.Num()),
		FText::AsNumber(GetSelectedCount())
	);
}

FText SAssetMoverPanel::GetMoveButtonText() const
{
	int32 SelCount = GetSelectedCount();
	if (SelCount == 0)
	{
		return LOCTEXT("MoveSelected", "이동");
	}
	return FText::Format(LOCTEXT("MoveCount", "▶ {0}개 이동"), FText::AsNumber(SelCount));
}

FText SAssetMoverPanel::GetFixUpButtonText() const
{
	if (PendingRedirectorCount > 0)
	{
		return FText::Format(
			LOCTEXT("FixUpWithCount", "Fix Up Redirectors ★{0}"),
			FText::AsNumber(PendingRedirectorCount)
		);
	}
	return LOCTEXT("FixUp", "Fix Up Redirectors");
}

FText SAssetMoverPanel::GetSelectedCountText() const
{
	return FText::Format(
		LOCTEXT("SelectedOf", "선택: {0} / {1}"),
		FText::AsNumber(GetSelectedCount()),
		FText::AsNumber(EntryList.Num())
	);
}

TOptional<float> SAssetMoverPanel::GetProgressValue() const
{
	return TOptional<float>(ProgressValue);
}

EVisibility SAssetMoverPanel::GetProgressVisibility() const
{
	return bIsMoving ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SAssetMoverPanel::GetProgressText() const
{
	return ProgressText;
}

// ────────────── 내부 유틸 ──────────────

void SAssetMoverPanel::RefreshList()
{
	if (AssetListView.IsValid())
	{
		AssetListView->RequestListRefresh();
	}
}

void SAssetMoverPanel::SortEntries()
{
	EntryList.Sort([this](const TSharedPtr<FAssetMoverEntry>& A, const TSharedPtr<FAssetMoverEntry>& B)
	{
		const bool bAsc = (SortMode == EColumnSortMode::Ascending);

		if (SortColumn == SafeAssetMoverColumns::Refs)
			return bAsc ? A->ReferenceCount < B->ReferenceCount
			            : A->ReferenceCount > B->ReferenceCount;

		if (SortColumn == SafeAssetMoverColumns::Deps)
			return bAsc ? A->DependencyCount < B->DependencyCount
			            : A->DependencyCount > B->DependencyCount;

		if (SortColumn == SafeAssetMoverColumns::Order)
			return bAsc ? A->MoveOrder < B->MoveOrder
			            : A->MoveOrder > B->MoveOrder;

		if (SortColumn == SafeAssetMoverColumns::Type)
		{
			FString TA = A->AssetData.AssetClassPath.GetAssetName().ToString();
			FString TB = B->AssetData.AssetClassPath.GetAssetName().ToString();
			return bAsc ? TA < TB : TA > TB;
		}

		// 기본: 이름 정렬
		FString NA = A->AssetData.AssetName.ToString();
		FString NB = B->AssetData.AssetName.ToString();
		return bAsc ? NA < NB : NA > NB;
	});
}

void SAssetMoverPanel::BuildTableColumns(TSharedRef<SHeaderRow>& OutHeaderRow)
{
	OutHeaderRow->AddColumn(
		SHeaderRow::Column(SafeAssetMoverColumns::Checkbox)
		.DefaultLabel(FText::GetEmpty())
		.FixedWidth(30.f)
	);

	OutHeaderRow->AddColumn(
		SHeaderRow::Column(SafeAssetMoverColumns::Name)
		.DefaultLabel(LOCTEXT("ColName", "이름"))
		.FillWidth(3.f)
		.SortMode(this, &SAssetMoverPanel::GetColumnSortMode, SafeAssetMoverColumns::Name)
		.OnSort(this, &SAssetMoverPanel::OnSortColumnChanged)
	);

	OutHeaderRow->AddColumn(
		SHeaderRow::Column(SafeAssetMoverColumns::Type)
		.DefaultLabel(LOCTEXT("ColType", "타입"))
		.FillWidth(1.5f)
		.SortMode(this, &SAssetMoverPanel::GetColumnSortMode, SafeAssetMoverColumns::Type)
		.OnSort(this, &SAssetMoverPanel::OnSortColumnChanged)
	);

	OutHeaderRow->AddColumn(
		SHeaderRow::Column(SafeAssetMoverColumns::Refs)
		.DefaultLabel(LOCTEXT("ColRefs", "레퍼런스"))
		.FixedWidth(90.f)
		.HAlignCell(HAlign_Center)
		.HAlignHeader(HAlign_Center)
		.SortMode(this, &SAssetMoverPanel::GetColumnSortMode, SafeAssetMoverColumns::Refs)
		.OnSort(this, &SAssetMoverPanel::OnSortColumnChanged)
	);

	OutHeaderRow->AddColumn(
		SHeaderRow::Column(SafeAssetMoverColumns::Deps)
		.DefaultLabel(LOCTEXT("ColDeps", "디펜던시"))
		.FixedWidth(90.f)
		.HAlignCell(HAlign_Center)
		.HAlignHeader(HAlign_Center)
		.SortMode(this, &SAssetMoverPanel::GetColumnSortMode, SafeAssetMoverColumns::Deps)
		.OnSort(this, &SAssetMoverPanel::OnSortColumnChanged)
	);

	OutHeaderRow->AddColumn(
		SHeaderRow::Column(SafeAssetMoverColumns::Order)
		.DefaultLabel(LOCTEXT("ColOrder", "순서"))
		.FixedWidth(60.f)
		.HAlignCell(HAlign_Center)
		.HAlignHeader(HAlign_Center)
		.SortMode(this, &SAssetMoverPanel::GetColumnSortMode, SafeAssetMoverColumns::Order)
		.OnSort(this, &SAssetMoverPanel::OnSortColumnChanged)
	);

	OutHeaderRow->AddColumn(
		SHeaderRow::Column(SafeAssetMoverColumns::Status)
		.DefaultLabel(LOCTEXT("ColStatus", "상태"))
		.FixedWidth(70.f)
		.HAlignCell(HAlign_Center)
		.HAlignHeader(HAlign_Center)
	);
}

TSharedRef<ITableRow> SAssetMoverPanel::GenerateAssetRow(
	TSharedPtr<FAssetMoverEntry> Entry,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SAssetTableRow, OwnerTable)
		.Entry(Entry)
		.IsChecked(this, &SAssetMoverPanel::GetAssetCheckboxState, Entry)
		.OnCheckStateChanged(this, &SAssetMoverPanel::OnAssetCheckboxChanged, Entry);
}

void SAssetMoverPanel::OnAssetMovedCallback(const FAssetMoverEntry& Entry, bool bSuccess)
{
	// 이동된 에셋의 리다이렉터 경로 추적
	if (bSuccess)
	{
		// 소스 경로에 생성된 리다이렉터 추적
		CreatedRedirectorPaths.AddUnique(Entry.SourcePath);
		PendingRedirectorCount = CreatedRedirectorPaths.Num();
	}

	// 진행률 업데이트
	int32 ProcessedCount = 0;
	for (const TSharedPtr<FAssetMoverEntry>& E : EntryList)
	{
		if (E->Status != EMoveStatus::Pending && E->Status != EMoveStatus::InProgress)
		{
			++ProcessedCount;
		}
	}
	int32 TotalSelected = GetSelectedCount() + ProcessedCount;
	if (TotalSelected > 0)
	{
		ProgressValue = static_cast<float>(ProcessedCount) / static_cast<float>(TotalSelected);
	}
	ProgressText = FText::Format(
		LOCTEXT("ProgressFmt", "{0} ({1}/{2})"),
		FText::FromName(Entry.AssetData.AssetName),
		FText::AsNumber(ProcessedCount),
		FText::AsNumber(TotalSelected)
	);

	RefreshList();
}

void SAssetMoverPanel::OnMoveCompletedCallback(int32 MovedCount)
{
	bIsMoving = false;
	ProgressValue = 1.f;
	ProgressText = FText::Format(
		LOCTEXT("MoveComplete", "완료: {0}개 이동됨"),
		FText::AsNumber(MovedCount)
	);

	// 리다이렉터 수 업데이트
	TArray<FString> FoundRedirectors;
	FAssetMoveExecutor::CollectRedirectors(SourceFolder, FoundRedirectors);
	PendingRedirectorCount = FoundRedirectors.Num();

	RefreshList();

	UE_LOG(LogSafeAssetMover, Log,
		TEXT("Move completed. %d assets moved. %d redirectors pending."),
		MovedCount, PendingRedirectorCount);
}

#undef LOCTEXT_NAMESPACE
