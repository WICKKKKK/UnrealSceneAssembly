#pragma once

#include "CoreMinimal.h"
#include "Solver/SceneAssemblyTypes.h"
#include "Styling/SlateTypes.h"
#include "UI/OrientValidationSettings.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class FJsonObject;
struct FPropertyChangedEvent;
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
	enum class EActiveOrientResult : uint8
	{
		None,
		SingleImage,
		DualImage,
	};

	struct FComputedRotation
	{
		bool bValid = false;
		FRotator WorldRotation = FRotator::ZeroRotator;
		FVector RefOrientPose = FVector::ZeroVector;
		FVector TargetOrientPose = FVector::ZeroVector;
		FRotator ThumbnailCameraRotation = FRotator::ZeroRotator;
		bool bHasRefOrientPose = false;
		bool bHasTargetOrientPose = false;
		bool bHasThumbnailCamera = false;
		FString MetadataText;
		// Symmetry branches (azimuth offsets from num_directions). Each entry is an
		// alternative world rotation; the user picks the one matching the concept art.
		TArray<FRotator> BranchRotations;
		TArray<int32> BranchAzimuthOffsets;
		TArray<FVector> BranchTargetOrientPoses;
	};

	bool CallController(const FString& FunctionCall, TSharedPtr<FJsonObject>& OutObject);
	FString BuildPayloadJson() const;
	bool ComputeRotation(const TCHAR* ControllerFunctionName, const FText& ModeLabel, FComputedRotation& OutRotation);
	bool ApplyRotationResponse(const TSharedPtr<FJsonObject>& Response, FComputedRotation& OutRotation);
	void RecomputeDualImageResultForCurrentSettings(bool bRedrawAxes);
	FString BuildSingleImagePayloadJson() const;
	int32 GetSingleImageBasisCandidateIndex() const;
	FString GetSingleImageBasisSummary() const;
	void DrawSingleImageAxes(bool bClearExisting);
	void DrawRotationResultAxes(const FComputedRotation& RotationResult, const FString& Label, bool bClearExisting);
	void DrawActiveAxes(bool bClearExisting);
	void SyncDualImageSelectedBranch();
	int32 GetSingleImageDirectionCount() const;
	double GetSingleImageDirectionAzimuthOffset() const;
	int32 GetDualImageDirectionCount() const;
	int32 GetDualImageAzimuthOffset() const;
	FRotator ComputeSingleImageWorldRotationForCurrentDirection() const;
	FString BuildAxesText(const FVector& FrontWorld, const FVector& RightWorld, const FVector& UpWorld) const;
	FString BuildSingleImageStatusText() const;
	FString BuildDualImageStatusText() const;
	void OnSettingsFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent);
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
	FReply OnComputeSingleImageClicked();
	FReply OnDrawSingleImageAxesClicked();
	FReply OnCycleSingleImageDirectionClicked();
	FReply OnClearSingleImageAxesClicked();
	FReply OnSpawnSingleImageClicked();
	FReply OnSpawnDualImageClicked();
	FReply OnCleanupClicked();
	FReply OnOpenCaptureFolderClicked();

	FText GetCaptureInfoText() const;
	FText GetSingleImageInfoText() const;
	FText GetLastResultText() const;
	FText GetLogText() const;
	const FSlateBrush* GetSceneBrush() const;
	bool HasCaptureCamera() const;
	bool HasSceneCapturePath() const;
	bool CanCompute() const;
	bool CanComputeSingleImage() const;
	bool CanDrawSingleImageAxes() const;
	bool CanDrawActiveAxes() const;
	bool CanSpawnSingleImage() const;
	bool CanSpawnDualImage() const;

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
	EActiveOrientResult ActiveResult = EActiveOrientResult::None;
	int32 DualImageDirectionIndex = 0;
	bool bHasSingleImageResult = false;
	FVector SingleImageOrientPose = FVector::ZeroVector;
	int32 SingleImageNumDirections = 1;
	int32 SingleImageDirectionIndex = 0;
	FRotator SingleImageWorldRotation = FRotator::ZeroRotator;
	FString SingleImageAxesText;
	FString DualImageAxesText;
	FString LastResult = TEXT("就绪。");
	FString LogText = TEXT("就绪。");
};
