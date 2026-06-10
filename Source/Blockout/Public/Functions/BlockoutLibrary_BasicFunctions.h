#pragma once

#include "CoreMinimal.h"
#include "Components/DynamicMeshComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Materials/MaterialInstanceConstant.h"
#include "UDynamicMesh.h"

#include "BlockoutLibrary_BasicFunctions.generated.h"

UCLASS()
class BLOCKOUT_API UBlockoutLibrary_BasicFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Blockout|Basic")
	static UMaterialInterface* LoadBlockoutMaterial(const FString& Path);

	UFUNCTION(BlueprintCallable, Category="Blockout|Basic")
	static void ClearBlockoutMaterial(UDynamicMeshComponent* Comp);

	UFUNCTION(BlueprintCallable, Category="Blockout|Basic")
	static UMaterialInstanceConstant* ExportDynamicMaterialInstanceToMaterialInstance(
		UMaterialInstanceDynamic* DynamicMaterialInstance,
		const FString& SaveFolderPath,
		const FString& AssetName);

	UFUNCTION(BlueprintCallable, Category="Blockout|Basic")
	static bool IsDynamicMeshValid(UDynamicMesh* DynamicMesh);
};
