// Copyright FB Team. All Rights Reserved.

using UnrealBuildTool;

public class SafeAssetMover : ModuleRules
{
	public SafeAssetMover(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"AssetTools",
			"AssetRegistry",
			"ContentBrowser",
			"ContentBrowserData",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"InputCore",
			"EditorWidgets",
			"EditorSubsystem",
			"SourceControl",
			"DesktopPlatform",
		});
	}
}
