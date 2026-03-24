// Copyright FB Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FToolBarBuilder;
class FMenuBuilder;

class FSafeAssetMoverModule : public IModuleInterface
{
public:
	static const FName TabId;

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	void RegisterContentBrowserContextMenu();
	TSharedRef<class SDockTab> OnSpawnTab(const class FSpawnTabArgs& Args);
	TSharedRef<class SDockTab> OnSpawnTabWithFolder(const class FSpawnTabArgs& Args, FString SourceFolder);
};
