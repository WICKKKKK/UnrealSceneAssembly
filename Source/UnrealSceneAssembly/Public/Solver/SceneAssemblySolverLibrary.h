#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Solver/SceneAssemblyTypes.h"
#include "SceneAssemblySolverLibrary.generated.h"

class AActor;

UCLASS()
class UNREALSCENEASSEMBLY_API USceneAssemblySolverLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Scene Assembly|Solver", meta = (DisplayName = "Get Actor OBB"))
	static FSceneOBB GetActorOBB(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "Scene Assembly|Solver", meta = (DisplayName = "Solve Asset Placement"))
	static TArray<FPlacementResult> SolvePlacement(const FSceneOBB& SceneOBB, const TArray<FAssetCandidate>& Candidates, const FSolverSettings& Settings);

	UFUNCTION(BlueprintCallable, Category = "Scene Assembly|Solver", meta = (DisplayName = "Resolve Image Orientation World Rotation"))
	static FRotator ResolveImageOrientationWorldRotation(const FAssetCandidate& Candidate, const FSolverSettings& Settings);

	UFUNCTION(BlueprintCallable, Category = "Scene Assembly|Solver", meta = (DisplayName = "Run Scene Assembly Solver Self Test"))
	static bool RunSolverSelfTest(float& OutFitIoU, FString& OutMessage);
};
