#pragma once

#include "BlockoutBaseGenerator.h"
#include "Components/SplineComponent.h"

#include "BlockoutHouse.generated.h"

UENUM(BlueprintType)
enum class EHouseRoofType : uint8
{
	SlopingRoof UMETA(DisplayName="Sloping Roof"),
	FlatRoof UMETA(DisplayName="Flat Roof"),
};

UCLASS(BlueprintType, Blueprintable)
class BLOCKOUT_API ABlockoutHouse : public ABlockoutBaseGenerator
{
	GENERATED_BODY()

public:
	ABlockoutHouse();
	virtual void CPPGenerateBlockoutMesh() override;

protected:
	UPROPERTY(Category="DynamicMeshActor", BlueprintReadWrite, meta=(AllowPrivateAccess="true"))
	USplineComponent* SplineComp = nullptr;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Floor", meta=(AllowPrivateAccess="true"))
	bool bGenerateFloor = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Floor", meta=(AllowPrivateAccess="true", MakeEditWidget, UIMin=0.0f, ClampMin=0.0f, EditCondition="bGenerateFloor==true", EditConditionHides))
	FVector BaseFloorThickness = FVector(0.0f, 0.0f, 20.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Floor", meta=(AllowPrivateAccess="true", EditCondition="bGenerateFloor==true", EditConditionHides))
	bool bGenerateMiddleFloor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Floor", meta=(AllowPrivateAccess="true", UIMin=0.01f, ClampMin=0.01f, EditCondition="bGenerateFloor==true && bGenerateMiddleFloor==true", EditConditionHides))
	float MiddleFloorThickness = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Floor", meta=(AllowPrivateAccess="true", UIMin=0.01f, ClampMin=0.01f, EditCondition="bGenerateFloor==true && bGenerateMiddleFloor==true", EditConditionHides))
	float FloorHeight = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Roof", meta=(AllowPrivateAccess="true"))
	bool bGenerateRoof = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Roof", meta=(AllowPrivateAccess="true", EditCondition="bGenerateRoof==true", EditConditionHides))
	EHouseRoofType RoofType = EHouseRoofType::SlopingRoof;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Roof", meta=(AllowPrivateAccess="true", UIMin=0.01f, ClampMin=0.01f, EditCondition="bGenerateRoof==true && RoofType==EHouseRoofType::SlopingRoof", EditConditionHides))
	float SlopingRoofHeight = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Roof", meta=(AllowPrivateAccess="true", UIMin=0.0f, UIMax=180.0f, EditCondition="bGenerateRoof==true && RoofType==EHouseRoofType::SlopingRoof", EditConditionHides))
	float RotateRidgeAngle = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Roof", meta=(AllowPrivateAccess="true", UIMin=0.01f, ClampMin=0.01f, EditCondition="bGenerateRoof==true && RoofType==EHouseRoofType::FlatRoof", EditConditionHides))
	float RoofThickness = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Roof", meta=(AllowPrivateAccess="true", EditCondition="bGenerateRoof==true && RoofType==EHouseRoofType::FlatRoof", EditConditionHides))
	bool bWithParapet = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Roof", meta=(AllowPrivateAccess="true", UIMin=0.01f, ClampMin=0.01f, EditCondition="bGenerateRoof==true && RoofType==EHouseRoofType::FlatRoof && bWithParapet==true", EditConditionHides))
	float ParapetHeight = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Wall", meta=(AllowPrivateAccess="true"))
	bool bGenerateWall = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Wall", meta=(AllowPrivateAccess="true", UIMin=0.01f, UIMax=100.0f, ClampMin=0.01f, EditCondition="bGenerateWall==true", EditConditionHides))
	float WallThickness = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Wall", meta=(AllowPrivateAccess="true", MakeEditWidget, UIMin=0.0f, ClampMin=0.0f, EditCondition="bGenerateWall==true", EditConditionHides))
	FVector WallHeight = FVector(0.0f, 0.0f, 700.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Wall", meta=(AllowPrivateAccess="true", EditCondition="bGenerateWall==true", EditConditionHides))
	bool bGenerateHoles = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Wall", meta=(AllowPrivateAccess="true", UIMin=0.1f, ClampMin=0.1f, EditCondition="bGenerateWall==true && bGenerateHoles==true", EditConditionHides))
	FVector HoleSize = FVector(100.0f, 100.0f, 150.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Wall", meta=(AllowPrivateAccess="true", UIMin=0.01f, ClampMin=0.01f, EditCondition="bGenerateWall==true && bGenerateHoles==true", EditConditionHides))
	float HoleInterval = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Wall", meta=(AllowPrivateAccess="true", EditCondition="bGenerateWall==true && bGenerateHoles==true", EditConditionHides))
	float HoleHeightOffset = 120.0f;
};
