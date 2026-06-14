#include "ThumbnailLibrary.h"

#include "AssetCompilingManager.h"
#include "Animation/SkeletalMeshActor.h"
#include "CanvasTypes.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ContentStreaming.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "EngineModule.h"
#include "GameTime.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture.h"
#include "Engine/TextureCube.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "HAL/UnrealMemory.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "LegacyScreenPercentageDriver.h"
#include "Math/PerspectiveMatrix.h"
#include "Math/RotationMatrix.h"
#include "Math/UnrealMathUtility.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Misc/FileHelper.h"
#include "Misc/ObjectThumbnail.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "PreviewScene.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "RHIFeatureLevel.h"
#include "SceneInterface.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "SceneViewExtensionContext.h"
#include "ShaderCompiler.h"
#include "SkinnedAssetCompiler.h"
#include "StaticMeshCompiler.h"
#include "TextureResource.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "UObject/SoftObjectPath.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"

DEFINE_LOG_CATEGORY_STATIC(LogSceneAssemblyThumbnail, Log, All);

#if WITH_EDITOR
namespace
{
constexpr float ThumbnailDefaultFovDegrees = 30.0f;
constexpr float ThumbnailMinCameraDistance = 48.0f;
constexpr float ThumbnailTextureForceResidentSeconds = 30.0f;

FQuat ThumbnailCameraRotationFromOrbit(float OrbitPitch, float OrbitYaw)
{
	const FRotator RotationOffsetToViewCenter(0.0f, 90.0f, 0.0f);
	FMatrix ThumbnailViewRotation = FRotationMatrix(FRotator(0.0f, OrbitYaw, 0.0f))
		* FRotationMatrix(FRotator(0.0f, 0.0f, OrbitPitch))
		* FInverseRotationMatrix(RotationOffsetToViewCenter);
	ThumbnailViewRotation = ThumbnailViewRotation.RemoveTranslation();

	FMatrix CameraToLocal = ThumbnailViewRotation.InverseFast();
	CameraToLocal.RemoveScaling();
	FQuat Rotation(CameraToLocal);
	if (!Rotation.IsNormalized())
	{
		Rotation.Normalize();
	}
	return Rotation;
}

const USceneThumbnailInfo* ResolveSceneThumbnailInfo(UObject* Asset)
{
	if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset))
	{
		return Cast<USceneThumbnailInfo>(StaticMesh->ThumbnailInfo);
	}

	if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Asset))
	{
		return Cast<USceneThumbnailInfo>(SkeletalMesh->GetThumbnailInfo());
	}

	return nullptr;
}

const USceneThumbnailInfo* SceneThumbnailInfoOrDefault(UObject* Asset)
{
	const USceneThumbnailInfo* ThumbnailInfo = ResolveSceneThumbnailInfo(Asset);
	if (!ThumbnailInfo)
	{
		ThumbnailInfo = GetDefault<USceneThumbnailInfo>();
	}
	return ThumbnailInfo;
}

bool ResolveThumbnailCameraRotation(UObject* Asset, FRotator& OutCameraRotation)
{
	const USceneThumbnailInfo* ThumbnailInfo = SceneThumbnailInfoOrDefault(Asset);
	if (!ThumbnailInfo || (!Asset->IsA<UStaticMesh>() && !Asset->IsA<USkeletalMesh>()))
	{
		return false;
	}

	OutCameraRotation = ThumbnailCameraRotationFromOrbit(ThumbnailInfo->OrbitPitch, ThumbnailInfo->OrbitYaw).Rotator();
	return true;
}

FThumbnailCaptureOptions NormalizeCaptureOptions(const FThumbnailCaptureOptions& CaptureOptions, int32 Resolution)
{
	FThumbnailCaptureOptions NormalizedOptions = CaptureOptions;
	NormalizedOptions.Resolution = CaptureOptions.Resolution > 0 ? CaptureOptions.Resolution : Resolution;
	NormalizedOptions.Resolution = FMath::Clamp(NormalizedOptions.Resolution, 64, 2048);
	NormalizedOptions.FovDegrees = CaptureOptions.FovDegrees > 0.0f
		? FMath::Clamp(CaptureOptions.FovDegrees, 1.0f, 170.0f)
		: ThumbnailDefaultFovDegrees;
	return NormalizedOptions;
}

