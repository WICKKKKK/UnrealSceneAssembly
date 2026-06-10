#include "Components/BlockoutSplineComponent.h"

#include "BlockoutBaseDynamicMeshActor.h"


UBlockoutSplineComponent::UBlockoutSplineComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBlockoutSplineComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// 广播事件
	// NotifySplineComponentChanged();

	ABlockoutBaseDynamicMeshActor* Owner = Cast<ABlockoutBaseDynamicMeshActor>(this->GetOwner());
	if (IsValid(Owner))
	{
		Owner->UpdateCurrent(true, true, true);
	}
}

// void UBlockoutSplineComponent::NotifySplineComponentChanged()
// {
// 	const bool CachedAllowEditorCall = GAllowActorScriptExecutionInEditor;
// 	GAllowActorScriptExecutionInEditor = true;
// 	if(OnSplineUpdated.IsBound())
// 		OnSplineUpdated.Broadcast();
// 	GAllowActorScriptExecutionInEditor = CachedAllowEditorCall;
// }

