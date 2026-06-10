#include "Semantic/SceneSemanticComponent.h"

#include "GameFramework/Actor.h"

USceneSemanticComponent::USceneSemanticComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

USceneSemanticComponent* USceneSemanticComponent::FindSemanticComponent(AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<USceneSemanticComponent>() : nullptr;
}

USceneSemanticComponent* USceneSemanticComponent::AddSemanticComponentIfMissing(AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	if (USceneSemanticComponent* ExistingComponent = FindSemanticComponent(Actor))
	{
		return ExistingComponent;
	}

	Actor->Modify();
	const FName ComponentName = MakeUniqueObjectName(Actor, USceneSemanticComponent::StaticClass(), TEXT("SceneSemanticComponent"));
	USceneSemanticComponent* Component = NewObject<USceneSemanticComponent>(Actor, USceneSemanticComponent::StaticClass(), ComponentName, RF_Transactional);
	if (!Component)
	{
		return nullptr;
	}

	Component->CreationMethod = EComponentCreationMethod::Instance;
	Actor->AddInstanceComponent(Component);
	Component->OnComponentCreated();
	Component->RegisterComponent();
	Actor->MarkPackageDirty();
	return Component;
}

bool USceneSemanticComponent::GetActorSemantic(AActor* Actor, FString& OutCategory, FString& OutDescription, TArray<FString>& OutTags)
{
	OutCategory.Reset();
	OutDescription.Reset();
	OutTags.Reset();

	const USceneSemanticComponent* Component = FindSemanticComponent(Actor);
	if (!Component)
	{
		return false;
	}

	OutCategory = Component->Category;
	OutDescription = Component->Description;
	OutTags = Component->Tags;
	return true;
}

USceneSemanticComponent* USceneSemanticComponent::SetActorSemantic(AActor* Actor, const FString& InCategory, const FString& InDescription, const TArray<FString>& InTags)
{
	USceneSemanticComponent* Component = AddSemanticComponentIfMissing(Actor);
	if (!Component)
	{
		return nullptr;
	}

	Component->Modify();
	Component->Category = InCategory;
	Component->Description = InDescription;
	Component->Tags = InTags;
	if (Actor)
	{
		Actor->MarkPackageDirty();
	}
	return Component;
}
