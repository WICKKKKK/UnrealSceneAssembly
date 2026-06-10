#pragma once

#include "Components/SplineComponent.h"

#include "BlockoutSplineComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSplineUpdatedSignature);

UCLASS(ClassGroup=Utility, ShowCategories = (Mobility), HideCategories = (Physics, Collision, Lighting, Rendering, Mobile),
	meta=(DisplayName="Blockout Spline", BlueprintSpawnableComponent))
class BLOCKOUT_API UBlockoutSplineComponent : public USplineComponent
{
	GENERATED_UCLASS_BODY()

public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	// // 当样条曲线更新时广播的委托
	// UPROPERTY(BlueprintAssignable, Category = "Spline")
	// FSplineUpdatedSignature OnSplineUpdated;

	// void NotifySplineComponentChanged();
};
