#include "BlockoutSettings.h"

UBlockoutSettings::UBlockoutSettings()
{
	// 设置默认材质路径
	DefaultBlockoutMaterialPath = FSoftObjectPath(TEXT("/UnrealSceneAssembly/BlockoutTools/Materials/MI_Grid_1m_Orange.MI_Grid_1m_Orange"));
	DefaultSubtractiveMaterialPath = FSoftObjectPath(TEXT("/UnrealSceneAssembly/BlockoutTools/Materials/MI_Translucent_Blue.MI_Translucent_Blue"));
	DefaultExportPath.Path = FString("/Game/Blockout/_Generated_Meshes");
}

UMaterialInterface* UBlockoutSettings::GetBlockoutMaterial() const
{
	auto Material = Cast<UMaterialInterface>(DefaultBlockoutMaterialPath.TryLoad());
	if(!IsValid(Material))
		Material = LoadObject<UMaterialInterface>(nullptr,TEXT("/UnrealSceneAssembly/BlockoutTools/Materials/MI_Grid_1m_Orange.MI_Grid_1m_Orange"));
	return Material;
}

UMaterialInterface* UBlockoutSettings::GetSubtractiveMaterial() const
{
	auto Material = Cast<UMaterialInterface>(DefaultSubtractiveMaterialPath.TryLoad());
	if(!IsValid(Material))
		Material = LoadObject<UMaterialInterface>(nullptr,TEXT("/UnrealSceneAssembly/BlockoutTools/Materials/MI_Translucent_Blue.MI_Translucent_Blue"));
	return Material;
}
