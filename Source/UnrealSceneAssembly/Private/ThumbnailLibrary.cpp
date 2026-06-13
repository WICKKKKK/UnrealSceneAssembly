#include "ThumbnailLibrary.h"

#include "AssetCompilingManager.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ContentStreaming.h"
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
#include "Math/PerspectiveMatrix.h"
#include "Math/RotationMatrix.h"
#include "Math/UnrealMathUtility.h"
#include "Materials/MaterialInstanceDynamic.h"
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
constexpr float SceneCaptureThumbnailFovDegrees = 30.0f;
constexpr float SceneCaptureTextureForceResidentSeconds = 30.0f;
constexpr int32 SceneCaptureWarmUpFrames = 8;
const TCHAR* ThumbnailBackdropMaterialPath = TEXT("/UnrealSceneAssembly/SceneCapture/M_SceneCaptureID.M_SceneCaptureID");
const TCHAR* ThumbnailBackdropColorParameter = TEXT("IDColor");
const TCHAR* ThumbnailBackdropSpherePath = TEXT("/Engine/BasicShapes/Sphere.Sphere");

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

float ThumbnailBoundsZOffset(const FBoxSphereBounds& Bounds)
{
	return static_cast<float>(Bounds.BoxExtent.Z + 1.0);
}

bool ComputeThumbnailCameraView(
	UObject* MeshAsset,
	const FBoxSphereBounds& Bounds,
	FVector& OutLocation,
	FRotator& OutRotation,
	float& OutFovDegrees)
{
	const USceneThumbnailInfo* ThumbnailInfo = SceneThumbnailInfoOrDefault(MeshAsset);
	if (!ThumbnailInfo)
	{
		return false;
	}

	OutFovDegrees = SceneCaptureThumbnailFovDegrees;
	const float HalfFovRadians = FMath::DegreesToRadians<float>(OutFovDegrees) * 0.5f;
	const bool bIsStaticMesh = MeshAsset->IsA<UStaticMesh>();
	const float BoundsScale = bIsStaticMesh ? 1.15f : 1.0f;
	const float HalfMeshSize = static_cast<float>(Bounds.SphereRadius * BoundsScale);
	const float TargetDistance = HalfMeshSize / FMath::Tan(HalfFovRadians);
	const float BoundsZOffset = ThumbnailBoundsZOffset(Bounds);

	FVector Origin(0.0f, 0.0f, -BoundsZOffset);
	const float OrbitPitch = ThumbnailInfo->OrbitPitch;
	const float OrbitYaw = ThumbnailInfo->OrbitYaw;
	const float OrbitZoom = FMath::Max(48.0f, TargetDistance + ThumbnailInfo->OrbitZoom);

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

	MeshComponent->PrestreamTextures(SceneCaptureTextureForceResidentSeconds, false, 0);
	MeshComponent->SetTextureForceResidentFlag(true);

	TArray<UTexture*> UsedTextures;
	MeshComponent->GetUsedTextures(UsedTextures, EMaterialQualityLevel::Num);
	for (UTexture* Texture : UsedTextures)
	{
		if (!Texture || ForcedTextures.Contains(Texture))
		{
			continue;
		}

		Texture->SetForceMipLevelsToBeResident(SceneCaptureTextureForceResidentSeconds);
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
	RenderTarget->TargetGamma = 2.2f;
	RenderTarget->InitCustomFormat(ThumbnailSize, ThumbnailSize, PF_B8G8R8A8, false);
	RenderTarget->UpdateResourceImmediate(true);
	return RenderTarget;
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

AStaticMeshActor* CreateBackdropSphere(UWorld* PreviewWorld, FPreviewScene& PreviewScene, const FVector& CameraLocation, const FVector& TargetLocation, const float Radius, const FLinearColor& BackgroundColor)
{
	if (!PreviewWorld)
	{
		return nullptr;
	}

	UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, ThumbnailBackdropSpherePath);
	UMaterialInterface* BackdropMaterial = LoadObject<UMaterialInterface>(nullptr, ThumbnailBackdropMaterialPath);
	if (!SphereMesh || !BackdropMaterial)
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Failed to load thumbnail backdrop resources (sphere=%s, material=%s)."), SphereMesh ? TEXT("ok") : TEXT("missing"), BackdropMaterial ? TEXT("ok") : TEXT("missing"));
		return nullptr;
	}

	UMaterialInstanceDynamic* BackdropMaterialInstance = UMaterialInstanceDynamic::Create(BackdropMaterial, GetTransientPackage());
	if (!BackdropMaterialInstance)
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Failed to create thumbnail backdrop material instance."));
		return nullptr;
	}

	BackdropMaterialInstance->SetVectorParameterValue(ThumbnailBackdropColorParameter, BackgroundColor);
	if (UMaterial* BaseMaterial = BackdropMaterialInstance->GetMaterial())
	{
		BaseMaterial->EnsureIsComplete();
	}
	BackdropMaterialInstance->EnsureIsComplete();

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnInfo.ObjectFlags = RF_Transient;

	AStaticMeshActor* BackdropActor = PreviewWorld->SpawnActor<AStaticMeshActor>(SpawnInfo);
	if (!BackdropActor || !BackdropActor->GetStaticMeshComponent())
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Failed to spawn thumbnail backdrop actor."));
		return nullptr;
	}

	UStaticMeshComponent* BackdropComponent = BackdropActor->GetStaticMeshComponent();
	BackdropActor->SetActorEnableCollision(false);
	BackdropComponent->SetCanEverAffectNavigation(false);
	BackdropComponent->SetMobility(EComponentMobility::Movable);
	BackdropComponent->SetStaticMesh(SphereMesh);
	BackdropComponent->SetCastShadow(false);
	BackdropComponent->bCastDynamicShadow = false;
	BackdropComponent->bCastStaticShadow = false;
	BackdropComponent->SetReceivesDecals(false);

	const FVector BackdropCenter = FMath::Lerp(CameraLocation, TargetLocation, 0.5f);
	const float SphereDiameter = 100.0f;
	const float BackdropScale = FMath::Max(Radius * 4.0f / SphereDiameter, 10.0f);
	BackdropActor->SetActorLocation(BackdropCenter, false);
	BackdropActor->SetActorScale3D(FVector(BackdropScale));
	BackdropComponent->SetMaterial(0, BackdropMaterialInstance);
	BackdropComponent->RecreateRenderState_Concurrent();
	return BackdropActor;
}

