#pragma once

#include "BlockoutEnum.h"
#include "BlockoutStruct.h"
#include "Commons/BlockoutTypes.h"
#include "Components/BlockoutBoxComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GeometryActors/GeneratedDynamicMeshActor.h"

#include "BlockoutBaseDynamicMeshActor.generated.h"

struct FTextPlacementParams
{
	FRotator Rotation;
	FVector Offset;
	EHorizTextAligment HorizontalAlignment;
	EVerticalTextAligment VerticalAlignment;
};

class USceneSemanticComponent;

UENUM()
enum class EBlockoutBoundingBoxMode : uint8
{
	LocalBox UMETA(DisplayName="Local Box"),
	GeneratedWorldBox UMETA(DisplayName="Generated World Box"),
	SubtractiveWorldBox UMETA(DisplayName="Subtractive World Box"),
};

UCLASS(BlueprintType, Blueprintable, HideCategories=("Replication", "Collision", "Actor", "Input", "Cooking", "Rendering", "LOD", "DynamicMeshActor", "HLOD"))
class BLOCKOUT_API ABlockoutBaseDynamicMeshActor : public AGeneratedDynamicMeshActor
{
	GENERATED_BODY()

public:
	ABlockoutBaseDynamicMeshActor();

	virtual void ExecuteRebuildGeneratedMeshIfPending() override;

	UFUNCTION(BlueprintCallable, CallInEditor, Category="General")
	void UpdateCurrentBlockout();

	UFUNCTION(BlueprintCallable, Category="Blockout|Actor")
	virtual void UpdateCurrent(bool bForceRebuildBlockout, bool bForceRebuildInteractiveAffect, bool bRequestOverlappingBlockoutRebuild);

	UFUNCTION(BlueprintCallable, Category="Blockout|Actor")
	void RequestUpdateCurrent();

	UFUNCTION(CallInEditor, Category="General")
	virtual void UpdateAll();

	UFUNCTION(CallInEditor, Category="Debug")
	void ProfileAllBlockoutUpdate();

	UFUNCTION(BlueprintCallable, Category="Blockout|Export")
	void ExportToStaticMeshActor(FString AssetExportPath);

	UFUNCTION(BlueprintCallable, Category="Blockout|Export")
	bool ExportActorToLevel(ULevel* TargetLevel, FString AssetExportPath, TArray<AActor*>& OutActors, TArray<UObject*>& OutAssets);

	UFUNCTION(CallInEditor, Category="Export")
	void Export();

	UFUNCTION(CallInEditor, Category="Panel")
	void ShowBlockoutToolsPanel();

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category="General")
	bool UpdateBBox(const FBox& InLocalBox, const FTransform& InTransform, EBlockoutBoxAxis InMoveAxis);
	virtual void UpdateBBox_Imp(const FBox& InLocalBox, const FTransform& InTransform, EBlockoutBoxAxis InMoveAxis);

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category="General")
	void GetBBox(FBox& OutLocalBox, FTransform& OutTransform);
	void GetBBox_Imp(FBox& OutLocalBox, FTransform& OutTransform);

	void RebuildBlockoutMesh();
	void BlockoutLog(FString InLog);
	void RebuildInteractiveAffect();
	virtual void CreateBlockoutMesh();
	virtual void SetBlockoutProperties();

	static bool ValidateCurrentActor(ABlockoutBaseDynamicMeshActor* BlockoutActor, bool bUseSubtractive);
	TArray<ABlockoutBaseDynamicMeshActor*> GetOverlappingBlockoutActor(bool bUseSubtractiveTarget, bool bUseSubtractiveFound);
	void SubtractiveRequestOverlappingBlockoutRebuild();

	virtual void EditorApplyRotation(const FRotator& DeltaRotation, bool bAltDown, bool bShiftDown, bool bCtrlDown) override;
	virtual void EditorApplyScale(const FVector& DeltaScale, const FVector* PivotLocation, bool bAltDown, bool bShiftDown, bool bCtrlDown) override;
	virtual void EditorApplyTranslation(const FVector& DeltaTranslation, bool bAltDown, bool bShiftDown, bool bCtrlDown) override;

	void OverlappingBoolean();
	bool OverlappingDetection(ABlockoutBaseDynamicMeshActor* ActorA, bool bUseSubtractiveA, ABlockoutBaseDynamicMeshActor* ActorB, bool bUseSubtractiveB, float Tolerance);
	FBox CalMeshAABB(UDynamicMesh* InMesh, FTransform Transform = FTransform::Identity);

	UFUNCTION(BlueprintCallable, Category="Blockout|Material")
	void AssignCustomBlockoutMat(UDynamicMeshComponent* InDynamicMeshComp, FColor BlockoutColor, FColor GridColor, FBlockoutMaterialUVController InUVController);

	virtual void AssignBlockoutMat();
	virtual void AssignSubtractiveMat();
	virtual void CreatePropertyTextLabel();

	UFUNCTION(BlueprintCallable, Category="Blockout|Create")
	UPARAM(DisplayName="Text Component") UTextRenderComponent* CreateTextComp(FText InText, const FTransform& InTransform);

	void SetAllTextCompProperty(bool bVisible, float InTextSize, FColor InTextColor);
	FString CreateSingleUPropertyTextLabel(FProperty* Property);
	void InitializeTextPlacementLookupTable();
	void PlaceTextLabelOnCubicFace(UTextRenderComponent* TextComp);

	virtual void BeginPlay() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void Destroyed() override;
	virtual void PostEditUndo() override;
	virtual FName GetCustomIconName() const override;
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostActorCreated() override;
	virtual void PostEditImport() override;
	virtual void PostRegisterAllComponents() override;

	void SetBlockoutMaterialPresetType(EBlockoutMaterialPresetType InBlockoutMaterialPresetType)
	{
		BlockoutMaterialPresetType = InBlockoutMaterialPresetType;
	}

	UDynamicMeshComponent* GetGeneratedMeshComp() const { return GeneratedMeshComp; }
	FVector GetGeneratedMeshAABBMin() const { return GeneratedMeshAABB.Min; }
	FVector GetGeneratedMeshAABBMax() const { return GeneratedMeshAABB.Max; }
	FVector GetSubtractiveMeshAABBMin() const { return SubtractiveMeshAABB.Min; }
	FVector GetSubtractiveMeshAABBMax() const { return SubtractiveMeshAABB.Max; }

	void CreateBlockoutMaterialInstance();
	void SetActorHiddenInOutliner(bool bHiddenInOutliner);
	void SetActorHiddenInEditor(bool bHiddenInEditor);
	FBox GetBoxFromBoundingBoxComp();
	bool FindNearestFace(FBlockoutFace& OutTargetFace, FBlockoutFace& OutOtherFace, float& OutNearestDistance, FVector& OutProjectionPoint);

	static const FBlockoutMaterialColor& GetMaterialColor(EBlockoutMaterialPresetType InPresetType);
	const FBlockoutMaterialColor& GetCurrentMaterialColor() const;

