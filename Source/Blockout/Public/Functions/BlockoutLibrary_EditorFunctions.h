#pragma once

#include "BlockoutBaseDynamicMeshActor.h"
#include "BlockoutStruct.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "BlockoutLibrary_EditorFunctions.generated.h"

UCLASS()
class BLOCKOUT_API UBlockoutLibrary_EditorFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Blockout|Editor")
	static ULevel* GetLevelFromPackagePath(const FString& PackagePath, bool& bOutSuccess);

	UFUNCTION(BlueprintCallable, Category="Blockout|Editor")
	static ULevel* GetSublevelFromWorld(UWorld* World, FString SublevelName, bool& bOutSuccess);

	UFUNCTION(BlueprintCallable, Category="Blockout|Editor")
	static UPARAM(DisplayName="LevelVisibility") bool GetLevelVisibility(ULevel* Level);

	UFUNCTION(BlueprintCallable, Category="Blockout|Editor")
	static ULevelStreaming* GetStreaminglevelByActor(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category="Blockout|Editor")
	static void SelectTargetActors(TArray<AActor*> Actors);

	UFUNCTION(BlueprintCallable, Category="Blockout|Editor")
	static UPARAM(DisplayName="AllActorsOfTargetClass") TArray<AActor*> GetOrSelectAllActorsByClass(UClass* TargetClass, bool bAddToSelection = false);

	UFUNCTION(BlueprintCallable, Category="Blockout|Editor")
	static UPARAM(DisplayName="AllActorsWithTargetTag") TArray<AActor*> GetOrSelectAllActorsByTag(const FName& Tag, bool bAddToSelection = false);

	UFUNCTION(BlueprintCallable, Category="Blockout|Editor")
	static AActor* SpawnActorInSublevel(TSubclassOf<AActor> ActorClass, ULevel* Sublevel, FTransform InTransform, bool& bOutSuccess);

	UFUNCTION(BlueprintCallable, Category="Blockout|Editor")
	static void MoveActorsToSublevel(TArray<AActor*> Actors, ULevel* Sublevel, bool& bOutSuccess);

	UFUNCTION(BlueprintCallable, Category="Blockout|Editor")
	static UPARAM(DisplayName="DuplicatedActor") AActor* DuplicateActor(AActor* InActor);

	UFUNCTION(BlueprintCallable, Category="Blockout|Editor")
	static UPARAM(DisplayName="bReferenced") bool CheckPresetActorReferenced(AActor* PresetActor);

	UFUNCTION(BlueprintCallable, Category="Blockout|Editor")
	static void DestroyNoReferencedPresetActors();

	UFUNCTION(BlueprintCallable, Category="Blockout|Editor")
	static UPARAM(DisplayName="Selected Blockout Actors") TArray<ABlockoutBaseDynamicMeshActor*> GetSelectedBlockoutActors();

	UFUNCTION(BlueprintCallable, Category="Blockout|Editor")
	static void SetupStaticMeshComponent(UStaticMeshComponent*& OutMeshComp, UStaticMesh* InMesh, bool bEnableCollision, ECollisionTraceFlag CollisionTraceFlag);

	UFUNCTION(BlueprintCallable, Category="Blockout|Editor")
	static UPARAM(DisplayName="Created StaticMesh Component") UStaticMeshComponent* CreateStaticMeshComponetByClass(AActor* Actor, UStaticMesh* InMesh, const FTransform& InTransform, TSubclassOf<UStaticMeshComponent> Class, bool bEnableCollision, ECollisionTraceFlag CollisionTraceFlag);

	UFUNCTION(BlueprintCallable, Category="Blockout|Editor")
	static void CopyToStaticMeshComponent(UStaticMeshComponent* InMeshComp, UStaticMeshComponent* OutMeshComp);

	UFUNCTION(BlueprintCallable, Category="Blockout|Editor")
	static UPARAM(DisplayName="Copied StaticMesh Component") UStaticMeshComponent* CopyAndCreateStaticMeshComponet(AActor* Actor, UStaticMeshComponent* InMeshComp, FTransform InTransform);

	UFUNCTION(BlueprintCallable, Category="Blockout|Editor")
	static void DrawDebugBlockoutFace(const FBlockoutFace& Face, const FColor& Color, float Duration = 0.1f, float Thickness = 1.0f);

	UFUNCTION(BlueprintCallable, Category="Blockout|Editor")
	static void DrawDebugBlockoutBox(const FBox& Box, const FTransform& Transform, const FColor& Color, float Duration = 0.1f, bool bWireframe = true, float Thickness = 1.0f);

	UFUNCTION(BlueprintCallable, Category="Blockout|Editor")
	static void DrawDebugBlockoutArrow(const FVector& Start, const FVector& End, const FColor& Color, float Duration = 0.1f, float Size = 50.0f, float Thickness = 1.0f);

	UFUNCTION(BlueprintCallable, Category="Blockout|Editor")
	static void DrawDebugBlockoutPoint(const FVector& InPoint, const FColor& Color, float Duration = 0.1f, float Size = 10.0f);
};