bool CaptureMeshThumbnailViaSceneCapture(UObject* MeshAsset, const FString& OutPngPath, int32 Resolution, const FLinearColor& BackgroundColor)
{
	if (!MeshAsset)
	{
		return false;
	}

	const int32 ThumbnailSize = FMath::Clamp(Resolution, 64, 2048);
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
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Asset is not a supported mesh type for SceneCapture thumbnail: %s"), *MeshAsset->GetPathName());
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

	FVector CameraLocation;
	FRotator CameraRotation;
	float CameraFovDegrees = SceneCaptureThumbnailFovDegrees;
	MeshComponent->UpdateBounds();
	if (!ComputeThumbnailCameraView(MeshAsset, MeshComponent->Bounds, CameraLocation, CameraRotation, CameraFovDegrees))
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Failed to compute thumbnail camera for %s."), *MeshAsset->GetPathName());
		RestoreMeshStreamingState(MeshComponent, ForcedTextures);
		return false;
	}

	const FVector TargetLocation = MeshComponent->Bounds.Origin;
	AStaticMeshActor* BackdropActor = CreateBackdropSphere(
		PreviewWorld,
		PreviewScene,
		CameraLocation,
		TargetLocation,
		static_cast<float>(MeshComponent->Bounds.SphereRadius + FVector::Distance(CameraLocation, TargetLocation)),
		BackgroundColor);

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

	USceneCaptureComponent2D* CaptureComponent = NewObject<USceneCaptureComponent2D>(GetTransientPackage(), NAME_None, RF_Transient);
	if (!CaptureComponent)
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Failed to create scene capture component for %s."), *MeshAsset->GetPathName());
		RestoreMeshStreamingState(MeshComponent, ForcedTextures);
		return false;
	}

	CaptureComponent->TextureTarget = RenderTarget;
	CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	CaptureComponent->ProjectionType = ECameraProjectionMode::Perspective;
	CaptureComponent->FOVAngle = CameraFovDegrees;
	CaptureComponent->bCaptureEveryFrame = false;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->bAlwaysPersistRenderingState = true;
	CaptureComponent->PostProcessBlendWeight = 1.0f;
	CaptureComponent->PostProcessSettings.bOverride_AutoExposureMethod = true;
	CaptureComponent->PostProcessSettings.AutoExposureMethod = AEM_Manual;
	CaptureComponent->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	CaptureComponent->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
	CaptureComponent->PostProcessSettings.bOverride_AutoExposureBias = true;
	CaptureComponent->PostProcessSettings.AutoExposureBias = 0.0f;
	CaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	CaptureComponent->RegisterComponentWithWorld(PreviewWorld);
	CaptureComponent->ShowFlags.DisableAdvancedFeatures();
	CaptureComponent->ShowFlags.LOD = 0;
	CaptureComponent->ShowFlags.SetMotionBlur(false);
	CaptureComponent->ShowFlags.SetEyeAdaptation(false);
	CaptureComponent->SetWorldLocationAndRotation(CameraLocation, CameraRotation);
	CaptureComponent->ShowOnlyActorComponents(PreviewActor);
	if (BackdropActor)
	{
		CaptureComponent->ShowOnlyActorComponents(BackdropActor);
	}

	for (int32 FrameIndex = 0; FrameIndex < SceneCaptureWarmUpFrames; ++FrameIndex)
	{
		CaptureComponent->CaptureScene();
		FlushRenderingCommands();
		IStreamingManager::Get().StreamAllResources(1.0f);
	}

	FTextureRenderTargetResource* RenderTargetResource = RenderTarget->GameThread_GetRenderTargetResource();
	TArray<FColor> CapturedPixels;
	FReadSurfaceDataFlags ReadFlags(RCM_UNorm);
	ReadFlags.SetLinearToGamma(false);
	const bool bReadPixels = RenderTargetResource && RenderTargetResource->ReadPixels(CapturedPixels, ReadFlags);

	CaptureComponent->TextureTarget = nullptr;
	CaptureComponent->DestroyComponent();
	RestoreMeshStreamingState(MeshComponent, ForcedTextures);

	if (!bReadPixels || CapturedPixels.Num() != ThumbnailSize * ThumbnailSize)
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Failed to read scene capture pixels for %s (pixels=%d)."), *MeshAsset->GetPathName(), CapturedPixels.Num());
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
}
#endif