void ResolveThumbnailOrbit(UObject* MeshAsset, const FThumbnailCaptureOptions& CaptureOptions, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom)
{
	if (CaptureOptions.bOverrideCamera)
	{
		OutOrbitPitch = CaptureOptions.OrbitPitch;
		OutOrbitYaw = CaptureOptions.OrbitYaw;
		OutOrbitZoom = CaptureOptions.OrbitZoom;
		return;
	}

	const USceneThumbnailInfo* ThumbnailInfo = SceneThumbnailInfoOrDefault(MeshAsset);
	OutOrbitPitch = ThumbnailInfo ? ThumbnailInfo->OrbitPitch : 0.0f;
	OutOrbitYaw = ThumbnailInfo ? ThumbnailInfo->OrbitYaw : 0.0f;
	OutOrbitZoom = ThumbnailInfo ? ThumbnailInfo->OrbitZoom : 0.0f;
}

float ThumbnailBoundsZOffset(const FBoxSphereBounds& Bounds)
{
	return static_cast<float>(Bounds.BoxExtent.Z + 1.0);
}

bool ComputeThumbnailCameraView(
	UObject* MeshAsset,
	const FBoxSphereBounds& Bounds,
	FVector& OutLocation,
	FRotator& OutRotation,
	float& OutFovDegrees,
	const FThumbnailCaptureOptions& CaptureOptions)
{
	OutFovDegrees = CaptureOptions.FovDegrees;
	const float HalfFovRadians = FMath::DegreesToRadians<float>(OutFovDegrees) * 0.5f;
	if (HalfFovRadians <= 0.0f || FMath::IsNearlyZero(FMath::Tan(HalfFovRadians)))
	{
		return false;
	}

	const bool bIsStaticMesh = MeshAsset->IsA<UStaticMesh>();
	const float BoundsScale = bIsStaticMesh ? 1.15f : 1.0f;
	const float HalfMeshSize = static_cast<float>(Bounds.SphereRadius * BoundsScale);
	const float TargetDistance = HalfMeshSize / FMath::Tan(HalfFovRadians);
	const float BoundsZOffset = ThumbnailBoundsZOffset(Bounds);

	FVector Origin(0.0f, 0.0f, -BoundsZOffset);
	float OrbitPitch = 0.0f;
	float OrbitYaw = 0.0f;
	float OrbitZoomOffset = 0.0f;
	ResolveThumbnailOrbit(MeshAsset, CaptureOptions, OrbitPitch, OrbitYaw, OrbitZoomOffset);
	const float OrbitZoom = FMath::Max(ThumbnailMinCameraDistance, TargetDistance + OrbitZoomOffset);

	const FRotator RotationOffsetToViewCenter(0.0f, 90.0f, 0.0f);
	FMatrix ViewRotationMatrix = FRotationMatrix(FRotator(0.0f, OrbitYaw, 0.0f))
		* FRotationMatrix(FRotator(0.0f, 0.0f, OrbitPitch))
		* FTranslationMatrix(FVector(0.0f, OrbitZoom, 0.0f))
		* FInverseRotationMatrix(RotationOffsetToViewCenter);

	ViewRotationMatrix = ViewRotationMatrix * FMatrix(
		FPlane(0.0f, 0.0f, 1.0f, 0.0f),
		FPlane(1.0f, 0.0f, 0.0f, 0.0f),
		FPlane(0.0f, 1.0f, 0.0f, 0.0f),
		FPlane(0.0f, 0.0f, 0.0f, 1.0f));

	Origin -= ViewRotationMatrix.InverseTransformPosition(FVector::ZeroVector);
	OutLocation = -Origin;
	OutRotation = ThumbnailCameraRotationFromOrbit(OrbitPitch, OrbitYaw).Rotator();
	return true;
}

void RestoreForcedTextureMips(const TArray<UTexture*>& ForcedTextures)
{
	for (UTexture* Texture : ForcedTextures)
	{
		if (Texture)
		{
			Texture->SetForceMipLevelsToBeResident(-1.0f);
		}
	}
}