private:
	UMaterialInterface* GetDefaultBlockoutMaterial() const;
	UMaterialInterface* GetSubtractiveMaterial() const;

protected:
	UPROPERTY(BlueprintReadOnly, Category="General", meta=(AllowPrivateAccess="true"))
	UDynamicMeshComponent* GeneratedMeshComp = nullptr;

	UPROPERTY(BlueprintReadWrite, Category="General", meta=(AllowPrivateAccess="true"))
	UDynamicMeshComponent* SubtractiveMeshComp = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="General", meta=(AllowPrivateAccess="true"))
	UBillboardComponent* BillboardComp = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="General", meta=(AllowPrivateAccess="true"))
	UBlockoutBoxComponent* BoundingBoxComp = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="General", meta=(AllowPrivateAccess="true"))
	UTextRenderComponent* MainTextComp = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scene Assembly|Semantic")
	TObjectPtr<USceneSemanticComponent> SemanticComponent = nullptr;

	UPROPERTY(BlueprintReadWrite, Category="Labeling")
	FString AllTextLabelString;

	UPROPERTY(BlueprintReadOnly, Category="General")
	FBox GeneratedMeshAABB = FBox(ForceInit);

	UPROPERTY(BlueprintReadOnly, Category="General")
	FBox SubtractiveMeshAABB = FBox(ForceInit);

	UPROPERTY(BlueprintReadOnly, Category="General")
	FBox MeshLocalAABB = FBox(ForceInit);

	UPROPERTY(BlueprintReadOnly, Category="Export")
	FString ExportedActorTag;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, DisplayName="Show in HUD?", Category="Editor")
	bool bShowHUD = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, DisplayName="Interactive Edit?", Category="Editor")
	bool bCanInteractiveEdit = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(EditCondition="bCanInteractiveEdit"), DisplayName="Interactive Mode", Category="Editor")
	EBlockoutInteractiveMode InteractiveMode = EBlockoutInteractiveMode::Box3D;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(EditCondition="bCanInteractiveEdit"), DisplayName="Uniform Scale?", Category="Editor")
	bool bUnitScale = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", DisplayName="Subtractive Mode", meta=(AllowPrivateAccess="true"))
	bool bSubtractive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", DisplayName="Use Bounding Box To Subtract", meta=(AllowPrivateAccess="true", EditCondition="bSubtractive==true"))
	bool bUseBoundingBoxToSubtract = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="General", AdvancedDisplay, DisplayName="Disable Subtractive Component", meta=(AllowPrivateAccess="true"))
	bool bDisableSubtractiveComp = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", DisplayName="Can Be Subtracted", meta=(AllowPrivateAccess="true"))
	bool bCanBeSubtracted = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", DisplayName="Show Bounding Box", meta=(AllowPrivateAccess="true"))
	bool bShowBoundingBox = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", DisplayName="Use Pivot Preset", meta=(AllowPrivateAccess="true"))
	bool bUsePivotPreset = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", DisplayName="Pivot Offset", meta=(AllowPrivateAccess="true", EditCondition="bUsePivotPreset==false"))
	FVector OffsetPivot = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", DisplayName="Pivot Preset Mode", meta=(AllowPrivateAccess="true", EditCondition="bUsePivotPreset==true", EditConditionHides))
	FBlockoutIntervalModeVector PivotOffsetMode = FBlockoutIntervalModeVector(EBlockoutIntervalMode::Min, EBlockoutIntervalMode::Min, EBlockoutIntervalMode::Min);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", DisplayName="Cast Shadows", meta=(AllowPrivateAccess="true"))
	bool bCastShadows = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", DisplayName="Enable Collisions", meta=(AllowPrivateAccess="true"))
	bool bEnableCollisions = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", DisplayName="Hidden In Game", meta=(AllowPrivateAccess="true"))
	bool bHiddenInGame = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", AdvancedDisplay, DisplayName="Enable Snapping", meta=(AllowPrivateAccess="true"))
	bool bEnableSnapping = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", AdvancedDisplay, DisplayName="Snap Threshold", meta=(AllowPrivateAccess="true", Units="cm"))
	float SnapThreshold = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", AdvancedDisplay, DisplayName="Angle Threshold", meta=(AllowPrivateAccess="true", Units="deg"))
	float AngleThreshold = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", AdvancedDisplay, DisplayName="Edge Snap Threshold", meta=(AllowPrivateAccess="true", Units="cm"))
	float EdgeSnapThreshold = 50.0f;

	FTransform SnapTransform = FTransform::Identity;
	bool bNeedUpdateSnapTransform = false;
	float DebugDuration = 0.05f;
	float DebugThickness = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Material", DisplayName="Custom Material?")
	bool bUseCustomMaterial = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="bUseCustomMaterial", EditConditionHides), Category="Material", DisplayName="Material")
	UMaterialInterface* CustomMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Material", meta=(EditCondition="!bUseCustomMaterial", EditConditionHides), AdvancedDisplay, DisplayName="Override Blueprint Material?")
	bool bApplyDefaultMaterial = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Material", DisplayName="Color Preset", meta=(AllowPrivateAccess="true", EditCondition="!bUseCustomMaterial && bApplyDefaultMaterial", EditConditionHides))
	EBlockoutMaterialPresetType BlockoutMaterialPresetType = EBlockoutMaterialPresetType::Orange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Material", DisplayName="UV Controller", meta=(AllowPrivateAccess="true", EditCondition="!bUseCustomMaterial && bApplyDefaultMaterial", EditConditionHides))
	FBlockoutMaterialUVController UVController = FBlockoutMaterialUVController(FBlockoutSingleUVController(false, 0.0f), FBlockoutSingleUVController(false, 0.0f), FBlockoutSingleUVController(true, 0.0f));

	UPROPERTY(Transient, DuplicateTransient)
	UMaterialInstanceDynamic* BlockoutMaterialInstance = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Labeling", DisplayName="Show Text Label", meta=(AllowPrivateAccess="true"))
	bool bShowTextLabel = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Labeling", DisplayName="Text Size", meta=(AllowPrivateAccess="true"))
	float TextSize = 16.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Labeling", DisplayName="Text Color", meta=(AllowPrivateAccess="true"))
	FColor TextColor = FColor::White;

	TMap<EBlockoutTextPlaceMode, TMap<EBlockoutHorizontalAlignment, TMap<EBlockoutVerticalAlignment, FTextPlacementParams>>> TextPlacementLookupTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Labeling", DisplayName="Place Mode", meta=(AllowPrivateAccess="true"))
	EBlockoutTextPlaceMode TextPlaceMode = EBlockoutTextPlaceMode::YZPositive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Labeling", DisplayName="Horizontal Alignment", meta=(AllowPrivateAccess="true"))
	EBlockoutHorizontalAlignment TextHorizontalAlignment = EBlockoutHorizontalAlignment::Center;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Labeling", DisplayName="Vertical Alignment", meta=(AllowPrivateAccess="true"))
	EBlockoutVerticalAlignment TextVerticalAlignment = EBlockoutVerticalAlignment::Center;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Labeling", DisplayName="Text Transform", meta=(AllowPrivateAccess="true"))
	FBlockoutTransform TextTransform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Export", DisplayName="Export Path", meta=(AllowPrivateAccess="true", LongPackageName))
	FDirectoryPath ExportPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Export", DisplayName="Export Materials", meta=(AllowPrivateAccess="true"))
	bool bExportMaterials = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Export", DisplayName="Export StaticMesh Asset", meta=(AllowPrivateAccess="true"))
	UStaticMesh* ExportStaticMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug", DisplayName="Debug Log", meta=(AllowPrivateAccess="true"))
	bool bShowDebugLog = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug", DisplayName="Bounding Box Mode", meta=(AllowPrivateAccess="true"))
	EBlockoutBoundingBoxMode BoundingBoxMode = EBlockoutBoundingBoxMode::LocalBox;

	bool bNeedRebuildBlockoutMesh = true;
	bool bNeedRebuildInteractiveAffect = true;
	bool bNeedRequestOverlappingBlockoutRebuild = false;
	FVector PreviousLocation;
	FRotator PreviousRotation;
	TArray<ABlockoutBaseDynamicMeshActor*> SubtractiveOverlappingBlockoutActors;
};
