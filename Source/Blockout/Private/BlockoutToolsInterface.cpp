#include "BlockoutToolsInterface.h"

#include "ActorFactories/ActorFactory.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BlockoutBaseDynamicMeshActor.h"
#include "BlockoutInstancer.h"
#include "BlockoutLog.h"
#include "BlockoutStruct.h"
#include "Building/BlockoutArbiSlopingRoof.h"
#include "Building/BlockoutHouse.h"
#include "DetailCustomization/BlockoutMultiPropertyCustomization.h"
#include "DetailCustomization/BlockoutSinglePropertyCustomization.h"
#include "Interfaces/IPluginManager.h"
#include "PropertyEditorModule.h"
#include "Shape/BlockoutBox.h"
#include "Shape/BlockoutPanel.h"
#include "Styling/SlateStyleRegistry.h"

#define LOCTEXT_NAMESPACE "FBlockoutToolsInterface"

TSharedPtr<FSlateStyleSet> FBlockoutToolsInterface::StyleSet = MakeShareable(new FSlateStyleSet("BlockoutStyle"));
FToolMenuSection* FBlockoutToolsInterface::BlockoutSection = nullptr;
FName FBlockoutToolsInterface::BlockoutCategoryName = TEXT("Blockout");
FString FBlockoutToolsInterface::BlockoutToolsContentAbsPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("UnrealSceneAssembly"))->GetBaseDir(), TEXT("Content"), TEXT("BlockoutTools")));
FString FBlockoutToolsInterface::BlockoutToolsContentPath = TEXT("/UnrealSceneAssembly/BlockoutTools/");

FBlockoutToolsInterface& FBlockoutToolsInterface::Get()
{
	static FBlockoutToolsInterface Instance;
	return Instance;
}

void FBlockoutToolsInterface::RegisterBlockoutPlacementMode()
{
	if (!IPlacementModeModule::IsAvailable())
	{
		return;
	}

	FPlacementCategoryInfo CategoryInfo(LOCTEXT("BlockoutCategory", "Blockout"), BlockoutCategoryName, TEXT("Blockout"), 65);
	IPlacementModeModule::Get().RegisterPlacementCategory(CategoryInfo);
}

void FBlockoutToolsInterface::UnregisterBlockoutPlacementMode()
{
	if (IPlacementModeModule::IsAvailable())
	{
		RemoveAllItemsFromBlockoutPlacementMode();
		IPlacementModeModule::Get().UnregisterPlacementCategory(BlockoutCategoryName);
	}
}

void FBlockoutToolsInterface::RegisterBlockoutDetailCustomizations()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	RegisteredStructNameArray.Empty();

	auto RegisterSingle = [&PropertyModule, this](UScriptStruct* Struct)
	{
		RegisteredStructNameArray.Add(Struct->GetFName());
		PropertyModule.RegisterCustomPropertyTypeLayout(RegisteredStructNameArray.Last(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FBlockoutSinglePropertyCustomization::MakeInstance));
	};
	auto RegisterMulti = [&PropertyModule, this](UScriptStruct* Struct)
	{
		RegisteredStructNameArray.Add(Struct->GetFName());
		PropertyModule.RegisterCustomPropertyTypeLayout(RegisteredStructNameArray.Last(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FBlockoutMultiPropertyCustomization::MakeInstance));
	};

	RegisterSingle(FBlockoutFloat::StaticStruct());
	RegisterSingle(FBlockoutInt::StaticStruct());
	RegisterSingle(FBlockoutBool::StaticStruct());
	RegisterSingle(FBlockoutFString::StaticStruct());
	RegisterMulti(FBlockoutFVector::StaticStruct());
	RegisterMulti(FBlockoutFVector4::StaticStruct());
	RegisterMulti(FBlockoutFVector2D::StaticStruct());
	RegisterMulti(FBlockoutFIntVector::StaticStruct());
	RegisterMulti(FBlockoutFIntVector2D::StaticStruct());
	RegisterMulti(FBlockoutIntervalModeVector::StaticStruct());
	RegisterMulti(FBlockoutTransform::StaticStruct());
	RegisterMulti(FBlockoutSingleUVController::StaticStruct());
	RegisterMulti(FBlockoutMaterialUVController::StaticStruct());
}

