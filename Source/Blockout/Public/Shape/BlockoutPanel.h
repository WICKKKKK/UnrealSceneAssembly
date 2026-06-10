#pragma once

#include "BlockoutBaseGenerator.h"
#include "GeometryScript/MeshBooleanFunctions.h"

#include "BlockoutPanel.generated.h"

UCLASS(BlueprintType, Blueprintable)
class BLOCKOUT_API ABlockoutPanel : public ABlockoutBaseGenerator
{
	GENERATED_BODY()

public:
	ABlockoutPanel();
	virtual void CPPGenerateBlockoutMesh() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", meta=(AllowPrivateAccess="true", MakeEditWidget="true", UIMin=0.01, ClampMin=0.01))
	FVector PanelSize = FVector(200.0f, 200.0f, 20.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", meta=(AllowPrivateAccess="true", UIMin=0, ClampMin=0))
	float CornerRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", meta=(AllowPrivateAccess="true", UIMin=3, UIMax=100, ClampMin=3))
	int32 CornerSubdivision = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Material", meta=(AllowPrivateAccess="true"))
	UMaterialInterface* MaterialPanel = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Topper", meta=(AllowPrivateAccess="true"))
	bool bUseTopper = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Topper", meta=(AllowPrivateAccess="true", UIMin=0.0f, ClampMin=0.0f, EditCondition="bUseTopper==true"))
	float TopperInsert = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Topper", meta=(AllowPrivateAccess="true", EditCondition="bUseTopper==true"))
	float TopperOffset = -10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Topper", meta=(AllowPrivateAccess="true", UIMin=0.0f, ClampMin=0.0f, EditCondition="bUseTopper==true"))
	float TopperExtrude = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Topper", meta=(AllowPrivateAccess="true", EditCondition="bUseTopper==true"))
	EGeometryScriptBooleanOperation BooleanOperator = EGeometryScriptBooleanOperation::Subtract;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Material", meta=(AllowPrivateAccess="true", EditCondition="bUseTopper==true"))
	UMaterialInterface* MaterialTopper = nullptr;
};
