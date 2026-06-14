using UnrealBuildTool;

public class UnrealSceneAssembly : ModuleRules
{
	public UnrealSceneAssembly(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"Slate",
			"SlateCore",
			"InputCore",
			"ToolMenus",
			"LevelEditor",
			"ApplicationCore",
			"Projects",
			"PythonScriptPlugin",
			"Json",
			"PropertyEditor",
			"ImageWrapper",
			"AssetRegistry",
			"RenderCore",
			"Renderer",
			"RHI",
			"SceneCapture",
			"DesktopPlatform",
		});
	}
}