void FBlockoutToolsInterface::UnregisterBlockoutDetailCustomizations()
{
	if (!FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		return;
	}

	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	for (FName StructName : RegisteredStructNameArray)
	{
		PropertyModule.UnregisterCustomPropertyTypeLayout(StructName);
	}
	RegisteredStructNameArray.Empty();
}

void FBlockoutToolsInterface::RegisterBlockoutAssetEventsCallback()
{
	if (IPlacementModeModule::IsAvailable())
	{
		IPlacementModeModule::Get().OnPlacementModeCategoryRefreshed().AddRaw(this, &FBlockoutToolsInterface::RefreshBlockoutPlacementMode);
	}
}

void FBlockoutToolsInterface::RegisterBlockoutStyleSet()
{
	StyleSet = MakeShareable(new FSlateStyleSet("BlockoutStyle"));
	StyleSet->SetContentRoot(BlockoutToolsContentAbsPath);
	const FString IconPath = FPaths::Combine(BlockoutToolsContentAbsPath, TEXT("Icons"));
	StyleSet->Set("BlockoutShape.Thumbnail", new FSlateImageBrush(FPaths::Combine(IconPath, TEXT("Shape256.png")), FVector2D(64.0f, 64.0f)));
	StyleSet->Set("BlockoutInstancer.Thumbnail", new FSlateImageBrush(FPaths::Combine(IconPath, TEXT("Instancer256.png")), FVector2D(64.0f, 64.0f)));
	StyleSet->Set("Blockout.Icon", new FSlateImageBrush(FPaths::Combine(IconPath, TEXT("FalconIcon64.png")), FVector2D(16.0f, 16.0f)));
	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
}

void FBlockoutToolsInterface::UnregisterBlockoutStyleSet()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
		StyleSet.Reset();
	}
}

void FBlockoutToolsInterface::RegisterBlockoutInterface()
{
	RegisterBlockoutPlacementMode();
	RegisterBlockoutDetailCustomizations();
	RegisterBlockoutAssetEventsCallback();
	RegisterBlockoutStyleSet();
	RefreshBlockoutPlacementMode(BlockoutCategoryName);
}

void FBlockoutToolsInterface::UnregisterBlockoutInterface()
{
	UnregisterBlockoutPlacementMode();
	UnregisterBlockoutDetailCustomizations();
	UnregisterBlockoutStyleSet();
}

bool FBlockoutToolsInterface::GetBlueprintsFromBaseClass(UClass* InBaseClass, TArray<FAssetData>& FoundAssets)
{
	if (!IsValid(InBaseClass))
	{
		return false;
	}

	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TMultiMap<FName, FString> TagsValues;
	for (FThreadSafeObjectIterator It(InBaseClass); It; ++It)
	{
		const UClass* Class = It->GetClass();
		if (Class->IsNative() && Class->ClassDefaultObject == *It)
		{
			TagsValues.AddUnique(FBlueprintTags::NativeParentClassPath, FObjectPropertyBase::GetExportPath(Class, nullptr, nullptr, PPF_None));
		}
	}

	return AssetRegistryModule.Get().GetAssetsByTagValues(TagsValues, FoundAssets);
}

