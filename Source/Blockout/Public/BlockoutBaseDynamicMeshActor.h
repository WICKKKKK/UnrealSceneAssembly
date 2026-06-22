#pragma once

#include "BlockoutEnum.h"
#include "BlockoutStruct.h"
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
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostEditUndo() override;
	virtual FName GetCustomIconName() const override;
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostActorCreated() override;
	virtual void PostEditImport() override;
	virtual void PostLoad() override;
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
	void UpgradeToCurrentVersion(int32 LoadedVersion);

	int32 LoadedBlockoutActorVersion = INDEX_NONE;

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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, DisplayName="是否显示在HUD?", Category="Editor")
	bool bShowHUD = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, DisplayName="允许交互编辑?", Category="Editor")
	bool bCanInteractiveEdit = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(EditCondition="bCanInteractiveEdit"), DisplayName="交互模式?", Category="Editor")
	EBlockoutInteractiveMode InteractiveMode = EBlockoutInteractiveMode::Box3D;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(EditCondition="bCanInteractiveEdit"), DisplayName="统一缩放?", Category="Editor")
	bool bUnitScale = false;

	// 开启则会将当前生成的 Mesh 作为布尔减法白盒, 可以对场景中其他白盒对象进行布尔切割
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", DisplayName="开启布尔减法模式", meta=(AllowPrivateAccess="true"))
	bool bSubtractive = false;

	// 开启则使用包围盒进行布尔切割而不是使用 Mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", DisplayName="使用包围盒进行布尔减法", meta=(AllowPrivateAccess="true", EditCondition="bSubtractive==true"))
	bool bUseBoundingBoxToSubtract = false;

	// 禁用布尔减法组件的效果, 禁用后将不会对其他白盒对象进行布尔切割
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="General", AdvancedDisplay, DisplayName="禁用布尔减法组件", meta=(AllowPrivateAccess="true"))
	bool bDisableSubtractiveComp = false;

	// 是否可以被其他白盒对象进行布尔切割
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", DisplayName="是否可被布尔", meta=(AllowPrivateAccess="true"))
	bool bCanBeSubtracted = true;

	// 开启则会在白盒 Actor 上显示包围盒
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", DisplayName="显示包围盒", meta=(AllowPrivateAccess="true"))
	bool bShowBoundingBox = true;

	// 开启则会使用 Pivot 预设
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", DisplayName="使用Pivot预设", meta=(AllowPrivateAccess="true"))
	bool bUsePivotPreset = false;

	// Pivot 偏移
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", DisplayName="Pivot偏移", meta=(AllowPrivateAccess="true", EditCondition="bUsePivotPreset==false"))
	FVector OffsetPivot = FVector::ZeroVector;

	// Pivot 预设模式, 可以设置 Pivot 在 BoundingBox 不同面上的位置
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", DisplayName="Pivot预设模式", meta=(AllowPrivateAccess="true", EditCondition="bUsePivotPreset==true", EditConditionHides))
	FBlockoutIntervalModeVector PivotOffsetMode = FBlockoutIntervalModeVector(EBlockoutIntervalMode::Min, EBlockoutIntervalMode::Min, EBlockoutIntervalMode::Min);

	// 是否投射阴影
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", DisplayName="投射阴影", meta=(AllowPrivateAccess="true"))
	bool bCastShadows = true;

	// 是否开启碰撞
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", DisplayName="开启碰撞", meta=(AllowPrivateAccess="true"))
	bool bEnableCollisions = true;

	// 是否在 Game 模式下隐藏
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", DisplayName="Game模式隐藏", meta=(AllowPrivateAccess="true"))
	bool bHiddenInGame = false;

	// 是否启用自动吸附功能
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", AdvancedDisplay, DisplayName="启用自动吸附", meta=(AllowPrivateAccess="true"))
	bool bEnableSnapping = false;

	// 最大吸附检测距离
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", AdvancedDisplay, DisplayName="吸附距离阈值", meta=(AllowPrivateAccess="true", Units="cm"))
	float SnapThreshold = 50.0f;

	// 最大吸附角度（度）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", AdvancedDisplay, DisplayName="角度吸附阈值", meta=(AllowPrivateAccess="true", Units="deg"))
	float AngleThreshold = 10.0f;

	// 最大边缘吸附距离
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", AdvancedDisplay, DisplayName="边缘吸附距离阈值", meta=(AllowPrivateAccess="true", Units="cm"))
	float EdgeSnapThreshold = 50.0f;

	FTransform SnapTransform = FTransform::Identity;
	bool bNeedUpdateSnapTransform = false;
	float DebugDuration = 0.05f;
	float DebugThickness = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Material", DisplayName="自定义材质?")
	bool bUseCustomMaterial = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="bUseCustomMaterial", EditConditionHides), Category="Material", DisplayName="材质")
	UMaterialInterface* CustomMaterial = nullptr;

	// 开启后对当前白盒应用默认材质效果
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Material", meta=(EditCondition="!bUseCustomMaterial", EditConditionHides), AdvancedDisplay, DisplayName="覆盖蓝图材质?")
	bool bApplyDefaultMaterial = true;

	// 预设白盒材质类型, 可选 Orange | Blue | Green | Red | Grey | Dark
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Material", DisplayName="颜色预设", meta=(AllowPrivateAccess="true", EditCondition="!bUseCustomMaterial && bApplyDefaultMaterial", EditConditionHides))
	EBlockoutMaterialPresetType BlockoutMaterialPresetType = EBlockoutMaterialPresetType::Orange;

	// UV 控制器, 仅在白盒材质模式为 非自定义材质 时有效, 可以控制不同方向的 UV 翻转和偏移
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Material", DisplayName="UV控制器", meta=(AllowPrivateAccess="true", EditCondition="!bUseCustomMaterial && bApplyDefaultMaterial", EditConditionHides))
	FBlockoutMaterialUVController UVController = FBlockoutMaterialUVController(FBlockoutSingleUVController(false, 0.0f), FBlockoutSingleUVController(false, 0.0f), FBlockoutSingleUVController(true, 0.0f));

	UPROPERTY(Transient, DuplicateTransient)
	UMaterialInstanceDynamic* BlockoutMaterialInstance = nullptr;

	// 是否显示文本标注
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Labeling", DisplayName="是否显示文本标注", meta=(AllowPrivateAccess="true"))
	bool bShowTextLabel = true;

	// 文本标注字体大小
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Labeling", DisplayName="字体大小", meta=(AllowPrivateAccess="true"))
	float TextSize = 16.0f;

	// 文本标注字体颜色
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Labeling", DisplayName="字体颜色", meta=(AllowPrivateAccess="true"))
	FColor TextColor = FColor::White;

	TMap<EBlockoutTextPlaceMode, TMap<EBlockoutHorizontalAlignment, TMap<EBlockoutVerticalAlignment, FTextPlacementParams>>> TextPlacementLookupTable;

	// 文本标注摆放模式, 可以控制文本标注在 BoundingBox 不同面上的摆放位置
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Labeling", DisplayName="摆放模式", meta=(AllowPrivateAccess="true"))
	EBlockoutTextPlaceMode TextPlaceMode = EBlockoutTextPlaceMode::YZPositive;

	// 文本标注水平对齐方式
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Labeling", DisplayName="水平对齐方式", meta=(AllowPrivateAccess="true"))
	EBlockoutHorizontalAlignment TextHorizontalAlignment = EBlockoutHorizontalAlignment::Center;

	// 文本标注垂直对齐方式
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Labeling", DisplayName="垂直对齐方式", meta=(AllowPrivateAccess="true"))
	EBlockoutVerticalAlignment TextVerticalAlignment = EBlockoutVerticalAlignment::Center;

	// 文本标注后处理 Transform
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Labeling", DisplayName="文本标注后处理Transform", meta=(AllowPrivateAccess="true"))
	FBlockoutTransform TextTransform;

	// 导出路径
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Export", DisplayName="导出路径", meta=(AllowPrivateAccess="true", LongPackageName))
	FDirectoryPath ExportPath;

	// 开启则会在导出时将材质一并导出到同级目录下
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Export", DisplayName="导出材质", meta=(AllowPrivateAccess="true"))
	bool bExportMaterials = true;

	// 导出的 StaticMesh 资产
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Export", DisplayName="导出StaticMesh资产", meta=(AllowPrivateAccess="true"))
	UStaticMesh* ExportStaticMesh = nullptr;

	// 是否输出 Debug Log
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug", DisplayName="输出 DebugLog", meta=(AllowPrivateAccess="true"))
	bool bShowDebugLog = false;

	// 显示 BoundingBox 模式, 可以选择显示 LocalBox | GeneratedWorldBox | SubtractiveWorldBox
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug", DisplayName="显示BoundingBox模式", meta=(AllowPrivateAccess="true"))
	EBlockoutBoundingBoxMode BoundingBoxMode = EBlockoutBoundingBoxMode::LocalBox;

	bool bNeedRebuildBlockoutMesh = true;
	bool bNeedRebuildInteractiveAffect = true;
	bool bNeedRequestOverlappingBlockoutRebuild = false;
	FVector PreviousLocation;
	FRotator PreviousRotation;
	TArray<ABlockoutBaseDynamicMeshActor*> SubtractiveOverlappingBlockoutActors;
};