void ReleaseMeshTextureForceResidentFlag(UMeshComponent* MeshComponent)
{
	if (MeshComponent)
	{
		MeshComponent->SetTextureForceResidentFlag(false);
	}
}

void RestoreMeshStreamingState(UMeshComponent* MeshComponent, const TArray<UTexture*>& ForcedTextures)
{
	ReleaseMeshTextureForceResidentFlag(MeshComponent);
	RestoreForcedTextureMips(ForcedTextures);
}

void ForceComponentTexturesResident(UMeshComponent* MeshComponent, TArray<UTexture*>& ForcedTextures)
{
	if (!MeshComponent)
	{
		return;
	}

	MeshComponent->PrestreamTextures(ThumbnailTextureForceResidentSeconds, false, 0);
	MeshComponent->SetTextureForceResidentFlag(true);

	TArray<UTexture*> UsedTextures;
	MeshComponent->GetUsedTextures(UsedTextures, EMaterialQualityLevel::Num);
	for (UTexture* Texture : UsedTextures)
	{
		if (!Texture || ForcedTextures.Contains(Texture))
		{
			continue;
		}

		Texture->SetForceMipLevelsToBeResident(ThumbnailTextureForceResidentSeconds);
		Texture->WaitForStreaming();
		ForcedTextures.Add(Texture);
	}
}

void EnsureMeshMaterialsComplete(UMeshComponent* MeshComponent)
{
	if (!MeshComponent)
	{
		return;
	}

	const int32 MaterialCount = MeshComponent->GetNumMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		UMaterialInterface* Material = MeshComponent->GetMaterial(MaterialIndex);
		if (!Material)
		{
			continue;
		}

		UMaterial* BaseMaterial = Material->GetMaterial();
		if (BaseMaterial)
		{
			BaseMaterial->EnsureIsComplete();
		}
		Material->EnsureIsComplete();
	}
	FlushRenderingCommands();
}

bool EncodePngFile(const FString& OutPngPath, const TArray<FColor>& Pixels, int32 Width, int32 Height)
{
	if (Width <= 0 || Height <= 0 || Pixels.Num() < Width * Height)
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Invalid PNG pixel data for %s (%dx%d, pixels=%d)."), *OutPngPath, Width, Height, Pixels.Num());
		return false;
	}

	const FString OutputDirectory = FPaths::GetPath(OutPngPath);
	if (!OutputDirectory.IsEmpty() && !IFileManager::Get().MakeDirectory(*OutputDirectory, true))
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Failed to create thumbnail directory: %s"), *OutputDirectory);
		return false;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	const TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!ImageWrapper.IsValid())
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Failed to create PNG image wrapper."));
		return false;
	}

	if (!ImageWrapper->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8))
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Failed to encode thumbnail data for %s."), *OutPngPath);
		return false;
	}

	const TArray64<uint8>& CompressedData = ImageWrapper->GetCompressed(100);
	if (CompressedData.Num() <= 0 || CompressedData.Num() > MAX_int32)
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Invalid compressed PNG size for %s."), *OutPngPath);
		return false;
	}

	TArray<uint8> PngBytes;
	PngBytes.SetNumUninitialized(static_cast<int32>(CompressedData.Num()));
	FMemory::Memcpy(PngBytes.GetData(), CompressedData.GetData(), static_cast<SIZE_T>(CompressedData.Num()));

	if (!FFileHelper::SaveArrayToFile(PngBytes, *OutPngPath))
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Failed to save thumbnail: %s"), *OutPngPath);
		return false;
	}

	return true;
}

UTextureRenderTarget2D* CreateThumbnailRenderTarget(int32 ThumbnailSize, const FLinearColor& BackgroundColor)
{
	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient);
	if (!RenderTarget)
	{
		return nullptr;
	}

	RenderTarget->ClearColor = BackgroundColor;
	RenderTarget->TargetGamma = GEngine ? GEngine->DisplayGamma : 2.2f;
	RenderTarget->RenderTargetFormat = RTF_RGBA8;
	RenderTarget->InitAutoFormat(ThumbnailSize, ThumbnailSize);
	RenderTarget->UpdateResourceImmediate(true);
	return RenderTarget;
}

