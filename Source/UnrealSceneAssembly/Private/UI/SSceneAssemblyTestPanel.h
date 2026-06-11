#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "Types/SlateEnums.h"
#include "UI/SceneAssemblyTestSettings.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class FJsonObject;
struct FSlateBrush;
struct FSlateDynamicImageBrush;
class IDetailsView;
class SVerticalBox;

struct FCropPreviewEntry
{
	FString ActorPath;
	FString ActorLabel;
	FString CropPath;
	FIntRect PixelBounds;
	TSharedPtr<FSlateDynamicImageBrush> Brush;
};

class SSceneAssemblyTestPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSceneAssemblyTestPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	bool CallController(const FString& FunctionCall, TSharedPtr<FJsonObject>& OutObject);
	FString BuildPayloadJson() const;
	void ApplyRunResponse(const TSharedPtr<FJsonObject>& Response);
	void ApplyAsyncStatusResponse(const TSharedPtr<FJsonObject>& Response);
	void AppendLog(const FString& Message);
	void UpdateSelectionSummaryFromEditor();
	EActiveTimerReturnType RefreshStatusTick(double InCurrentTime, float InDeltaTime);
	void LoadCaptureMetadataFromJson();
	void RefreshCaptureBrushes();
	void RefreshConceptBrush();
	void RebuildCropPreviews();
	void RefreshCropPreviewWidget();
	FString CropPreviewDirectory() const;
	FString CropPreviewPathForActor(const FString& ActorPath, int32 Index) const;
	bool IsBlockoutActor(const AActor* Actor) const;
	bool IsActorInViewFrustum(const AActor* Actor) const;
	void CollectSelectedBlockoutActors(TArray<AActor*>& OutActors) const;
	void CollectVisibleBlockoutActors(TArray<AActor*>& OutActors) const;
	bool CaptureAestheticReference(const TArray<AActor*>& TargetActors);
	FReply OpenContainingFolder(const FString& FilePath);

	FReply OnSelectAllWhiteboxesClicked();
	FReply OnSelectVisibleWhiteboxesClicked();
	FReply OnDeselectClicked();
	FReply OnCaptureAestheticReferenceClicked();
	FReply OnCaptureSelectedAestheticReferenceClicked();
	FReply OnJumpToCaptureCameraClicked();
	FReply OnUploadConceptArtClicked();
	FReply OnOpenSceneCaptureFolderClicked();
	FReply OnOpenIdMapFolderClicked();
	FReply OnOpenConceptArtFolderClicked();
	FReply OnRefreshCropPreviewsClicked();
	FReply OnSelectPreviewActorClicked(FString ActorPath);
	FReply OnFocusPreviewActorClicked(FString ActorPath);
	FReply OnSolvePlaceClicked();
	FReply OnCancelJobClicked();
	FReply OnCleanupClicked();

	FText GetSelectionText() const;
	FText GetLastResultText() const;
	FText GetLogText() const;
	FText GetCaptureInfoText() const;
	FText GetConceptArtInfoText() const;
	const FSlateBrush* GetSceneCaptureBrush() const;
	const FSlateBrush* GetIdMapBrush() const;
	const FSlateBrush* GetConceptArtBrush() const;
	FText GetCropPreviewSummaryText() const;
	bool HasCaptureCamera() const;
	bool HasSceneCapturePath() const;
	bool HasIdMapPath() const;
	bool HasConceptArtPath() const;
	bool CanUploadConceptArt() const;
	bool CanRun() const;
	bool CanCleanup() const;
	bool CanCancelJob() const;
	TOptional<float> GetJobProgress() const;
	FText GetJobProgressText() const;
	EVisibility GetJobProgressVisibility() const;

	FString GetResultTag() const;

	TStrongObjectPtr<USceneAssemblyTestSettings> Settings;
	TSharedPtr<IDetailsView> SettingsDetailsView;
	TSharedPtr<FSlateDynamicImageBrush> SceneCaptureBrush;
	TSharedPtr<FSlateDynamicImageBrush> IdMapBrush;
	TSharedPtr<FSlateDynamicImageBrush> ConceptArtBrush;
	TSharedPtr<SVerticalBox> CropPreviewContainer;
	TArray<TSharedPtr<FCropPreviewEntry>> CropPreviews;
	FProgressBarStyle JobProgressBarStyle;

	FString SelectionSummary = TEXT("已选 0 个 Actor（白盒 0 个）。");
	int32 SelectedCount = 0;
	int32 SelectedWhiteboxCount = 0;
	FString CaptureOutputDir;
	FString CaptureBaseName;
	FString CapturedSceneImagePath;
	FString CapturedIdMapPath;
	FString CapturedJsonPath;
	FString ConceptArtPath;
	bool bHasCaptureCamera = false;
	FVector CaptureCameraLocation = FVector::ZeroVector;
	FRotator CaptureCameraRotation = FRotator::ZeroRotator;
	float CaptureCameraFov = 90.0f;
	int32 CaptureImageWidth = 0;
	int32 CaptureImageHeight = 0;
	bool bJobRunning = false;
	FString JobState = TEXT("idle");
	int32 JobTotal = 0;
	int32 JobCompleted = 0;
	int32 JobSpawned = 0;
	int32 JobSucceeded = 0;
	int32 JobFailed = 0;
	FString LastResult = TEXT("就绪。");
	FString LogText = TEXT("就绪。");
};
