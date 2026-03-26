// Copyright FB Team. All Rights Reserved.

#include "UI/SAssetMoverPanel.h"
#include "UI/SAssetTableRow.h"
#include "AssetDependencyAnalyzer.h"
#include "AssetMoveScheduler.h"
#include "AssetMoveExecutor.h"
#include "AssetCSVManager.h"
#include "AssetWorkloadDistributor.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"

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
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "SAssetMoverPanel"

void SAssetMoverPanel::Construct(const FArguments& InArgs)
{
	SourceFolder = InArgs._InitialSourceFolder;
	Executor = MakeShared<FAssetMoveExecutor>();

	Executor->OnAssetMoved.AddSP(this, &SAssetMoverPanel::OnAssetMovedCallback);
	Executor->OnMoveCompleted.AddSP(this, &SAssetMoverPanel::OnMoveCompletedCallback);
	Executor->OnUndoCompleted.AddSP(this, &SAssetMoverPanel::OnUndoCompletedCallback);

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
		[ SNew(SSeparator) ]

		// ── CSV / 담당자 배정 도구 ──
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.f, 4.f, 8.f, 4.f)
		[
			SNew(SHorizontalBox)

			// 작업자 수 레이블
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MaxWorkers", "작업자 수:"))
			]
			// 작업자 수 입력
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 8.f, 0.f)
			[
				SAssignNew(MaxWorkerCountBox, SEditableTextBox)
				.Text(FText::AsNumber(MaxWorkerCount))
				.MinDesiredWidth(36.f)
				.OnTextCommitted(this, &SAssetMoverPanel::OnMaxWorkerCountCommitted)
			]

			// 내 번호 레이블
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MyWorkerNum", "내 번호:"))
			]
			// 내 번호 입력
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 8.f, 0.f)
			[
				SAssignNew(MyAssigneeIndexBox, SEditableTextBox)
				.Text(FText::AsNumber(MyAssigneeIndex + 1))
				.MinDesiredWidth(36.f)
				.OnTextCommitted(this, &SAssetMoverPanel::OnMyAssigneeIndexCommitted)
			]

			// 담당자 배정 버튼
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("AssignWorkers", "담당자 배정"))
				.ToolTipText(LOCTEXT("AssignTooltip",
					"섬(연결 컴포넌트) 단위로 작업량을 분석하여\n"
					"각 작업자에게 균등하게 에셋을 배정합니다."))
				.IsEnabled(this, &SAssetMoverPanel::CanAssignWorkers)
				.OnClicked(this, &SAssetMoverPanel::OnAssignWorkersClicked)
			]

			// 스페이서
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)

			// CSV 내보내기
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("ExportCSV", "CSV 내보내기"))
				.ToolTipText(LOCTEXT("ExportCSVTooltip",
					"현재 에셋 목록 (담당자 배정 포함)을 CSV 파일로 저장합니다."))
				.IsEnabled(this, &SAssetMoverPanel::CanExportCSV)
				.OnClicked(this, &SAssetMoverPanel::OnExportCSVClicked)
			]

			// CSV 가져오기
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("ImportCSV", "CSV 가져오기"))
				.ToolTipText(LOCTEXT("ImportCSVTooltip",
					"CSV 파일에서 대상 경로와 담당자 배정을 가져옵니다.\n"
					"스캔 후 불러오면 TargetPath와 AssigneeIndex를 덮어씁니다."))
				.OnClicked(this, &SAssetMoverPanel::OnImportCSVClicked)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[ SNew(SSeparator) ]

		// ── 순환 의존성 경고 배너 ──
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.f, 4.f, 8.f, 0.f)
		[
			SAssignNew(CyclicWarningWidget, STextBlock)
			.Text(this, &SAssetMoverPanel::GetCyclicWarningText)
			.ColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.6f, 0.1f)))
			.Visibility(this, &SAssetMoverPanel::GetCyclicWarningVisibility)
		]

		// ── 에셋 목록 테이블 (가로 스크롤) ──
		// Name 열 FillWidth → 창 크기에 맞게 늘어남
		// MinDesiredWidth → 스캔 후 최장 Target 경로 기반으로 동적 계산되어 SBox에 반영
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		.Padding(8.f, 4.f)
		[
			SNew(SScrollBox)
			.Orientation(Orient_Horizontal)
			.ConsumeMouseWheel(EConsumeMouseWheel::Never)
			+ SScrollBox::Slot()
			[
				SAssignNew(TableConstraintBox, SBox)
				.MinDesiredWidth(ComputedTableMinWidth)
				[
					SAssignNew(AssetListView, SListView<TSharedPtr<FAssetMoverEntry>>)
					.ListItemsSource(&EntryList)
					.OnGenerateRow(this, &SAssetMoverPanel::GenerateAssetRow)
					.HeaderRow(HeaderRow)
					.SelectionMode(ESelectionMode::None)
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[ SNew(SSeparator) ]

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
		[ SNew(SSeparator) ]

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
				SAssignNew(P4SubmitConfirmButton, SButton)
				.Text(this, &SAssetMoverPanel::GetP4SubmitConfirmButtonText)
				.ToolTipText(LOCTEXT("P4SubmitConfirmTooltip",
					"P4 Submit을 완료한 뒤 이 버튼을 눌러 Fix Up Redirectors를 활성화합니다.\n"
					"Submit 전에 Fix Up을 실행하면 Pending 변경목록의 파일을 Checkout하려다 실패합니다."))
				.IsEnabled(this, &SAssetMoverPanel::CanConfirmP4Submit)
				.OnClicked(this, &SAssetMoverPanel::OnP4SubmitConfirmClicked)
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
			.Padding(0.f, 0.f, 4.f, 0.f)
			[
				SAssignNew(CancelButton, SButton)
				.Text(LOCTEXT("Cancel", "취소"))
				.ToolTipText(LOCTEXT("CancelTooltip", "진행 중인 이동 작업을 취소합니다"))
				.IsEnabled(this, &SAssetMoverPanel::CanCancelMove)
				.OnClicked(this, &SAssetMoverPanel::OnCancelClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew(UndoButton, SButton)
				.Text(LOCTEXT("Undo", "↩ 실행 취소"))
				.ToolTipText(LOCTEXT("UndoTooltip",
					"마지막 이동 세션을 되돌립니다.\n"
					"에셋을 원래 경로로 복원하고 P4 Pending 변경을 Revert합니다."))
				.IsEnabled(this, &SAssetMoverPanel::CanUndo)
				.OnClicked(this, &SAssetMoverPanel::OnUndoClicked)
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
		Executor->OnUndoCompleted.RemoveAll(this);
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

// ── 이벤트 핸들러 ──────────────────────────────────────────────

FReply SAssetMoverPanel::OnScanClicked()
{
	if (SourceFolder.IsEmpty()) return FReply::Handled();

	EntryList.Empty();
	DependencyGraph = FDependencyGraph();
	CreatedRedirectorPaths.Empty();
	PendingRedirectorCount = 0;
	bP4SubmitConfirmed = false;
	bAssignmentDone = false;

	FAssetDependencyAnalyzer::AnalyzeFolder(SourceFolder, EntryList, DependencyGraph);
	FAssetMoveScheduler::ComputeMoveOrder(EntryList, DependencyGraph);
	SortEntries();
	RecalculateTargetPaths();
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

	const int32 SelCount = GetSelectedCount();
	const FText ConfirmMsg = FText::Format(
		LOCTEXT("MoveConfirm", "선택된 {0}개 에셋을 {1} 로 이동합니다.\n계속하시겠습니까?"),
		FText::AsNumber(SelCount),
		FText::FromString(TargetFolder)
	);
	if (FMessageDialog::Open(EAppMsgType::OkCancel, ConfirmMsg) != EAppReturnType::Ok)
	{
		return FReply::Handled();
	}

	TotalToMove = SelCount;
	bIsMoving = true;
	bP4SubmitConfirmed = false;
	ProgressValue = 0.f;

	Executor->MoveAssets(EntryList, TargetFolder);
	return FReply::Handled();
}

FReply SAssetMoverPanel::OnFixUpRedirectorsClicked()
{
	TArray<FString> AllRedirectors = CreatedRedirectorPaths;

	TArray<FString> SourceRedirectors;
	FAssetMoveExecutor::CollectRedirectors(SourceFolder, SourceRedirectors);
	AllRedirectors.Append(SourceRedirectors);

	TSet<FString> UniqueSet(AllRedirectors);
	AllRedirectors = UniqueSet.Array();

	Executor->FixUpRedirectors(AllRedirectors);

	CreatedRedirectorPaths.Empty();
	PendingRedirectorCount = 0;
	RefreshList();
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

FReply SAssetMoverPanel::OnP4SubmitConfirmClicked()
{
	bP4SubmitConfirmed = true;
	return FReply::Handled();
}

FReply SAssetMoverPanel::OnAssignWorkersClicked()
{
	if (EntryList.Num() == 0 || MaxWorkerCount <= 0) return FReply::Handled();

	FAssetWorkloadDistributor::AssignWorkers(EntryList, DependencyGraph, MaxWorkerCount);
	bAssignmentDone = true;

	// 내 담당이 아닌 항목은 선택 해제
	for (TSharedPtr<FAssetMoverEntry>& Entry : EntryList)
	{
		if (!IsEntryAssignedToMe(Entry))
		{
			Entry->bSelected = false;
		}
	}

	RefreshList();
	return FReply::Handled();
}

FReply SAssetMoverPanel::OnExportCSVClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform) return FReply::Handled();

	TArray<FString> OutFiles;
	const void* ParentWindow = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

	DesktopPlatform->SaveFileDialog(
		ParentWindow,
		TEXT("CSV 파일 저장"),
		FPaths::ProjectDir(),
		TEXT("asset_move_list.csv"),
		TEXT("CSV 파일 (*.csv)|*.csv"),
		EFileDialogFlags::None,
		OutFiles
	);

	if (OutFiles.Num() > 0)
	{
		FAssetCSVManager::ExportCSV(OutFiles[0], EntryList);
	}
	return FReply::Handled();
}

FReply SAssetMoverPanel::OnImportCSVClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform) return FReply::Handled();

	TArray<FString> OutFiles;
	const void* ParentWindow = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

	DesktopPlatform->OpenFileDialog(
		ParentWindow,
		TEXT("CSV 파일 열기"),
		FPaths::ProjectDir(),
		TEXT(""),
		TEXT("CSV 파일 (*.csv)|*.csv"),
		EFileDialogFlags::None,
		OutFiles
	);

	if (OutFiles.Num() > 0)
	{
		const int32 Updated = FAssetCSVManager::ImportCSV(OutFiles[0], EntryList);
		if (Updated > 0)
		{
			bAssignmentDone = true;
			// 내 담당이 아닌 항목 선택 해제
			for (TSharedPtr<FAssetMoverEntry>& Entry : EntryList)
			{
				if (!IsEntryAssignedToMe(Entry))
				{
					Entry->bSelected = false;
				}
			}
		}
		// CSV에서 가져온 TargetPath를 덮어쓰지 않도록 너비만 업데이트
		UpdateTableMinWidth();
		RefreshList();
	}
	return FReply::Handled();
}

void SAssetMoverPanel::OnMaxWorkerCountCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	const int32 Val = FCString::Atoi(*Text.ToString());
	MaxWorkerCount = FMath::Clamp(Val, 1, 20);
	if (MaxWorkerCountBox.IsValid())
	{
		MaxWorkerCountBox->SetText(FText::AsNumber(MaxWorkerCount));
	}
}

void SAssetMoverPanel::OnMyAssigneeIndexCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	// 1-based 입력 → 0-based 저장
	const int32 Val = FCString::Atoi(*Text.ToString());
	MyAssigneeIndex = FMath::Clamp(Val - 1, 0, 19);
	if (MyAssigneeIndexBox.IsValid())
	{
		MyAssigneeIndexBox->SetText(FText::AsNumber(MyAssigneeIndex + 1));
	}
	// 번호 변경 시 배정 필터 재적용
	if (bAssignmentDone)
	{
		for (TSharedPtr<FAssetMoverEntry>& Entry : EntryList)
		{
			Entry->bSelected = IsEntryAssignedToMe(Entry);
		}
		RefreshList();
	}
}