FSceneView* CreateThumbnailSceneView(
	FSceneViewFamily* ViewFamily,
	const FVector& CameraLocation,
	const FRotator& CameraRotation,
	float FovDegrees,
	int32 ThumbnailSize,
	const FLinearColor& BackgroundColor)
{
	if (!ViewFamily || ThumbnailSize <= 0)
	{
		return nullptr;
	}

	const FIntRect ViewRect(0, 0, ThumbnailSize, ThumbnailSize);
	const float HalfFovRadians = FMath::DegreesToRadians<float>(FovDegrees) * 0.5f;
	static_assert((int32)ERHIZBuffer::IsInverted != 0, "Check NearPlane and Projection Matrix");
	constexpr float NearPlane = 1.0f;

	FMatrix ViewRotationMatrix = FInverseRotationMatrix(CameraRotation);
	ViewRotationMatrix = ViewRotationMatrix * FMatrix(
		FPlane(0.0f, 0.0f, 1.0f, 0.0f),
		FPlane(1.0f, 0.0f, 0.0f, 0.0f),
		FPlane(0.0f, 1.0f, 0.0f, 0.0f),
		FPlane(0.0f, 0.0f, 0.0f, 1.0f));

	FSceneViewInitOptions ViewInitOptions;
	ViewInitOptions.ViewFamily = ViewFamily;
	ViewInitOptions.SetViewRectangle(ViewRect);
	ViewInitOptions.ViewOrigin = CameraLocation;
	ViewInitOptions.ViewRotationMatrix = ViewRotationMatrix;
	ViewInitOptions.ProjectionMatrix = FReversedZPerspectiveMatrix(HalfFovRadians, 1.0f, 1.0f, NearPlane);
	ViewInitOptions.BackgroundColor = BackgroundColor;

	FSceneView* View = new FSceneView(ViewInitOptions);
	ViewFamily->Views.Add(View);
	View->StartFinalPostprocessSettings(ViewInitOptions.ViewOrigin);
	View->EndFinalPostprocessSettings(ViewInitOptions);

	const float ScreenSize = static_cast<float>(ThumbnailSize);
	const float FovScreenSize = static_cast<float>(ThumbnailSize) / FMath::Tan(FMath::DegreesToRadians<float>(FovDegrees));
	IStreamingManager::Get().AddViewInformation(CameraLocation, ScreenSize, FovScreenSize);
	return View;
}

bool RenderThumbnailPreviewScene(
	FPreviewScene& PreviewScene,
	UTextureRenderTarget2D* RenderTarget,
	const FVector& CameraLocation,
	const FRotator& CameraRotation,
	float CameraFovDegrees,
	int32 ThumbnailSize,
	const FLinearColor& BackgroundColor)
{
	FTextureRenderTargetResource* RenderTargetResource = RenderTarget ? RenderTarget->GameThread_GetRenderTargetResource() : nullptr;
	if (!RenderTargetResource)
	{
		return false;
	}

	const FGameTime ThumbnailTime = FGameTime::GetTimeSinceAppStart();
	FCanvas Canvas(RenderTargetResource, nullptr, ThumbnailTime, GMaxRHIFeatureLevel);
	Canvas.Clear(BackgroundColor);

	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		RenderTargetResource,
		PreviewScene.GetScene(),
		FEngineShowFlags(ESFIM_Game))
		.SetTime(ThumbnailTime)
		.SetRealtimeUpdate(false));

	ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
	ViewFamily.EngineShowFlags.MotionBlur = 0;
	ViewFamily.EngineShowFlags.LOD = 0;

	FSceneView* View = CreateThumbnailSceneView(&ViewFamily, CameraLocation, CameraRotation, CameraFovDegrees, ThumbnailSize, BackgroundColor);
	if (!View)
	{
		return false;
	}

	ViewFamily.EngineShowFlags.ScreenPercentage = false;
	ViewFamily.SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(ViewFamily, 1.0f));

	if (GEngine)
	{
		ViewFamily.ViewExtensions = GEngine->ViewExtensions->GatherActiveExtensions(FSceneViewExtensionContext(ViewFamily.Scene));
		for (const FSceneViewExtensionRef& Extension : ViewFamily.ViewExtensions)
		{
			Extension->SetupViewFamily(ViewFamily);
			Extension->SetupView(ViewFamily, *View);
		}
	}

	GetRendererModule().BeginRenderingViewFamily(&Canvas, &ViewFamily);
	Canvas.Flush_GameThread();
	FlushRenderingCommands();
	return true;
}

