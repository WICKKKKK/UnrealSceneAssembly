#pragma once

#include "CoreMinimal.h"
#include "Types/SlateEnums.h"
#include "UI/SceneAssemblyTestSettings.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class FJsonObject;
struct FSlateBrush;
struct FSlateDynamicImageBrush;
class IDetailsView;

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
	void AppendLog(const FString& Message);
	void UpdateSelectionSummaryFromEditor();
	EActiveTimerReturnType RefreshSelectionTick(double InCurrentTime, float InDeltaTime);
	void LoadCaptureMetadataFromJson();
	void RefreshCaptureBrushes();
	void RefreshConceptBrush();
	bool IsBlockoutActor(const AActor* Actor) const;
	void CollectSelectedBlockoutActors(TArray<AActor*>& OutActors) const;

	FReply OnSelectAllWhiteboxesClicked();
	FReply OnDeselectClicked();
	FReply OnCaptureAestheticReferenceClicked();
	FReply OnJumpToCaptureCameraClicked();
	FReply OnUploadConceptArtClicked();
	FReply OnSolvePlaceClicked();
	FReply OnCleanupClicked();

	FText GetSelectionText() const;
	FText GetLastResultText() const;
	FText GetLogText() const;
	FText GetCaptureInfoText() const;
	FText GetConceptArtInfoText() const;
	const FSlateBrush* GetSceneCaptureBrush() const;
	const FSlateBrush* GetIdMapBrush() const;
	const FSlateBrush* GetConceptArtBrush() const;
	bool HasCaptureCamera() const;
	bool CanUploadConceptArt() const;
	bool CanRun() const;

	FString GetResultTag() const;

	TStrongObjectPtr<USceneAssemblyTestSettings> Settings;
	TSharedPtr<IDetailsView> SettingsDetailsView;
	TSharedPtr<FSlateDynamicImageBrush> SceneCaptureBrush;
	TSharedPtr<FSlateDynamicImageBrush> IdMapBrush;
	TSharedPtr<FSlateDynamicImageBrush> ConceptArtBrush;

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
	FString LastResult = TEXT("就绪。");
	FString LogText = TEXT("就绪。");
};
