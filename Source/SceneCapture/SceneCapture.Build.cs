using UnrealBuildTool;

public class SceneCapture : ModuleRules
{
	public SceneCapture(ReadOnlyTargetRules Target) : base(Target)
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
			"AssetRegistry",
			"UnrealEd",
			"Json",
			"ImageWrapper",
			"MaterialEditor",
			"Projects",
			"RHI",
			"RenderCore",
		});
	}
}