UObject* LoadThumbnailAsset(const FString& AssetObjectPath)
{
	FSoftObjectPath SoftObjectPath(AssetObjectPath);
	UObject* Asset = SoftObjectPath.TryLoad();
	if (!Asset)
	{
		Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetObjectPath);
	}
	return Asset;
}

void ConfigurePreviewLighting(FPreviewScene& PreviewScene)
{
	if (PreviewScene.DirectionalLight)
	{
		PreviewScene.DirectionalLight->Intensity = 0.2f;
		PreviewScene.DirectionalLight->SetMobility(EComponentMobility::Movable);
	}

	if (PreviewScene.SkyLight)
	{
		PreviewScene.SkyLight->bLowerHemisphereIsBlack = false;
		PreviewScene.SkyLight->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap;
		PreviewScene.SkyLight->Intensity = 1.69f;
		PreviewScene.SkyLight->SetMobility(EComponentMobility::Movable);

		if (GUnrealEd && GUnrealEd->GetThumbnailManager())
		{
			PreviewScene.SetSkyCubemap(GUnrealEd->GetThumbnailManager()->AmbientCubemap);
		}
	}

	UDirectionalLightComponent* DirectionalLight2 = NewObject<UDirectionalLightComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	DirectionalLight2->Intensity = 5.0f;
	DirectionalLight2->ForwardShadingPriority = 1;
	DirectionalLight2->SetMobility(EComponentMobility::Movable);
	PreviewScene.AddComponent(DirectionalLight2, FTransform(FRotator(-40.0f, -144.678f, 0.0f)));

	UDirectionalLightComponent* DirectionalLight3 = NewObject<UDirectionalLightComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	DirectionalLight3->Intensity = 1.0f;
	DirectionalLight3->ForwardShadingPriority = 2;
	DirectionalLight3->SetMobility(EComponentMobility::Movable);
	PreviewScene.AddComponent(DirectionalLight3, FTransform(FRotator(299.235f, 144.993f, 0.0f)));

	PreviewScene.UpdateCaptureContents();
}