FPlacementModeID FBlockoutToolsInterface::AddItemToBlockoutPlacementMode(const FAssetData& AssetData)
{
	FString ClassName = AssetData.AssetName.ToString();
	UClass* ParentClass = nullptr;
	if (const FAssetTagValueRef ParentClassTag = AssetData.TagsAndValues.FindTag(FName(TEXT("NativeParentClass"))); ParentClassTag.IsSet())
	{
		ParentClass = StaticLoadClass(UObject::StaticClass(), nullptr, *ParentClassTag.GetValue());
	}
	if (UBlueprint* BlueprintAsset = Cast<UBlueprint>(AssetData.GetAsset()))
	{
		if (!BlueprintAsset->BlueprintDisplayName.IsEmpty())
		{
			ClassName = BlueprintAsset->BlueprintDisplayName;
		}
		ParentClass = BlueprintAsset->ParentClass;
	}

	const FName Thumbnail = ParentClass && ParentClass->IsChildOf(ABlockoutInstancer::StaticClass()) ? FName("BlockoutInstancer.Thumbnail") : FName("BlockoutShape.Thumbnail");
	TOptional<FPlacementModeID> ID = IPlacementModeModule::Get().RegisterPlaceableItem(BlockoutCategoryName, MakeShareable(new FPlaceableItem(
		*UActorFactory::StaticClass(), AssetData, Thumbnail, NAME_None, TOptional<FLinearColor>(), TOptional<int32>(), FText::FromString(ClassName))));
	return ID.GetValue();
}

void FBlockoutToolsInterface::AddClassToBlockoutPlacementMode(UClass* ActorClass, const FText& DisplayName, FName ThumbnailBrush)
{
	if (!IsValid(ActorClass) || !IPlacementModeModule::IsAvailable())
	{
		return;
	}

	FAssetData AssetData(ActorClass);
	TOptional<FPlacementModeID> ID = IPlacementModeModule::Get().RegisterPlaceableItem(BlockoutCategoryName, MakeShareable(new FPlaceableItem(
		*UActorFactory::StaticClass(), AssetData, ThumbnailBrush, NAME_None, TOptional<FLinearColor>(), TOptional<int32>(), DisplayName)));
	RegisteredIDs.Add(ID.GetValue());
}

void FBlockoutToolsInterface::RefreshBlockoutPlacementMode(FName CategoryName)
{
	if (CategoryName != BlockoutCategoryName || !IPlacementModeModule::IsAvailable())
	{
		return;
	}

	RemoveAllItemsFromBlockoutPlacementMode();
	AddClassToBlockoutPlacementMode(ABlockoutBox::StaticClass(), LOCTEXT("BlockoutBox", "Box"), FName("BlockoutShape.Thumbnail"));
	AddClassToBlockoutPlacementMode(ABlockoutPanel::StaticClass(), LOCTEXT("BlockoutPanel", "Panel"), FName("BlockoutShape.Thumbnail"));
	AddClassToBlockoutPlacementMode(ABlockoutHouse::StaticClass(), LOCTEXT("BlockoutHouse", "House"), FName("BlockoutShape.Thumbnail"));
	AddClassToBlockoutPlacementMode(ABlockoutArbiSlopingRoof::StaticClass(), LOCTEXT("BlockoutRoof", "Arbi Sloping Roof"), FName("BlockoutShape.Thumbnail"));
	AddClassToBlockoutPlacementMode(ABlockoutInstancer::StaticClass(), LOCTEXT("BlockoutInstancer", "Instancer"), FName("BlockoutInstancer.Thumbnail"));

	TArray<FAssetData> BlueprintAssets;
	if (GetBlueprintsFromBaseClass(ABlockoutBaseDynamicMeshActor::StaticClass(), BlueprintAssets))
	{
		for (const FAssetData& Asset : BlueprintAssets)
		{
			RegisteredIDs.Add(AddItemToBlockoutPlacementMode(Asset));
			RegisteredAssets.Add(Asset);
		}
	}
}

void FBlockoutToolsInterface::RemoveAllItemsFromBlockoutPlacementMode()
{
	for (FPlacementModeID& PlacementModeID : RegisteredIDs)
	{
		IPlacementModeModule::Get().UnregisterPlaceableItem(PlacementModeID);
	}
	RegisteredAssets.Empty();
	RegisteredIDs.Empty();
}

#undef LOCTEXT_NAMESPACE
