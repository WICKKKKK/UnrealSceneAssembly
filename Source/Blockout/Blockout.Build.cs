using UnrealBuildTool;

public class Blockout : ModuleRules
{
	public Blockout(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GeometryCore",
			"GeometryFramework",
			"GeometryScriptingCore",
			"GeometryScriptingEditor",
			"DynamicMesh",
			"MeshDescription",
			"MeshConversion",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealSceneAssembly",
			"UnrealEd",
			"Slate",
			"SlateCore",
			"InputCore",
			"ToolMenus",
			"LevelEditor",
			"PhysicsCore",
			"PlacementMode",
			"AssetRegistry",
			"DeveloperSettings",
			"EditorScriptingUtilities",
			"Projects",
			"PropertyEditor",
			"Blutility",
			"UMG",
			"UMGEditor",
			"ContentBrowser",
			"AssetTools",
		});
	}
}