bool CaptureMeshThumbnail(UObject* MeshAsset, const FString& OutPngPath, int32 Resolution, const FLinearColor& BackgroundColor, const FThumbnailCaptureOptions& CaptureOptions)
{
	if (!MeshAsset)
	{
		return false;
	}

	const FThumbnailCaptureOptions NormalizedOptions = NormalizeCaptureOptions(CaptureOptions, Resolution);
	const int32 ThumbnailSize = NormalizedOptions.Resolution;
	TArray<UTexture*> ForcedTextures;

	FPreviewScene PreviewScene(FPreviewScene::ConstructionValues()
		.SetLightRotation(FRotator(304.736f, 39.84f, 0.0f))
		.SetSkyBrightness(1.69f)
		.SetCreatePhysicsScene(false)
		.SetTransactional(false)
		.SetForceMipsResident(true));
	ConfigurePreviewLighting(PreviewScene);

	UWorld* PreviewWorld = PreviewScene.GetWorld();
	if (!PreviewWorld)
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Failed to create preview world for %s."), *MeshAsset->GetPathName());
		return false;
	}

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnInfo.ObjectFlags = RF_Transient;

	AActor* PreviewActor = nullptr;
	UMeshComponent* MeshComponent = nullptr;

	if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(MeshAsset))
	{
		if (StaticMesh->IsCompiling())
		{
			TArray<UStaticMesh*> StaticMeshesToCompile;
			StaticMeshesToCompile.Add(StaticMesh);
			FStaticMeshCompilingManager::Get().FinishCompilation(StaticMeshesToCompile);
		}

		AStaticMeshActor* StaticMeshActor = PreviewWorld->SpawnActor<AStaticMeshActor>(SpawnInfo);
		if (!StaticMeshActor || !StaticMeshActor->GetStaticMeshComponent())
		{
			UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Failed to spawn static mesh preview actor for %s."), *MeshAsset->GetPathName());
			return false;
		}

		PreviewActor = StaticMeshActor;
		UStaticMeshComponent* StaticMeshComponent = StaticMeshActor->GetStaticMeshComponent();
		MeshComponent = StaticMeshComponent;
		StaticMeshComponent->SetCanEverAffectNavigation(false);
		StaticMeshComponent->SetMobility(EComponentMobility::Movable);
		StaticMeshActor->SetActorEnableCollision(false);
		StaticMeshComponent->SetStaticMesh(StaticMesh);
		StaticMeshComponent->ForcedLodModel = 1;
		StaticMeshComponent->UpdateBounds();
		const float BoundsZOffset = ThumbnailBoundsZOffset(StaticMeshComponent->Bounds);
		StaticMeshActor->SetActorLocation(-StaticMeshComponent->Bounds.Origin + FVector(0.0f, 0.0f, BoundsZOffset), false);
		StaticMeshComponent->RecreateRenderState_Concurrent();
		StaticMesh->WaitForStreaming();
	}
	else if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(MeshAsset))
	{
		if (SkeletalMesh->IsCompiling())
		{
			TArray<USkinnedAsset*> SkinnedAssetsToCompile;
			SkinnedAssetsToCompile.Add(SkeletalMesh);
			FSkinnedAssetCompilingManager::Get().FinishCompilation(SkinnedAssetsToCompile);
		}

		ASkeletalMeshActor* SkeletalMeshActor = PreviewWorld->SpawnActor<ASkeletalMeshActor>(SpawnInfo);
		if (!SkeletalMeshActor || !SkeletalMeshActor->GetSkeletalMeshComponent())
		{
			UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Failed to spawn skeletal mesh preview actor for %s."), *MeshAsset->GetPathName());
			return false;
		}

		PreviewActor = SkeletalMeshActor;
		USkeletalMeshComponent* SkeletalMeshComponent = SkeletalMeshActor->GetSkeletalMeshComponent();
		MeshComponent = SkeletalMeshComponent;
		SkeletalMeshComponent->SetMobility(EComponentMobility::Movable);
		SkeletalMeshActor->SetActorEnableCollision(false);
		SkeletalMeshComponent->SetSkeletalMesh(SkeletalMesh, false);
		SkeletalMeshComponent->SetForcedLOD(1);
		SkeletalMeshComponent->UpdateBounds();
		const float BoundsZOffset = ThumbnailBoundsZOffset(SkeletalMeshComponent->Bounds);
		SkeletalMeshActor->SetActorLocation(-SkeletalMeshComponent->Bounds.Origin + FVector(0.0f, 0.0f, BoundsZOffset), false);
		SkeletalMeshComponent->RecreateRenderState_Concurrent();
	}

	if (!PreviewActor || !MeshComponent)
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Asset is not a supported mesh type for thumbnail capture: %s"), *MeshAsset->GetPathName());
		return false;
	}

	ForceComponentTexturesResident(MeshComponent, ForcedTextures);
	EnsureMeshMaterialsComplete(MeshComponent);
	FAssetCompilingManager::Get().FinishAllCompilation();
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}

	FlushRenderingCommands();
	IStreamingManager::Get().StreamAllResources(5.0f);
	FlushRenderingCommands();
	PreviewScene.GetScene()->UpdateSpeedTreeWind(0.0);

	FVector CameraLocation;
	FRotator CameraRotation;
	float CameraFovDegrees = NormalizedOptions.FovDegrees;
	MeshComponent->UpdateBounds();
	if (!ComputeThumbnailCameraView(MeshAsset, MeshComponent->Bounds, CameraLocation, CameraRotation, CameraFovDegrees, NormalizedOptions))
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Failed to compute thumbnail camera for %s."), *MeshAsset->GetPathName());
		RestoreMeshStreamingState(MeshComponent, ForcedTextures);
		return false;
	}

	FAssetCompilingManager::Get().FinishAllCompilation();
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}
	FlushRenderingCommands();

	UTextureRenderTarget2D* RenderTarget = CreateThumbnailRenderTarget(ThumbnailSize, BackgroundColor);
	if (!RenderTarget)
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Failed to create thumbnail render target for %s."), *MeshAsset->GetPathName());
		RestoreMeshStreamingState(MeshComponent, ForcedTextures);
		return false;
	}

	if (!RenderThumbnailPreviewScene(PreviewScene, RenderTarget, CameraLocation, CameraRotation, CameraFovDegrees, ThumbnailSize, BackgroundColor))
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Failed to render thumbnail preview scene for %s."), *MeshAsset->GetPathName());
		RestoreMeshStreamingState(MeshComponent, ForcedTextures);
		return false;
	}

	FTextureRenderTargetResource* RenderTargetResource = RenderTarget->GameThread_GetRenderTargetResource();
	TArray<FColor> CapturedPixels;
	FReadSurfaceDataFlags ReadFlags(RCM_UNorm);
	const bool bReadPixels = RenderTargetResource && RenderTargetResource->ReadPixels(CapturedPixels, ReadFlags);

	RestoreMeshStreamingState(MeshComponent, ForcedTextures);

	if (!bReadPixels || CapturedPixels.Num() != ThumbnailSize * ThumbnailSize)
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Failed to read thumbnail pixels for %s (pixels=%d)."), *MeshAsset->GetPathName(), CapturedPixels.Num());
		return false;
	}

	for (FColor& Pixel : CapturedPixels)
	{
		Pixel.A = 255;
	}

	return EncodePngFile(OutPngPath, CapturedPixels, ThumbnailSize, ThumbnailSize);
}

