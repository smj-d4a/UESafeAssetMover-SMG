// Copyright FB Team. All Rights Reserved.

#include "UI/SafeAssetMoverWindow.h"
#include "UI/SAssetMoverPanel.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "FSafeAssetMoverWindow"

const FName FSafeAssetMoverWindow::TabId = FName("SafeAssetMover");

static FString PendingSourceFolder;

void FSafeAssetMoverWindow::Register()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		TabId,
		FOnSpawnTab::CreateStatic(&FSafeAssetMoverWindow::SpawnTab)
	)
	.SetDisplayName(LOCTEXT("TabTitle", "Safe Asset Mover"))
	.SetTooltipText(LOCTEXT("TabTooltip", "의존성 순서로 안전하게 에셋을 이동합니다"))
	.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions.Move"));
}

void FSafeAssetMoverWindow::Unregister()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabId);
}

void FSafeAssetMoverWindow::Open(const FString& PresetSourceFolder)
{
	PendingSourceFolder = PresetSourceFolder;
	FGlobalTabmanager::Get()->TryInvokeTab(TabId);
}

TSharedRef<SDockTab> FSafeAssetMoverWindow::SpawnTab(const FSpawnTabArgs& Args)
{
	FString SourceFolder = PendingSourceFolder;
	PendingSourceFolder.Empty();

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SAssetMoverPanel)
			.InitialSourceFolder(SourceFolder)
		];
}

#undef LOCTEXT_NAMESPACE
