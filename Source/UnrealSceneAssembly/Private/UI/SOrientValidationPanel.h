#pragma once

#include "CoreMinimal.h"
#include "Solver/SceneAssemblyTypes.h"
#include "Styling/SlateTypes.h"
#include "UI/OrientValidationSettings.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class FJsonObject;
class IDetailsView;
class UStaticMesh;
struct FSlateBrush;
struct FSlateDynamicImageBrush;

class SOrientValidationPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOrientValidationPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	struct FComputedRotation
	{
		bool bValid = false;
		FRotator WorldRotation = FRotator::ZeroRotator;
		FString RelativePoseText;
		FString MetadataText;
	};

	bool CallController(const FString& FunctionCall, TSharedPtr<FJsonObject>& OutObject);
	FString BuildPayloadJson() const;
	bool ComputeRotation(const TCHAR* ControllerFunctionName, const FText& ModeLabel, FComputedRotation& OutRotation);
	bool ApplyRotationResponse(const TSharedPtr<FJsonObject>& Response, FComputedRotation& OutRotation);
	FString BuildTimingReport(const TSharedPtr<FJsonObject>& Response, const FText& ModeLabel, double RoundTripMs) const;
	void UpdateSettingsResults();
	void LoadCaptureMetadataFromJson();
	void RefreshSceneBrush();
	void AppendLog(const FString& Message);
	FString GetTargetMeshAssetPath() const;
	FString GetTargetMeshName() const;
	FReply OpenContainingFolder(const FString& FilePath);
	AActor* SpawnResultActor(const FComputedRotation& RotationResult, const FString& LabelSuffix, const FVector& Location);

	FReply OnCaptureSceneClicked();
	FReply OnJumpToCaptureCameraClicked();
	FReply OnComputeDualImageClicked();
	FReply OnComputePrecomputedClicked();
	FReply OnSpawnDualImageClicked();
	FReply OnSpawnPrecomputedClicked();
	FReply OnCleanupClicked();
	FReply OnOpenCaptureFolderClicked();

	FText GetCaptureInfoText() const;
	FText GetLastResultText() const;
	FText GetLogText() const;
	const FSlateBrush* GetSceneBrush() const;
	bool HasCaptureCamera() const;
	bool HasSceneCapturePath() const;
	bool CanCompute() const;
	bool CanSpawnDualImage() const;
	bool CanSpawnPrecomputed() const;

	TStrongObjectPtr<UOrientValidationSettings> Settings;
	TSharedPtr<IDetailsView> SettingsDetailsView;
	TSharedPtr<FSlateDynamicImageBrush> SceneBrush;

	FString CaptureOutputDir;
	FString CaptureBaseName;
	FString CapturedSceneImagePath;
	FString CapturedJsonPath;
	bool bHasCaptureCamera = false;
	FVector CaptureCameraLocation = FVector::ZeroVector;
	FRotator CaptureCameraRotation = FRotator::ZeroRotator;
	float CaptureCameraFov = 90.0f;
	int32 CaptureImageWidth = 0;
	int32 CaptureImageHeight = 0;

	FComputedRotation DualImageResult;
	FComputedRotation PrecomputedResult;
	FString LastResult = TEXT("就绪。");
	FString LogText = TEXT("就绪。");
};