bool ExportNonMeshThumbnail(UObject* Asset, const FString& AssetObjectPath, const FString& OutPngPath, int32 Resolution)
{
	FAssetCompilingManager::Get().FinishAllCompilation();
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}

	FlushRenderingCommands();

	const uint32 ThumbnailSize = static_cast<uint32>(FMath::Clamp(Resolution, 64, 2048));
	FObjectThumbnail Thumbnail;
	ThumbnailTools::RenderThumbnail(
		Asset,
		ThumbnailSize,
		ThumbnailSize,
		ThumbnailTools::EThumbnailTextureFlushMode::AlwaysFlush,
		nullptr,
		&Thumbnail);

	if (Thumbnail.IsEmpty())
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Generated thumbnail is empty: %s"), *AssetObjectPath);
		return false;
	}

	const int32 Width = Thumbnail.GetImageWidth();
	const int32 Height = Thumbnail.GetImageHeight();
	const TArray<uint8>& RawData = Thumbnail.GetUncompressedImageData();
	if (Width <= 0 || Height <= 0 || RawData.Num() < Width * Height * 4)
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Invalid thumbnail data for %s (%dx%d, bytes=%d)."), *AssetObjectPath, Width, Height, RawData.Num());
		return false;
	}

	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(Width * Height);
	FMemory::Memcpy(Pixels.GetData(), RawData.GetData(), Width * Height * sizeof(FColor));
	for (FColor& Pixel : Pixels)
	{
		Pixel.A = 255;
	}

	return EncodePngFile(OutPngPath, Pixels, Width, Height);
}

