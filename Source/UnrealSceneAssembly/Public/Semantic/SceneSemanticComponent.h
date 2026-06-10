#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SceneSemanticComponent.generated.h"

class AActor;

UCLASS(ClassGroup = (SceneAssembly), meta = (BlueprintSpawnableComponent))
class UNREALSCENEASSEMBLY_API USceneSemanticComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USceneSemanticComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly|Semantic")
	FString Category;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly|Semantic")
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly|Semantic")
	TArray<FString> Tags;

	UFUNCTION(BlueprintCallable, Category = "Scene Assembly|Semantic", meta = (DisplayName = "Find Scene Semantic Component"))
	static USceneSemanticComponent* FindSemanticComponent(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "Scene Assembly|Semantic", meta = (DisplayName = "Add Scene Semantic Component If Missing"))
	static USceneSemanticComponent* AddSemanticComponentIfMissing(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "Scene Assembly|Semantic", meta = (DisplayName = "Get Actor Semantic"))
	static bool GetActorSemantic(AActor* Actor, FString& OutCategory, FString& OutDescription, TArray<FString>& OutTags);

	UFUNCTION(BlueprintCallable, Category = "Scene Assembly|Semantic", meta = (DisplayName = "Set Actor Semantic"))
	static USceneSemanticComponent* SetActorSemantic(AActor* Actor, const FString& InCategory, const FString& InDescription, const TArray<FString>& InTags);
};