FReply SAssetMoverPanel::OnUndoClicked()
{
	if (!Executor.IsValid() || !Executor->CanUndo()) return FReply::Handled();

	const FText ConfirmMsg = LOCTEXT("UndoConfirm",
		"마지막 이동 세션을 되돌립니다.\n"
		"에셋이 원래 경로로 복원되고, P4 Pending 변경이 Revert됩니다.\n"
		"계속하시겠습니까?");

	if (FMessageDialog::Open(EAppMsgType::OkCancel, ConfirmMsg) != EAppReturnType::Ok)
	{
		return FReply::Handled();
	}

	Executor->UndoLastMoveSession();
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

	TSharedRef<SWindow> PickerWindow = SNew(SWindow)
		.Title(LOCTEXT("PickSourceFolder", "소스 폴더 선택"))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(400.f, 500.f));

	TSharedPtr<FString> SelectedPath = MakeShared<FString>(TEXT(""));
	TSharedPtr<FString> PickerCurrentPath = MakeShared<FString>(
		SourceFolder.IsEmpty() ? TEXT("/Game") : SourceFolder
	);

	FPathPickerConfig PathConfig;
	PathConfig.DefaultPath = SourceFolder.IsEmpty() ? TEXT("/Game") : SourceFolder;
	PathConfig.OnPathSelected = FOnPathSelected::CreateLambda(
		[PickerCurrentPath](const FString& Path)
		{
			*PickerCurrentPath = Path;
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
			.OnClicked(FOnClicked::CreateLambda([&PickerWindow, SelectedPath, PickerCurrentPath]()
			{
				*SelectedPath = *PickerCurrentPath;
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

	TSharedPtr<FString> SelectedPath = MakeShared<FString>(TEXT(""));
	TSharedPtr<FString> PickerCurrentPath = MakeShared<FString>(
		TargetFolder.IsEmpty() ? TEXT("/Game") : TargetFolder
	);

	FPathPickerConfig PathConfig;
	PathConfig.DefaultPath = TargetFolder.IsEmpty() ? TEXT("/Game") : TargetFolder;
	PathConfig.OnPathSelected = FOnPathSelected::CreateLambda(
		[PickerCurrentPath](const FString& Path)
		{
			*PickerCurrentPath = Path;
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
			.OnClicked(FOnClicked::CreateLambda([&PickerWindow, SelectedPath, PickerCurrentPath]()
			{
				*SelectedPath = *PickerCurrentPath;
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
	SortMode   = Mode;
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
	RecalculateTargetPaths();
	RefreshList();
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

// ── 쿼리 ────────────────────────────────────────────────────────

bool SAssetMoverPanel::CanMoveSelected() const
{
	if (bIsMoving || TargetFolder.IsEmpty()) return false;
	for (const TSharedPtr<FAssetMoverEntry>& Entry : EntryList)
	{
		if (Entry->bSelected
			&& Entry->Status == EMoveStatus::Pending
			&& IsEntryAssignedToMe(Entry))
		{
			return true;
		}
	}
	return false;
}

bool SAssetMoverPanel::CanFixUpRedirectors() const
{
	return !bIsMoving && PendingRedirectorCount > 0 && bP4SubmitConfirmed;
}

bool SAssetMoverPanel::CanConfirmP4Submit() const
{
	return !bIsMoving && PendingRedirectorCount > 0 && !bP4SubmitConfirmed;
}

bool SAssetMoverPanel::CanAssignWorkers() const
{
	return !bIsMoving && EntryList.Num() > 0 && MaxWorkerCount > 0;
}

bool SAssetMoverPanel::CanExportCSV() const
{
	return EntryList.Num() > 0;
}

bool SAssetMoverPanel::IsEntryAssignedToMe(TSharedPtr<FAssetMoverEntry> Entry) const
{
	if (!Entry.IsValid()) return false;
	if (!bAssignmentDone || Entry->AssigneeIndex < 0) return true;
	return Entry->AssigneeIndex == MyAssigneeIndex;
}

FText SAssetMoverPanel::GetP4SubmitConfirmButtonText() const
{
	return bP4SubmitConfirmed
		? LOCTEXT("P4SubmitConfirmed", "✔ Submit 완료됨")
		: LOCTEXT("P4SubmitConfirm",   "P4 Submit 완료");
}

bool SAssetMoverPanel::CanCancelMove() const
{
	return bIsMoving;
}

bool SAssetMoverPanel::CanUndo() const
{
	return !bIsMoving && Executor.IsValid() && Executor->CanUndo();
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

int32 SAssetMoverPanel::GetCyclicDepCount() const
{
	int32 Count = 0;
	for (const TSharedPtr<FAssetMoverEntry>& Entry : EntryList)
	{
		if (Entry->bHasCyclicDep) ++Count;
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
	if (SelCount == 0) return LOCTEXT("MoveSelected", "이동");
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

FText SAssetMoverPanel::GetCyclicWarningText() const
{
	return FText::Format(
		LOCTEXT("CyclicWarning", "⚠ {0}개 에셋에서 순환 의존성이 감지되었습니다. 마지막에 이동됩니다."),
		FText::AsNumber(GetCyclicDepCount())
	);
}

EVisibility SAssetMoverPanel::GetCyclicWarningVisibility() const
{
	return GetCyclicDepCount() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
}

// ── 내부 유틸 ────────────────────────────────────────────────────

void SAssetMoverPanel::RefreshList()
{
	if (AssetListView.IsValid())
	{
		AssetListView->RebuildList();
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

		FString NA = A->AssetData.AssetName.ToString();
		FString NB = B->AssetData.AssetName.ToString();
		return bAsc ? NA < NB : NA > NB;
	});
}

void SAssetMoverPanel::RecalculateTargetPaths()
{
	if (TargetFolder.IsEmpty() || SourceFolder.IsEmpty()) return;

	for (TSharedPtr<FAssetMoverEntry>& Entry : EntryList)
	{
		const FString PackagePath = Entry->AssetData.PackageName.ToString();
		FString RelPath;
		if (PackagePath.StartsWith(SourceFolder))
		{
			RelPath = PackagePath.RightChop(SourceFolder.Len());
			if (RelPath.StartsWith(TEXT("/")))
			{
				RelPath.RightChopInline(1);
			}
		}
		const FString SubFolder = FPaths::GetPath(RelPath);
		const FString AssetName = Entry->AssetData.AssetName.ToString();
		if (SubFolder.IsEmpty())
		{
			Entry->TargetPath = TargetFolder / AssetName;
		}
		else
		{
			Entry->TargetPath = TargetFolder / SubFolder / AssetName;
		}
	}

	UpdateTableMinWidth();
}

void SAssetMoverPanel::UpdateTableMinWidth()
{
	// 최장 Target 경로 픽셀 폭 추정 (7px/char) → MinDesiredWidth 동적 계산
	// 고정 열 합계: Checkbox(30) + Type(120) + Refs(90) + Deps(90) + Order(60) + Status(70) = 460
	// Name(150) + Target + Reason(150) + Assignee(80)
	float MaxTargetPx = 150.f;
	for (const TSharedPtr<FAssetMoverEntry>& Entry : EntryList)
	{
		MaxTargetPx = FMath::Max(MaxTargetPx, Entry->TargetPath.Len() * 7.f);
	}
	MaxTargetPx = FMath::Min(MaxTargetPx, 600.f);

	ComputedTableMinWidth = 460.f + 150.f + MaxTargetPx + 150.f + 80.f;

	if (TableConstraintBox.IsValid())
	{
		TableConstraintBox->SetMinDesiredWidth(FOptionalSize(ComputedTableMinWidth));
	}
}

void SAssetMoverPanel::BuildTableColumns(TSharedRef<SHeaderRow>& OutHeaderRow)
{
	OutHeaderRow->AddColumn(
		SHeaderRow::Column(SafeAssetMoverColumns::Checkbox)
		.DefaultLabel(FText::GetEmpty())
		.FixedWidth(30.f)
	);

	// FillWidth: 창 크기에 맞게 늘어남, 가로 스크롤은 MinDesiredWidth로 제어
	OutHeaderRow->AddColumn(
		SHeaderRow::Column(SafeAssetMoverColumns::Name)
		.DefaultLabel(LOCTEXT("ColName", "이름"))
		.FillWidth(1.f)
		.SortMode(this, &SAssetMoverPanel::GetColumnSortMode, SafeAssetMoverColumns::Name)
		.OnSort(this, &SAssetMoverPanel::OnSortColumnChanged)
	);

	OutHeaderRow->AddColumn(
		SHeaderRow::Column(SafeAssetMoverColumns::Type)
		.DefaultLabel(LOCTEXT("ColType", "타입"))
		.ManualWidth(120.f)
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

	OutHeaderRow->AddColumn(
		SHeaderRow::Column(SafeAssetMoverColumns::Target)
		.DefaultLabel(LOCTEXT("ColTarget", "대상 경로"))
		.ManualWidth(150.f)
		.HAlignCell(HAlign_Left)
		.HAlignHeader(HAlign_Left)
	);

	OutHeaderRow->AddColumn(
		SHeaderRow::Column(SafeAssetMoverColumns::Reason)
		.DefaultLabel(LOCTEXT("ColReason", "실패 사유"))
		.ManualWidth(150.f)
		.HAlignCell(HAlign_Left)
		.HAlignHeader(HAlign_Left)
	);

	OutHeaderRow->AddColumn(
		SHeaderRow::Column(SafeAssetMoverColumns::Assignee)
		.DefaultLabel(LOCTEXT("ColAssignee", "담당자"))
		.FixedWidth(80.f)
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
		.OnCheckStateChanged(this, &SAssetMoverPanel::OnAssetCheckboxChanged, Entry)
		.bIsAssignedToMe(this, &SAssetMoverPanel::IsEntryAssignedToMe, Entry)
		.OnTargetPathChanged(FSimpleDelegate::CreateSP(this, &SAssetMoverPanel::UpdateTableMinWidth));
}

void SAssetMoverPanel::OnAssetMovedCallback(const FAssetMoverEntry& Entry, bool bSuccess)
{
	if (bSuccess)
	{
		CreatedRedirectorPaths.AddUnique(Entry.SourcePath);
		PendingRedirectorCount = CreatedRedirectorPaths.Num();
	}

	int32 ProcessedCount = 0;
	for (const TSharedPtr<FAssetMoverEntry>& E : EntryList)
	{
		if (E->Status != EMoveStatus::Pending && E->Status != EMoveStatus::InProgress)
		{
			++ProcessedCount;
		}
	}

	if (TotalToMove > 0)
	{
		ProgressValue = static_cast<float>(ProcessedCount) / static_cast<float>(TotalToMove);
	}
	ProgressText = FText::Format(
		LOCTEXT("ProgressFmt", "{0} ({1}/{2})"),
		FText::FromName(Entry.AssetData.AssetName),
		FText::AsNumber(ProcessedCount),
		FText::AsNumber(TotalToMove)
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

	TArray<FString> FoundRedirectors;
	FAssetMoveExecutor::CollectRedirectors(SourceFolder, FoundRedirectors);
	PendingRedirectorCount = FoundRedirectors.Num();

	RefreshList();

	UE_LOG(LogSafeAssetMover, Log,
		TEXT("Move completed. %d assets moved. %d redirectors pending."),
		MovedCount, PendingRedirectorCount);
}

void SAssetMoverPanel::OnUndoCompletedCallback(int32 RestoredCount)
{
	for (TSharedPtr<FAssetMoverEntry>& Entry : EntryList)
	{
		if (Entry->Status == EMoveStatus::Moved
			|| Entry->Status == EMoveStatus::Failed
			|| Entry->Status == EMoveStatus::LockedByOther)
		{
			Entry->Status        = EMoveStatus::Pending;
			Entry->bSelected     = true;
			Entry->FailureReason = FString();
			Entry->CheckedOutBy  = FString();
		}
	}

	CreatedRedirectorPaths.Empty();
	PendingRedirectorCount = 0;
	bP4SubmitConfirmed = false;

	ProgressValue = 0.f;
	ProgressText  = FText::Format(
		LOCTEXT("UndoComplete", "실행 취소 완료: {0}개 에셋 복원됨"),
		FText::AsNumber(RestoredCount)
	);

	RefreshList();

	UE_LOG(LogSafeAssetMover, Log,
		TEXT("Undo completed. %d assets restored."), RestoredCount);
}

#undef LOCTEXT_NAMESPACE