bool ExportAssetThumbnailInternal(const FString& AssetObjectPath, const FString& OutPngPath, int32 Resolution, const FLinearColor& BackgroundColor, const FThumbnailCaptureOptions& CaptureOptions)
{
	if (AssetObjectPath.IsEmpty())
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("AssetObjectPath is empty."));
		return false;
	}

	if (OutPngPath.IsEmpty())
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("OutPngPath is empty for %s."), *AssetObjectPath);
		return false;
	}

	UObject* Asset = LoadThumbnailAsset(AssetObjectPath);
	if (!Asset)
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Failed to load asset: %s"), *AssetObjectPath);
		return false;
	}

	const FThumbnailCaptureOptions NormalizedOptions = NormalizeCaptureOptions(CaptureOptions, Resolution);
	const int32 EffectiveResolution = NormalizedOptions.Resolution;
	const bool bHasCustomMeshOptions = NormalizedOptions.bOverrideCamera ||
		!FMath::IsNearlyEqual(NormalizedOptions.FovDegrees, ThumbnailDefaultFovDegrees) ||
		CaptureOptions.Resolution > 0;

	if (Asset->IsA<UStaticMesh>() || Asset->IsA<USkeletalMesh>())
	{
		const bool bCaptured = CaptureMeshThumbnail(Asset, OutPngPath, EffectiveResolution, BackgroundColor, NormalizedOptions);
		if (bCaptured)
		{
			return true;
		}

		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Mesh thumbnail capture failed for %s; falling back to native thumbnail renderer."), *AssetObjectPath);
	}

	return ExportNonMeshThumbnail(Asset, AssetObjectPath, OutPngPath, bHasCustomMeshOptions ? EffectiveResolution : Resolution);
}

FThumbnailExportResult ExportAssetThumbnailWithCameraInternal(const FString& AssetObjectPath, const FString& OutPngPath, int32 Resolution, const FLinearColor& BackgroundColor, const FThumbnailCaptureOptions& CaptureOptions)
{
	FThumbnailExportResult Result;
	const bool bExported = ExportAssetThumbnailInternal(AssetObjectPath, OutPngPath, Resolution, BackgroundColor, CaptureOptions);
	if (!bExported)
	{
		return Result;
	}

	UObject* Asset = LoadThumbnailAsset(AssetObjectPath);
	if (!Asset || (!Asset->IsA<UStaticMesh>() && !Asset->IsA<USkeletalMesh>()))
	{
		Result.bSuccess = true;
		return Result;
	}

	if (CaptureOptions.bOverrideCamera)
	{
		Result.CameraRotation = ThumbnailCameraRotationFromOrbit(CaptureOptions.OrbitPitch, CaptureOptions.OrbitYaw).Rotator();
		Result.bSuccess = true;
		return Result;
	}

	if (!ResolveThumbnailCameraRotation(Asset, Result.CameraRotation))
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Failed to resolve thumbnail camera rotation for %s."), *AssetObjectPath);
		return Result;
	}

	Result.bSuccess = true;
	return Result;
}
}
#endif

bool UThumbnailLibrary::ExportAssetThumbnail(const FString& AssetObjectPath, const FString& OutPngPath, int32 Resolution, FLinearColor BackgroundColor)
{
#if WITH_EDITOR
	return ExportAssetThumbnailInternal(AssetObjectPath, OutPngPath, Resolution, BackgroundColor, FThumbnailCaptureOptions());
#else
	return false;
#endif
}

bool UThumbnailLibrary::ExportAssetThumbnailWithOptions(const FString& AssetObjectPath, const FString& OutPngPath, const FThumbnailCaptureOptions& CaptureOptions, int32 Resolution, FLinearColor BackgroundColor)
{
#if WITH_EDITOR
	return ExportAssetThumbnailInternal(AssetObjectPath, OutPngPath, Resolution, BackgroundColor, CaptureOptions);
#else
	return false;
#endif
}

FThumbnailExportResult UThumbnailLibrary::ExportAssetThumbnailWithCamera(const FString& AssetObjectPath, const FString& OutPngPath, int32 Resolution, FLinearColor BackgroundColor)
{
#if WITH_EDITOR
	return ExportAssetThumbnailWithCameraInternal(AssetObjectPath, OutPngPath, Resolution, BackgroundColor, FThumbnailCaptureOptions());
#else
	return FThumbnailExportResult();
#endif
}

FThumbnailExportResult UThumbnailLibrary::ExportAssetThumbnailWithCameraAndOptions(const FString& AssetObjectPath, const FString& OutPngPath, const FThumbnailCaptureOptions& CaptureOptions, int32 Resolution, FLinearColor BackgroundColor)
{
#if WITH_EDITOR
	return ExportAssetThumbnailWithCameraInternal(AssetObjectPath, OutPngPath, Resolution, BackgroundColor, CaptureOptions);
#else
	return FThumbnailExportResult();
#endif
}
