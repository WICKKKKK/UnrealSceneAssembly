#pragma once

#include "AssetRegistry/AssetData.h"
#include "CoreMinimal.h"
#include "IPlacementModeModule.h"
#include "Styling/SlateStyle.h"

class BLOCKOUT_API FBlockoutToolsInterface
{
public:
	~FBlockoutToolsInterface() = default;
	FBlockoutToolsInterface(const FBlockoutToolsInterface&) = delete;
	FBlockoutToolsInterface& operator=(const FBlockoutToolsInterface&) = delete;
	static FBlockoutToolsInterface& Get();

	void RegisterBlockoutPlacementMode();
	void UnregisterBlockoutPlacementMode();
	void SetupEntryPoints();
	FPlacementModeID AddItemToBlockoutPlacementMode(const FAssetData& AssetData);
	FPlacementModeID AddNativeClassToBlockoutPlacementMode(UClass* NativeClass);
	void GetNativePlaceableClassesFromBaseClass(UClass* InBaseClass, TArray<UClass*>& FoundClasses);
	void RemoveAllItemsFromBlockoutPlacementMode();
	void RefreshBlockoutPlacementMode(FName CategoryName);
	bool GetBlueprintsFromBaseClass(UClass* InBaseClass, TArray<FAssetData>& FoundAssets);
	void OpenBlockoutToolsPanel();

	void RegisterBlockoutDetailCustomizations();
	void UnregisterBlockoutDetailCustomizations();
	void RegisterBlockoutAssetEventsCallback();
	void RegisterBlockoutStyleSet();
	void UnregisterBlockoutStyleSet();
	void RegisterBlockoutInterface();
	void UnregisterBlockoutInterface();

	static TSharedPtr<FSlateStyleSet> StyleSet;
	static FToolMenuSection* BlockoutSection;
	static FName BlockoutCategoryName;
	static FString BlockoutToolsContentAbsPath;
	static FString BlockoutToolsContentPath;

private:
	FBlockoutToolsInterface() = default;

	TArray<FAssetData> RegisteredAssets;
	TArray<FPlacementModeID> RegisteredIDs;
	TArray<FName> RegisteredClassNameArray;
	TArray<FName> RegisteredStructNameArray;
};
