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
	FPlacementModeID AddItemToBlockoutPlacementMode(const FAssetData& AssetData);
	void AddClassToBlockoutPlacementMode(UClass* ActorClass, const FText& DisplayName, FName ThumbnailBrush);
	void RemoveAllItemsFromBlockoutPlacementMode();
	void RefreshBlockoutPlacementMode(FName CategoryName);
	bool GetBlueprintsFromBaseClass(UClass* InBaseClass, TArray<FAssetData>& FoundAssets);

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
