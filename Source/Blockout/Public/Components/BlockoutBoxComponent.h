#pragma once

#include "Components/BoxComponent.h"

#include "BlockoutBoxComponent.generated.h"


UCLASS(ClassGroup="Collision", hidecategories=(Object,LOD,Lighting,TextureStreaming), meta=(DisplayName="Blockout Box Collision", BlueprintSpawnableComponent))
class BLOCKOUT_API UBlockoutBoxComponent : public UBoxComponent
{
	GENERATED_UCLASS_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Blockout|Component")
	void SetLineThicknessCustom(float Thickness);
	
};
