// Copyright FB Team. All Rights Reserved.

#include "SafeAssetMoverModule.h"
#include "UI/SafeAssetMoverWindow.h"
#include "ToolMenus.h"
#include "ContentBrowserModule.h"
#include "ContentBrowserDelegates.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "FSafeAssetMoverModule"

const FName FSafeAssetMoverModule::TabId = FSafeAssetMoverWindow::TabId;

void FSafeAssetMoverModule::StartupModule()
{
	FSafeAssetMoverWindow::Register();

	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FSafeAssetMoverModule::RegisterMenus)
	);
}

void FSafeAssetMoverModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FSafeAssetMoverWindow::Unregister();
}

void FSafeAssetMoverModule::RegisterMenus()
{
	// Tools 메뉴에 항목 추가
	FToolMenuOwnerScoped OwnerScoped(this);

	if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools"))
	{
		FToolMenuSection& Section = Menu->FindOrAddSection("SafeAssetMover");
		Section.Label = LOCTEXT("SafeAssetMoverSection", "Asset Management");

		Section.AddMenuEntry(
			"OpenSafeAssetMover",
			LOCTEXT("OpenMover", "Safe Asset Mover..."),
			LOCTEXT("OpenMoverTooltip", "의존성 순서로 안전하게 에셋을 이동합니다"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions.Move"),
			FUIAction(FExecuteAction::CreateLambda([]()
			{
				FSafeAssetMoverWindow::Open();
			}))
		);
	}

	// Content Browser 폴더 우클릭 컨텍스트 메뉴
	RegisterContentBrowserContextMenu();
}

void FSafeAssetMoverModule::RegisterContentBrowserContextMenu()
{
	if (!UToolMenus::Get()->IsMenuRegistered("ContentBrowser.FolderContextMenu"))
	{
		UToolMenus::Get()->RegisterMenu("ContentBrowser.FolderContextMenu");
	}

	UToolMenu* FolderMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.FolderContextMenu");
	if (!FolderMenu) return;

	FToolMenuSection& Section = FolderMenu->FindOrAddSection("SafeAssetMoverSection");
	Section.Label = LOCTEXT("SafeAssetMoverContextSection", "Safe Asset Mover");

	Section.AddDynamicEntry(
		"MoveFolderSafely",
		FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			// 선택된 폴더 경로 가져오기
			UContentBrowserDataMenuContext_FolderMenu* Context =
				InSection.FindContext<UContentBrowserDataMenuContext_FolderMenu>();

			TArray<FString> SelectedFolders;
			if (Context)
			{
				for (const FContentBrowserItem& Item : Context->SelectedItems)
				{
					FString VirtualPath = Item.GetVirtualPath().ToString();
					// /All/Game/... -> /Game/... 변환
					if (VirtualPath.StartsWith(TEXT("/All")))
					{
						VirtualPath = VirtualPath.RightChop(4);
					}
					SelectedFolders.Add(VirtualPath);
				}
			}

			FString FirstFolder = SelectedFolders.Num() > 0 ? SelectedFolders[0] : TEXT("");

			InSection.AddMenuEntry(
				"OpenSafeAssetMoverForFolder",
				LOCTEXT("MoveFolderSafely", "Move Folder Assets Safely..."),
				LOCTEXT("MoveFolderSafelyTooltip", "의존성 순서로 이 폴더의 에셋을 안전하게 이동합니다"),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions.Move"),
				FUIAction(FExecuteAction::CreateLambda([FirstFolder]()
				{
					FSafeAssetMoverWindow::Open(FirstFolder);
				}))
			);
		})
	);
}

TSharedRef<SDockTab> FSafeAssetMoverModule::OnSpawnTab(const FSpawnTabArgs& Args)
{
	return OnSpawnTabWithFolder(Args, TEXT(""));
}

TSharedRef<SDockTab> FSafeAssetMoverModule::OnSpawnTabWithFolder(const FSpawnTabArgs& Args, FString SourceFolder)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SBox)
			.MinDesiredWidth(700.f)
			.MinDesiredHeight(500.f)
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSafeAssetMoverModule, SafeAssetMover)
