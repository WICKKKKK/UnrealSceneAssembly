#pragma once

#include "BlockoutBaseGenerator.h"

#include "BlockoutArch.generated.h"

UCLASS(NotBlueprintable, BlueprintType, meta=(BlockoutPlaceable, DisplayName="Arch"))
class BLOCKOUT_API ABlockoutArch : public ABlockoutBaseGenerator
{
	GENERATED_BODY()

public:
	ABlockoutArch();
	virtual void CPPGenerateBlockoutMesh() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="角度", meta=(AllowPrivateAccess="true", ClampMin="0.01", ClampMax="360.0", UIMin="0.01", UIMax="360.0"))
	float Angle = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="细分", meta=(AllowPrivateAccess="true", ClampMin="2", UIMin="6", UIMax="50"))
	int32 Sections = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="X半径", meta=(AllowPrivateAccess="true", ClampMin="0.01", ClampMax="100000.0", UIMin="0.01", UIMax="10000.0"))
	float XRadius = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="Y半径", meta=(AllowPrivateAccess="true", ClampMin="0.01", ClampMax="100000.0", UIMin="0.01", UIMax="10000.0"))
	float YRadius = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="是否翻转", meta=(AllowPrivateAccess="true"))
	bool bReversed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="厚度", meta=(AllowPrivateAccess="true", ClampMin="0.01", UIMin="0.01", UIMax="200.0"))
	float Thickness = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="对齐方式", meta=(AllowPrivateAccess="true"))
	EBlockoutAlignment Alignment = EBlockoutAlignment::Center;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="进深", meta=(AllowPrivateAccess="true", ClampMin="0.01", UIMin="0.01"))
	float ExtrudeDistance = 100.0f;
};