bool UThumbnailLibrary::ExportAssetThumbnail(const FString& AssetObjectPath, const FString& OutPngPath, int32 Resolution, FLinearColor BackgroundColor)
{
#if WITH_EDITOR
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

	FSoftObjectPath SoftObjectPath(AssetObjectPath);
	UObject* Asset = SoftObjectPath.TryLoad();
	if (!Asset)
	{
		Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetObjectPath);
	}

	if (!Asset)
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Failed to load asset: %s"), *AssetObjectPath);
		return false;
	}

	if (Asset->IsA<UStaticMesh>() || Asset->IsA<USkeletalMesh>())
	{
		const bool bCaptured = CaptureMeshThumbnailViaSceneCapture(Asset, OutPngPath, Resolution, BackgroundColor);
		if (!bCaptured)
		{
			UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("SceneCapture thumbnail failed for %s."), *AssetObjectPath);
		}
		return bCaptured;
	}

	return ExportNonMeshThumbnail(Asset, AssetObjectPath, OutPngPath, Resolution);
#else
	return false;
#endif
}

FThumbnailExportResult UThumbnailLibrary::ExportAssetThumbnailWithCamera(const FString& AssetObjectPath, const FString& OutPngPath, int32 Resolution, FLinearColor BackgroundColor)
{
	FThumbnailExportResult Result;
#if WITH_EDITOR
	const bool bExported = ExportAssetThumbnail(AssetObjectPath, OutPngPath, Resolution, BackgroundColor);
	if (!bExported)
	{
		return Result;
	}

	FSoftObjectPath SoftObjectPath(AssetObjectPath);
	UObject* Asset = SoftObjectPath.TryLoad();
	if (!Asset)
	{
		Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetObjectPath);
	}

	if (!Asset || (!Asset->IsA<UStaticMesh>() && !Asset->IsA<USkeletalMesh>()))
	{
		Result.bSuccess = true;
		return Result;
	}

	if (!ResolveThumbnailCameraRotation(Asset, Result.CameraRotation))
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Failed to resolve thumbnail camera rotation for %s."), *AssetObjectPath);
		return Result;
	}
	Result.bSuccess = true;
#endif
	return Result;
}
