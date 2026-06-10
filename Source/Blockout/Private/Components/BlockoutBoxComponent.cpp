#include "Components/BlockoutBoxComponent.h"


UBlockoutBoxComponent::UBlockoutBoxComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LineThickness = 1.0f;
}

void UBlockoutBoxComponent::SetLineThicknessCustom(float Thickness)
{
	LineThickness = Thickness;
}