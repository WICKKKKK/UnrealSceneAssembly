using UnrealBuildTool;

public class UnrealSceneAssembly : ModuleRules
{
	public UnrealSceneAssembly(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
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
			"ImageWrapper",
			"AssetRegistry",
			"RenderCore",
			"RHI",
		});
	}
}
