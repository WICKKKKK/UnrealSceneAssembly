#include "SceneCaptureLibrary.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "AssetCompilingManager.h"
#include "HAL/FileManager.h"
#include "HighResScreenshot.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "LevelEditorViewport.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Modules/ModuleManager.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "SceneCaptureTypes.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogSceneCaptureLibrary, Log, All);

namespace
{
constexpr int32 MaxCaptureResolution = 8192;
constexpr int32 MaxRgbId = 0xFFFFFF;
constexpr int32 HighResScreenshotDrawAttempts = 3;
constexpr int32 SceneCaptureWarmUpFrames = 8;
constexpr int32 IdVisibilityWarmUpFrames = 2;
constexpr int32 IdFinalWarmUpFrames = 2;
constexpr int32 IdColorSnapTolerance = 2;
constexpr double GoldenRatioConjugate = 0.6180339887498948482;
const TCHAR* IdMaterialPath = TEXT("/UnrealSceneAssembly/SceneCapture/M_SceneCaptureID.M_SceneCaptureID");
const FName IdColorParameterName(TEXT("IDColor"));

struct FIdMapEntry
{
	int32 Id = 0;
	FColor Color = FColor::Black;
	FString ActorPath;
	bool bHasPixelBounds = false;
	int32 PixelMinX = 0;
	int32 PixelMinY = 0;
	int32 PixelMaxX = 0;
	int32 PixelMaxY = 0;
};

struct FMaterialOverrideRecord
{
	TWeakObjectPtr<UPrimitiveComponent> Component;
	int32 MaterialIndex = INDEX_NONE;
	TWeakObjectPtr<UMaterialInterface> OriginalMaterial;
};

struct FIdCaptureRenderResult
{
	TArray<FColor> Pixels;
	TSet<uint32> VisibleColorKeys;
};

uint32 MakeColorKey(const FColor& Color)
{
	return (static_cast<uint32>(Color.R) << 16) | (static_cast<uint32>(Color.G) << 8) | static_cast<uint32>(Color.B);
}

FColor MakeHsvColor(const double Hue, const double Saturation, const double Value)
{
	const double WrappedHue = FMath::Frac(Hue) * 6.0;
	const int32 Sector = FMath::FloorToInt(WrappedHue);
	const double Fraction = WrappedHue - static_cast<double>(Sector);
	const double P = Value * (1.0 - Saturation);
	const double Q = Value * (1.0 - Saturation * Fraction);
	const double T = Value * (1.0 - Saturation * (1.0 - Fraction));

	double R = Value;
	double G = T;
	double B = P;
	switch (Sector % 6)
	{
	case 0:
		R = Value;
		G = T;
		B = P;
		break;
	case 1:
		R = Q;
		G = Value;
		B = P;
		break;
	case 2:
		R = P;
		G = Value;
		B = T;
		break;
	case 3:
		R = P;
		G = Q;
		B = Value;
		break;
	case 4:
		R = T;
		G = P;
		B = Value;
		break;
	default:
		R = Value;
		G = P;
		B = Q;
		break;
	}

	return FColor(
		static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(R * 255.0), 0, 255)),
		static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(G * 255.0), 0, 255)),
		static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(B * 255.0), 0, 255)),
		255);
}

FColor GenerateIdColor(const int32 Id, TSet<uint32>& UsedColorKeys)
{
	static constexpr double SaturationLevels[] = { 0.90, 0.72, 1.00, 0.58 };
	static constexpr double ValueLevels[] = { 1.00, 0.92, 0.84 };
	const int32 SaturationLevelCount = UE_ARRAY_COUNT(SaturationLevels);
	const int32 ValueLevelCount = UE_ARRAY_COUNT(ValueLevels);

	for (int32 SearchIndex = 0; SearchIndex < MaxRgbId; ++SearchIndex)
	{
		const int32 CandidateIndex = (Id - 1) + SearchIndex;
		const int32 RingIndex = CandidateIndex / 1536;
		const double Hue = FMath::Frac(static_cast<double>(CandidateIndex + 1) * GoldenRatioConjugate);
		const double Saturation = SaturationLevels[RingIndex % SaturationLevelCount];
		const double Value = ValueLevels[(RingIndex / SaturationLevelCount) % ValueLevelCount];
		const FColor Candidate = MakeHsvColor(Hue, Saturation, Value);
		const uint32 Key = MakeColorKey(Candidate);
		if (Key != 0 && !UsedColorKeys.Contains(Key))
		{
			UsedColorKeys.Add(Key);
			return Candidate;
		}
	}

	for (uint32 CandidateKey = 1; CandidateKey <= static_cast<uint32>(MaxRgbId); ++CandidateKey)
	{
		if (!UsedColorKeys.Contains(CandidateKey))
		{
			UsedColorKeys.Add(CandidateKey);
			return FColor(
				static_cast<uint8>((CandidateKey >> 16) & 0xFF),
				static_cast<uint8>((CandidateKey >> 8) & 0xFF),
				static_cast<uint8>(CandidateKey & 0xFF),
				255);
		}
	}

	return FColor::White;
}

FString MakeSafeBaseName(const FString& BaseName)
{
	FString SafeBaseName = BaseName.IsEmpty()
		? FString::Printf(TEXT("capture_%s"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")))
		: BaseName;

	SafeBaseName = FPaths::MakeValidFileName(SafeBaseName);
	return SafeBaseName.IsEmpty() ? TEXT("capture") : SafeBaseName;
}

bool EnsureOutputDirectory(const FString& OutputDir, FString& OutAbsoluteOutputDir)
{
	OutAbsoluteOutputDir.Reset();

	if (OutputDir.IsEmpty())
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("OutputDir is empty."));
		return false;
	}

	OutAbsoluteOutputDir = FPaths::ConvertRelativePathToFull(OutputDir);
	FPaths::NormalizeDirectoryName(OutAbsoluteOutputDir);

	if (!IFileManager::Get().MakeDirectory(*OutAbsoluteOutputDir, true))
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Failed to create output directory: %s"), *OutAbsoluteOutputDir);
		return false;
	}

	return true;
}

TSharedRef<FJsonObject> MakeVectorJson(const FVector& Value)
{
	const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("x"), Value.X);
	Json->SetNumberField(TEXT("y"), Value.Y);
	Json->SetNumberField(TEXT("z"), Value.Z);
	return Json;
}

TSharedRef<FJsonObject> MakeRotatorJson(const FRotator& Value)
{
	const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("pitch"), Value.Pitch);
	Json->SetNumberField(TEXT("yaw"), Value.Yaw);
	Json->SetNumberField(TEXT("roll"), Value.Roll);
	return Json;
}

TSharedRef<FJsonObject> MakeCameraJson(const FSceneCaptureCameraInfo& Camera)
{
	const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetObjectField(TEXT("location"), MakeVectorJson(Camera.Location));
	Json->SetObjectField(TEXT("rotation"), MakeRotatorJson(Camera.Rotation));
	Json->SetNumberField(TEXT("fov_horizontal"), Camera.FovHorizontal);
	Json->SetNumberField(TEXT("aspect_ratio"), Camera.AspectRatio);
	Json->SetStringField(TEXT("projection_type"), TEXT("perspective"));

	const TSharedRef<FJsonObject> ResolutionJson = MakeShared<FJsonObject>();
	ResolutionJson->SetNumberField(TEXT("width"), Camera.Width);
	ResolutionJson->SetNumberField(TEXT("height"), Camera.Height);
	Json->SetObjectField(TEXT("resolution"), ResolutionJson);
	return Json;
}

TArray<TSharedPtr<FJsonValue>> MakeIdMapJson(const TArray<FIdMapEntry>& Entries)
{
	TArray<TSharedPtr<FJsonValue>> JsonEntries;
	JsonEntries.Reserve(Entries.Num());

	for (const FIdMapEntry& Entry : Entries)
	{
		const TSharedRef<FJsonObject> EntryJson = MakeShared<FJsonObject>();
		EntryJson->SetNumberField(TEXT("id"), Entry.Id);

		TArray<TSharedPtr<FJsonValue>> ColorJson;
		ColorJson.Add(MakeShared<FJsonValueNumber>(Entry.Color.R));
		ColorJson.Add(MakeShared<FJsonValueNumber>(Entry.Color.G));
		ColorJson.Add(MakeShared<FJsonValueNumber>(Entry.Color.B));
		EntryJson->SetArrayField(TEXT("color_rgb"), ColorJson);

		EntryJson->SetStringField(TEXT("actor_path"), Entry.ActorPath);
		if (Entry.bHasPixelBounds)
		{
			TArray<TSharedPtr<FJsonValue>> BoundsJson;
			BoundsJson.Add(MakeShared<FJsonValueNumber>(Entry.PixelMinX));
			BoundsJson.Add(MakeShared<FJsonValueNumber>(Entry.PixelMinY));
			BoundsJson.Add(MakeShared<FJsonValueNumber>(Entry.PixelMaxX));
			BoundsJson.Add(MakeShared<FJsonValueNumber>(Entry.PixelMaxY));
			EntryJson->SetArrayField(TEXT("pixel_bbox"), BoundsJson);
		}
		else
		{
			EntryJson->SetField(TEXT("pixel_bbox"), MakeShared<FJsonValueNull>());
		}
		JsonEntries.Add(MakeShared<FJsonValueObject>(EntryJson));
	}

	return JsonEntries;
}

bool WriteJsonFile(const FString& JsonPath, const TSharedRef<FJsonObject>& JsonObject)
{
	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	if (!FJsonSerializer::Serialize(JsonObject, Writer))
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Failed to serialize JSON: %s"), *JsonPath);
		return false;
	}

	if (!FFileHelper::SaveStringToFile(Output, *JsonPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Failed to save JSON: %s"), *JsonPath);
		return false;
	}

	return true;
}

bool EncodePngData(const TArray<FColor>& Pixels, const int32 Width, const int32 Height, TArray64<uint8>& OutCompressedData)
{
	OutCompressedData.Reset();
	if (Width <= 0 || Height <= 0 || Pixels.Num() != Width * Height)
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Invalid PNG pixel data: %d pixels, %dx%d."), Pixels.Num(), Width, Height);
		return false;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	const TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!ImageWrapper.IsValid())
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Failed to create PNG image wrapper."));
		return false;
	}

	const int64 RawDataSize = static_cast<int64>(Pixels.Num()) * sizeof(FColor);
	if (!ImageWrapper->SetRaw(Pixels.GetData(), RawDataSize, Width, Height, ERGBFormat::BGRA, 8))
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Failed to encode PNG pixel data."));
		return false;
	}

	OutCompressedData = ImageWrapper->GetCompressed(100);
	return OutCompressedData.Num() > 0;
}

bool SavePngFile(const FString& PngPath, const TArray<FColor>& Pixels, const int32 Width, const int32 Height)
{
	TArray64<uint8> CompressedData;
	if (!EncodePngData(Pixels, Width, Height, CompressedData))
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Failed to encode PNG: %s"), *PngPath);
		return false;
	}
	if (CompressedData.Num() <= 0)
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Invalid compressed PNG size for %s: %lld."), *PngPath, static_cast<long long>(CompressedData.Num()));
		return false;
	}

	if (!FFileHelper::SaveArrayToFile(CompressedData, *PngPath))
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Failed to save PNG: %s"), *PngPath);
		return false;
	}

	return true;
}

bool LoadImageFileAsPixels(const FString& ImagePath, TArray<FColor>& OutPixels, int32& OutWidth, int32& OutHeight)
{
	OutPixels.Reset();
	OutWidth = 0;
	OutHeight = 0;

	TArray<uint8> CompressedData;
	if (!FFileHelper::LoadFileToArray(CompressedData, *ImagePath) || CompressedData.IsEmpty())
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Failed to load image file: %s"), *ImagePath);
		return false;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	const EImageFormat ImageFormat = ImageWrapperModule.DetectImageFormat(CompressedData.GetData(), CompressedData.Num());
	if (ImageFormat == EImageFormat::Invalid)
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Unsupported image format: %s"), *ImagePath);
		return false;
	}

	const TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat, *ImagePath);
	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(CompressedData.GetData(), CompressedData.Num()))
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Failed to decode image file: %s"), *ImagePath);
		return false;
	}

	TArray64<uint8> RawData;
	if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Failed to decode image pixels: %s"), *ImagePath);
		return false;
	}

	OutWidth = static_cast<int32>(ImageWrapper->GetWidth());
	OutHeight = static_cast<int32>(ImageWrapper->GetHeight());
	if (OutWidth <= 0 || OutHeight <= 0 || RawData.Num() != static_cast<int64>(OutWidth) * OutHeight * 4)
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Invalid decoded image size: %s (%dx%d, %lld bytes)."), *ImagePath, OutWidth, OutHeight, static_cast<long long>(RawData.Num()));
		OutPixels.Reset();
		OutWidth = 0;
		OutHeight = 0;
		return false;
	}

	OutPixels.SetNumUninitialized(OutWidth * OutHeight);
	FMemory::Memcpy(OutPixels.GetData(), RawData.GetData(), RawData.Num());
	return true;
}

bool CropBitmapRegion(const TArray<FColor>& SourcePixels, const int32 SourceWidth, const int32 SourceHeight, const int32 XMin, const int32 YMin, const int32 XMax, const int32 YMax, TArray<FColor>& OutPixels, int32& OutWidth, int32& OutHeight)
{
	OutPixels.Reset();
	OutWidth = 0;
	OutHeight = 0;
	if (SourceWidth <= 0 || SourceHeight <= 0 || SourcePixels.Num() != SourceWidth * SourceHeight)
	{
		return false;
	}

	const int32 ClampedMinX = FMath::Clamp(FMath::Min(XMin, XMax), 0, SourceWidth - 1);
	const int32 ClampedMaxX = FMath::Clamp(FMath::Max(XMin, XMax), 0, SourceWidth - 1);
	const int32 ClampedMinY = FMath::Clamp(FMath::Min(YMin, YMax), 0, SourceHeight - 1);
	const int32 ClampedMaxY = FMath::Clamp(FMath::Max(YMin, YMax), 0, SourceHeight - 1);
	if (ClampedMinX > ClampedMaxX || ClampedMinY > ClampedMaxY)
	{
		return false;
	}

	OutWidth = ClampedMaxX - ClampedMinX + 1;
	OutHeight = ClampedMaxY - ClampedMinY + 1;
	OutPixels.SetNumUninitialized(OutWidth * OutHeight);
	for (int32 Row = 0; Row < OutHeight; ++Row)
	{
		const int32 SourceIndex = (ClampedMinY + Row) * SourceWidth + ClampedMinX;
		const int32 DestIndex = Row * OutWidth;
		FMemory::Memcpy(&OutPixels[DestIndex], &SourcePixels[SourceIndex], sizeof(FColor) * OutWidth);
	}
	return true;
}

bool CropBitmapToAspect(const TArray<FColor>& SourcePixels, const int32 SourceWidth, const int32 SourceHeight, const float TargetAspectRatio, TArray<FColor>& OutPixels, int32& OutWidth, int32& OutHeight)
{
	OutPixels.Reset();
	OutWidth = 0;
	OutHeight = 0;

	if (SourceWidth <= 0 || SourceHeight <= 0 || TargetAspectRatio <= 0.0f || SourcePixels.Num() != SourceWidth * SourceHeight)
	{
		return false;
	}

	const float SourceAspectRatio = static_cast<float>(SourceWidth) / static_cast<float>(SourceHeight);
	int32 CropX = 0;
	int32 CropY = 0;
	int32 CropWidth = SourceWidth;
	int32 CropHeight = SourceHeight;

	if (SourceAspectRatio > TargetAspectRatio)
	{
		CropWidth = FMath::Clamp(FMath::RoundToInt(static_cast<float>(SourceHeight) * TargetAspectRatio), 1, SourceWidth);
		CropX = (SourceWidth - CropWidth) / 2;
	}
	else if (SourceAspectRatio < TargetAspectRatio)
	{
		CropHeight = FMath::Clamp(FMath::RoundToInt(static_cast<float>(SourceWidth) / TargetAspectRatio), 1, SourceHeight);
		CropY = (SourceHeight - CropHeight) / 2;
	}

	OutPixels.SetNumUninitialized(CropWidth * CropHeight);
	for (int32 Row = 0; Row < CropHeight; ++Row)
	{
		const int32 SourceIndex = (CropY + Row) * SourceWidth + CropX;
		const int32 DestIndex = Row * CropWidth;
		FMemory::Memcpy(&OutPixels[DestIndex], &SourcePixels[SourceIndex], sizeof(FColor) * CropWidth);
	}

	OutWidth = CropWidth;
	OutHeight = CropHeight;
	return true;
}

FLevelEditorViewportClient* FindActivePerspectiveViewportClient()
{
	if (!GEditor)
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("GEditor is unavailable."));
		return nullptr;
	}

	FLevelEditorViewportClient* ViewportClient = nullptr;
	FViewport* ActiveViewport = GEditor->GetActiveViewport();

	if (GCurrentLevelEditingViewportClient && GCurrentLevelEditingViewportClient->Viewport && GCurrentLevelEditingViewportClient->IsPerspective())
	{
		if (!ActiveViewport || GCurrentLevelEditingViewportClient->Viewport == ActiveViewport)
		{
			ViewportClient = GCurrentLevelEditingViewportClient;
		}
	}

	if (!ViewportClient)
	{
		for (FLevelEditorViewportClient* Candidate : GEditor->GetLevelViewportClients())
		{
			if (!Candidate || !Candidate->Viewport || !Candidate->IsPerspective())
			{
				continue;
			}

			if (Candidate->Viewport == ActiveViewport)
			{
				ViewportClient = Candidate;
				break;
			}

			if (!ViewportClient)
			{
				ViewportClient = Candidate;
			}
		}
	}

	return ViewportClient;
}

bool ReadActiveViewportCamera(FSceneCaptureCameraInfo& OutCamera, const int32 RequestedWidth, const int32 RequestedHeight)
{
	FLevelEditorViewportClient* ViewportClient = FindActivePerspectiveViewportClient();
	if (!ViewportClient || !ViewportClient->Viewport)
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("No active perspective editor viewport is available."));
		return false;
	}

	const FIntPoint ViewportSize = ViewportClient->Viewport->GetSizeXY();
	if (ViewportSize.X <= 0 || ViewportSize.Y <= 0)
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Active viewport has invalid size: %dx%d."), ViewportSize.X, ViewportSize.Y);
		return false;
	}

	OutCamera.Location = ViewportClient->GetViewLocation();
	OutCamera.Rotation = ViewportClient->GetViewRotation();
	OutCamera.FovHorizontal = ViewportClient->ViewFOV > 0.0f ? ViewportClient->ViewFOV : 90.0f;
	OutCamera.Width = RequestedWidth > 0 ? FMath::Clamp(RequestedWidth, 16, MaxCaptureResolution) : ViewportSize.X;
	OutCamera.Height = RequestedHeight > 0 ? FMath::Clamp(RequestedHeight, 16, MaxCaptureResolution) : ViewportSize.Y;
	OutCamera.AspectRatio = OutCamera.Height > 0 ? static_cast<float>(OutCamera.Width) / static_cast<float>(OutCamera.Height) : 1.0f;
	return true;
}

UWorld* GetEditorWorld()
{
	return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

UTextureRenderTarget2D* CreateRenderTarget(const FSceneCaptureCameraInfo& Camera, const EPixelFormat PixelFormat, const bool bForceLinearGamma)
{
	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient);
	if (!RenderTarget)
	{
		return nullptr;
	}

	RenderTarget->ClearColor = FLinearColor::Black;
	RenderTarget->TargetGamma = bForceLinearGamma ? 1.0f : 2.2f;
	RenderTarget->InitCustomFormat(Camera.Width, Camera.Height, PixelFormat, bForceLinearGamma);
	RenderTarget->UpdateResourceImmediate(true);
	return RenderTarget;
}

void AddIdMaterialUsagesForPrimitive(const UPrimitiveComponent* PrimitiveComponent, TSet<EMaterialUsage>& OutUsages)
{
	if (const USplineMeshComponent* SplineMeshComponent = Cast<USplineMeshComponent>(PrimitiveComponent))
	{
		OutUsages.Add(MATUSAGE_SplineMesh);
		if (const UStaticMesh* StaticMesh = SplineMeshComponent->GetStaticMesh(); StaticMesh && StaticMesh->IsNaniteEnabled() && StaticMesh->HasValidNaniteData())
		{
			OutUsages.Add(MATUSAGE_Nanite);
		}
		return;
	}

	if (const UInstancedStaticMeshComponent* InstancedStaticMeshComponent = Cast<UInstancedStaticMeshComponent>(PrimitiveComponent))
	{
		OutUsages.Add(MATUSAGE_InstancedStaticMeshes);
		if (const UStaticMesh* StaticMesh = InstancedStaticMeshComponent->GetStaticMesh(); StaticMesh && StaticMesh->IsNaniteEnabled() && StaticMesh->HasValidNaniteData())
		{
			OutUsages.Add(MATUSAGE_Nanite);
		}
		return;
	}

	if (const UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(PrimitiveComponent))
	{
		if (const UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh(); StaticMesh && StaticMesh->IsNaniteEnabled() && StaticMesh->HasValidNaniteData())
		{
			OutUsages.Add(MATUSAGE_Nanite);
		}
		return;
	}

	if (const USkinnedMeshComponent* SkinnedMeshComponent = Cast<USkinnedMeshComponent>(PrimitiveComponent))
	{
		OutUsages.Add(MATUSAGE_SkeletalMesh);
		if (!SkinnedMeshComponent->ActiveMorphTargets.IsEmpty() || !SkinnedMeshComponent->MorphTargetWeights.IsEmpty())
		{
			OutUsages.Add(MATUSAGE_MorphTargets);
		}
	}
}

void EnsureIdMaterialUsages(UMaterialInterface* Material, const TSet<EMaterialUsage>& RequiredUsages)
{
	UMaterial* BaseMaterial = Material ? Material->GetMaterial() : nullptr;
	if (!BaseMaterial)
	{
		return;
	}

	bool bAnyUsageNeedsRecompile = false;
	for (const EMaterialUsage Usage : RequiredUsages)
	{
		bool bNeedsRecompile = false;
		UMaterialEditingLibrary::SetMaterialUsage(BaseMaterial, Usage, bNeedsRecompile);
		bAnyUsageNeedsRecompile |= bNeedsRecompile;
	}

	if (bAnyUsageNeedsRecompile)
	{
		UE_LOG(LogSceneCaptureLibrary, Display, TEXT("ID material usages were updated; waiting for ID material shader compilation: %s"), IdMaterialPath);
	}

	BaseMaterial->EnsureIsComplete();
	FlushRenderingCommands();
}

USceneCaptureComponent2D* CreateCaptureComponent(UWorld* World, const FSceneCaptureCameraInfo& Camera, UTextureRenderTarget2D* RenderTarget, const ESceneCaptureSource CaptureSource)
{
	if (!World || !RenderTarget)
	{
		return nullptr;
	}

	USceneCaptureComponent2D* CaptureComponent = NewObject<USceneCaptureComponent2D>(GetTransientPackage(), NAME_None, RF_Transient);
	if (!CaptureComponent)
	{
		return nullptr;
	}

	CaptureComponent->TextureTarget = RenderTarget;
	CaptureComponent->CaptureSource = CaptureSource;
	CaptureComponent->FOVAngle = Camera.FovHorizontal;
	CaptureComponent->ProjectionType = ECameraProjectionMode::Perspective;
	CaptureComponent->bCaptureEveryFrame = false;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->PostProcessBlendWeight = 0.0f;
	CaptureComponent->RegisterComponentWithWorld(World);
	CaptureComponent->SetWorldLocationAndRotation(Camera.Location, Camera.Rotation);
	return CaptureComponent;
}

void DestroyCaptureComponent(USceneCaptureComponent2D*& CaptureComponent)
{
	if (CaptureComponent)
	{
		CaptureComponent->TextureTarget = nullptr;
		CaptureComponent->DestroyComponent();
		CaptureComponent = nullptr;
	}
}

bool SaveActiveViewportHighResScreenshot(const FSceneCaptureCameraInfo& Camera, const FString& PngPath)
{
	FLevelEditorViewportClient* ViewportClient = FindActivePerspectiveViewportClient();
	if (!ViewportClient || !ViewportClient->Viewport)
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("No active perspective editor viewport is available for high-res screenshot."));
		return false;
	}

	FViewport* Viewport = ViewportClient->Viewport;
	FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();
	const FString PreviousFilenameOverride = Config.FilenameOverride;
	const bool bPreviousMaskEnabled = Config.bMaskEnabled;
	const bool bPreviousDateTimeBasedNaming = Config.bDateTimeBasedNaming;
	const bool bPreviousDumpBufferVisualizationTargets = Config.bDumpBufferVisualizationTargets;
	const bool bPreviousCaptureHdr = Config.bCaptureHDR;
	const bool bPreviousForce128BitRendering = Config.bForce128BitRendering;
	const FIntRect PreviousCaptureRegion = Config.CaptureRegion;
	const FIntRect PreviousUnscaledCaptureRegion = Config.UnscaledCaptureRegion;
	const float PreviousResolutionMultiplier = Config.ResolutionMultiplier;
	const float PreviousResolutionMultiplierScale = Config.ResolutionMultiplierScale;

	TArray<FColor> CapturedPixels;
	int32 CapturedWidth = 0;
	int32 CapturedHeight = 0;
	bool bScreenshotCaptured = false;
	FDelegateHandle ScreenshotCapturedHandle;

	ScreenshotCapturedHandle = FScreenshotRequest::OnScreenshotCaptured().AddLambda(
		[&CapturedPixels, &CapturedWidth, &CapturedHeight, &bScreenshotCaptured](const int32 InWidth, const int32 InHeight, const TArray<FColor>& InColors)
		{
			CapturedWidth = InWidth;
			CapturedHeight = InHeight;
			CapturedPixels = InColors;
			bScreenshotCaptured = true;
		});

	ON_SCOPE_EXIT
	{
		FScreenshotRequest::OnScreenshotCaptured().Remove(ScreenshotCapturedHandle);
		FScreenshotRequest::Reset();
		GScreenshotResolutionX = 0;
		GScreenshotResolutionY = 0;
		Config.FilenameOverride = PreviousFilenameOverride;
		Config.bMaskEnabled = bPreviousMaskEnabled;
		Config.bDateTimeBasedNaming = bPreviousDateTimeBasedNaming;
		Config.bDumpBufferVisualizationTargets = bPreviousDumpBufferVisualizationTargets;
		Config.bCaptureHDR = bPreviousCaptureHdr;
		Config.bForce128BitRendering = bPreviousForce128BitRendering;
		Config.CaptureRegion = PreviousCaptureRegion;
		Config.UnscaledCaptureRegion = PreviousUnscaledCaptureRegion;
		Config.ResolutionMultiplier = PreviousResolutionMultiplier;
		Config.ResolutionMultiplierScale = PreviousResolutionMultiplierScale;
	};

	Config.SetFilename(PngPath);
	Config.SetHDRCapture(false);
	Config.SetForce128BitRendering(false);
	Config.SetMaskEnabled(false);
	Config.bDateTimeBasedNaming = false;
	Config.bDumpBufferVisualizationTargets = false;
	if (!Config.SetResolution(Camera.Width, Camera.Height, 1.0f))
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Failed to configure high-res screenshot resolution: %dx%d."), Camera.Width, Camera.Height);
		return false;
	}

	if (!Viewport->TakeHighResScreenShot())
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Failed to request high-res screenshot."));
		return false;
	}

	for (int32 Attempt = 0; Attempt < HighResScreenshotDrawAttempts && !bScreenshotCaptured; ++Attempt)
	{
		Viewport->Draw(false);
		FlushRenderingCommands();
	}

	if (!bScreenshotCaptured)
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("High-res screenshot did not produce captured pixels."));
		return false;
	}

	if (CapturedWidth != Camera.Width || CapturedHeight != Camera.Height)
	{
		UE_LOG(LogSceneCaptureLibrary, Verbose, TEXT("High-res screenshot returned %dx%d for requested %dx%d."), CapturedWidth, CapturedHeight, Camera.Width, Camera.Height);
	}

	for (FColor& Pixel : CapturedPixels)
	{
		Pixel.A = 255;
	}

	return SavePngFile(PngPath, CapturedPixels, CapturedWidth, CapturedHeight);
}

bool CaptureColorPng(const FSceneCaptureCameraInfo& Camera, const FString& PngPath)
{
	if (SaveActiveViewportHighResScreenshot(Camera, PngPath))
	{
		return true;
	}

	UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Falling back to SceneCapture2D for scene capture: %s"), *PngPath);

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Editor world is unavailable."));
		return false;
	}

	UTextureRenderTarget2D* RenderTarget = CreateRenderTarget(Camera, PF_B8G8R8A8, false);
	if (!RenderTarget)
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Failed to create color render target."));
		return false;
	}

	USceneCaptureComponent2D* CaptureComponent = CreateCaptureComponent(World, Camera, RenderTarget, SCS_FinalColorLDR);
	if (!CaptureComponent)
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Failed to create scene capture component."));
		return false;
	}

	FAssetCompilingManager::Get().FinishAllCompilation();
	FlushRenderingCommands();

	CaptureComponent->bAlwaysPersistRenderingState = true;
	for (int32 FrameIndex = 0; FrameIndex < SceneCaptureWarmUpFrames; ++FrameIndex)
	{
		CaptureComponent->CaptureScene();
		FlushRenderingCommands();
	}

	TArray<FColor> Bitmap;
	FReadSurfaceDataFlags ReadFlags(RCM_UNorm);
	FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
	const bool bReadOk = Resource && Resource->ReadPixels(Bitmap, ReadFlags);
	DestroyCaptureComponent(CaptureComponent);

	if (!bReadOk)
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Failed to read color render target pixels."));
		return false;
	}

	for (FColor& Pixel : Bitmap)
	{
		Pixel.A = 255;
	}

	return SavePngFile(PngPath, Bitmap, Camera.Width, Camera.Height);
}

bool CaptureColorPngFast(const FSceneCaptureCameraInfo& Camera, const FString& PngPath)
{
	FLevelEditorViewportClient* ViewportClient = FindActivePerspectiveViewportClient();
	if (!ViewportClient || !ViewportClient->Viewport)
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("No active perspective editor viewport is available for fast scene capture."));
		return false;
	}

	FViewport* Viewport = ViewportClient->Viewport;
	const FIntPoint ViewportSize = Viewport->GetSizeXY();
	if (ViewportSize.X <= 0 || ViewportSize.Y <= 0)
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Active viewport has invalid size for fast scene capture: %dx%d."), ViewportSize.X, ViewportSize.Y);
		return false;
	}

	const bool bWasInGameView = ViewportClient->IsInGameView();
	if (!bWasInGameView)
	{
		ViewportClient->SetGameView(true);
	}

	ON_SCOPE_EXIT
	{
		if (!bWasInGameView)
		{
			ViewportClient->SetGameView(false);
		}
		ViewportClient->Invalidate();
	};

	Viewport->Draw(false);
	FlushRenderingCommands();

	TArray<FColor> Bitmap;
	FReadSurfaceDataFlags ReadFlags(RCM_UNorm);
	const bool bReadOk = Viewport->ReadPixels(Bitmap, ReadFlags, FIntRect(0, 0, ViewportSize.X, ViewportSize.Y));
	if (!bReadOk || Bitmap.Num() != ViewportSize.X * ViewportSize.Y)
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Failed to read active viewport pixels for fast scene capture."));
		return false;
	}

	for (FColor& Pixel : Bitmap)
	{
		Pixel.A = 255;
	}

	if (Camera.Width != ViewportSize.X || Camera.Height != ViewportSize.Y)
	{
		TArray<FColor> CroppedBitmap;
		int32 CroppedWidth = 0;
		int32 CroppedHeight = 0;
		if (!CropBitmapToAspect(Bitmap, ViewportSize.X, ViewportSize.Y, Camera.AspectRatio, CroppedBitmap, CroppedWidth, CroppedHeight))
		{
			UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Failed to crop active viewport pixels for fast scene capture."));
			return false;
		}

		TArray<FColor> ResizedBitmap;
		ResizedBitmap.SetNumUninitialized(Camera.Width * Camera.Height);
		FImageUtils::ImageResize(CroppedWidth, CroppedHeight, CroppedBitmap, Camera.Width, Camera.Height, ResizedBitmap, false);
		for (FColor& Pixel : ResizedBitmap)
		{
			Pixel.A = 255;
		}
		return SavePngFile(PngPath, ResizedBitmap, Camera.Width, Camera.Height);
	}

	return SavePngFile(PngPath, Bitmap, ViewportSize.X, ViewportSize.Y);
}

bool ActorMatchesAnyClass(const AActor* Actor, const TArray<TSubclassOf<AActor>>& TargetClasses)
{
	if (!Actor)
	{
		return false;
	}

	for (const TSubclassOf<AActor>& TargetClass : TargetClasses)
	{
		if (const UClass* Class = TargetClass.Get())
		{
			if (Actor->IsA(Class))
			{
				return true;
			}
		}
	}

	return false;
}

bool CollectTargetActors(UWorld* World, const TArray<TSubclassOf<AActor>>& TargetClasses, TArray<AActor*>& OutActors)
{
	OutActors.Reset();
	if (!World)
	{
		return false;
	}

	if (TargetClasses.Num() <= 0)
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("TargetClasses is empty."));
		return false;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor->IsTemplate() || Actor->HasAnyFlags(RF_Transient))
		{
			continue;
		}

		if (ActorMatchesAnyClass(Actor, TargetClasses))
		{
			OutActors.Add(Actor);
		}
	}

	OutActors.Sort([](const AActor& Left, const AActor& Right)
	{
		return Left.GetPathName() < Right.GetPathName();
	});

	if (OutActors.Num() > MaxRgbId)
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Too many target actors for RGB ID encoding: %d > %d."), OutActors.Num(), MaxRgbId);
		OutActors.Reset();
		return false;
	}

	return true;
}

UMaterialInterface* LoadIdMaterial()
{
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, IdMaterialPath);
	if (!Material)
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Failed to load ID material: %s. Run Content/Python/create_scene_capture_id_material.py to create it."), IdMaterialPath);
		return nullptr;
	}
	return Material;
}

void ApplyMaterialToPrimitive(UPrimitiveComponent* PrimitiveComponent, UMaterialInterface* Material, TArray<FMaterialOverrideRecord>& OutOverrides, TSet<EMaterialUsage>& OutRequiredUsages)
{
	if (!PrimitiveComponent || !PrimitiveComponent->IsRegistered() || !Material)
	{
		return;
	}

	AddIdMaterialUsagesForPrimitive(PrimitiveComponent, OutRequiredUsages);

	const int32 MaterialCount = PrimitiveComponent->GetNumMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		FMaterialOverrideRecord OverrideRecord;
		OverrideRecord.Component = PrimitiveComponent;
		OverrideRecord.MaterialIndex = MaterialIndex;
		OverrideRecord.OriginalMaterial = PrimitiveComponent->GetMaterial(MaterialIndex);
		OutOverrides.Add(OverrideRecord);

		PrimitiveComponent->SetMaterial(MaterialIndex, Material);
	}
}

bool ApplyIdMaterialOverrides(UWorld* World, const TArray<AActor*>& Actors, UMaterialInterface* IdMaterial, const bool /*bOnlyVisibleTargets*/, TArray<FIdMapEntry>& OutEntries, TArray<FMaterialOverrideRecord>& OutOverrides, TSet<EMaterialUsage>& OutRequiredUsages)
{
	OutEntries.Reset();
	OutOverrides.Reset();
	OutRequiredUsages.Reset();

	if (!World || !IdMaterial)
	{
		return false;
	}

	TSet<uint32> UsedColorKeys;
	UsedColorKeys.Reserve(Actors.Num());

	for (int32 ActorIndex = 0; ActorIndex < Actors.Num(); ++ActorIndex)
	{
		AActor* Actor = Actors[ActorIndex];
		if (!Actor)
		{
			continue;
		}
		UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(IdMaterial, Actor);
		if (!DynamicMaterial)
		{
			UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Failed to create ID dynamic material for actor: %s"), *Actor->GetPathName());
			return false;
		}

		const int32 Id = ActorIndex + 1;
		const FColor IdColor = GenerateIdColor(Id, UsedColorKeys);
		FIdMapEntry Entry;
		Entry.Id = Id;
		Entry.Color = IdColor;
		Entry.ActorPath = Actor->GetPathName();
		OutEntries.Add(Entry);

		DynamicMaterial->SetVectorParameterValue(
			IdColorParameterName,
			FLinearColor(
				static_cast<float>(IdColor.R) / 255.0f,
				static_cast<float>(IdColor.G) / 255.0f,
				static_cast<float>(IdColor.B) / 255.0f,
				1.0f));

		TArray<UPrimitiveComponent*> PrimitiveComponents;
		Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			ApplyMaterialToPrimitive(PrimitiveComponent, DynamicMaterial, OutOverrides, OutRequiredUsages);
		}
	}

	return OutEntries.Num() == Actors.Num();
}

void RestoreMaterialOverrides(const TArray<FMaterialOverrideRecord>& Overrides)
{
	for (int32 Index = Overrides.Num() - 1; Index >= 0; --Index)
	{
		const FMaterialOverrideRecord& OverrideRecord = Overrides[Index];
		if (UPrimitiveComponent* Component = OverrideRecord.Component.Get())
		{
			if (OverrideRecord.MaterialIndex != INDEX_NONE)
			{
				Component->SetMaterial(OverrideRecord.MaterialIndex, OverrideRecord.OriginalMaterial.Get());
			}
		}
	}
}

FColor QuantizeLinearColor(const FLinearColor& Pixel)
{
	FColor QuantizedColor(
		static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Pixel.R * 255.0f), 0, 255)),
		static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Pixel.G * 255.0f), 0, 255)),
		static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Pixel.B * 255.0f), 0, 255)),
		255);

	if (QuantizedColor.R <= IdColorSnapTolerance && QuantizedColor.G <= IdColorSnapTolerance && QuantizedColor.B <= IdColorSnapTolerance)
	{
		return FColor::Black;
	}

	return QuantizedColor;
}

int32 ColorDistanceSquared(const FColor& Left, const FColor& Right)
{
	const int32 DeltaR = static_cast<int32>(Left.R) - static_cast<int32>(Right.R);
	const int32 DeltaG = static_cast<int32>(Left.G) - static_cast<int32>(Right.G);
	const int32 DeltaB = static_cast<int32>(Left.B) - static_cast<int32>(Right.B);
	return DeltaR * DeltaR + DeltaG * DeltaG + DeltaB * DeltaB;
}

FColor SnapIdMapPixel(const FColor& Pixel, const TArray<FIdMapEntry>& IdMapEntries, const TSet<uint32>& IdColorKeys)
{
	if (Pixel == FColor::Black)
	{
		return FColor::Black;
	}

	if (IdColorKeys.Contains(MakeColorKey(Pixel)))
	{
		return Pixel;
	}

	const int32 MaxDistanceSquared = IdColorSnapTolerance * IdColorSnapTolerance * 3;
	const FIdMapEntry* BestEntry = nullptr;
	int32 BestDistanceSquared = MaxDistanceSquared + 1;
	for (const FIdMapEntry& Entry : IdMapEntries)
	{
		const int32 DistanceSquared = ColorDistanceSquared(Pixel, Entry.Color);
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestEntry = &Entry;
		}
	}

	return BestEntry && BestDistanceSquared <= MaxDistanceSquared ? BestEntry->Color : FColor::Black;
}

void ConfigureIdCaptureComponent(USceneCaptureComponent2D* CaptureComponent, const bool bRenderWholeSceneForVisibility, const TArray<AActor*>& ShowOnlyActors)
{
	if (!CaptureComponent)
	{
		return;
	}

	CaptureComponent->PrimitiveRenderMode = bRenderWholeSceneForVisibility
		? ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives
		: ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	if (!bRenderWholeSceneForVisibility)
	{
		for (AActor* Actor : ShowOnlyActors)
		{
			if (Actor)
			{
				CaptureComponent->ShowOnlyActorComponents(Actor);
			}
		}
	}

	CaptureComponent->ShowFlags.SetAntiAliasing(false);
	CaptureComponent->ShowFlags.SetAmbientCubemap(false);
	CaptureComponent->ShowFlags.SetAmbientOcclusion(false);
	CaptureComponent->ShowFlags.SetAtmosphere(false);
	CaptureComponent->ShowFlags.SetBloom(false);
	CaptureComponent->ShowFlags.SetDynamicShadows(false);
	CaptureComponent->ShowFlags.SetEyeAdaptation(false);
	CaptureComponent->ShowFlags.SetFog(false);
	CaptureComponent->ShowFlags.SetLighting(false);
	CaptureComponent->ShowFlags.SetMotionBlur(false);
	CaptureComponent->ShowFlags.SetPostProcessing(false);
	CaptureComponent->ShowFlags.SetSkyLighting(false);
	CaptureComponent->ShowFlags.SetTemporalAA(false);
	CaptureComponent->ShowFlags.SetTonemapper(false);
	CaptureComponent->ShowFlags.SetVolumetricFog(false);
	CaptureComponent->bAlwaysPersistRenderingState = true;
}

bool RenderIdCapture(USceneCaptureComponent2D* CaptureComponent, UTextureRenderTarget2D* RenderTarget, const TArray<FIdMapEntry>& IdMapEntries, const int32 WarmUpFrames, FIdCaptureRenderResult& OutResult)
{
	if (!CaptureComponent || !RenderTarget)
	{
		return false;
	}

	for (int32 FrameIndex = 0; FrameIndex < WarmUpFrames; ++FrameIndex)
	{
		CaptureComponent->CaptureScene();
		FlushRenderingCommands();
	}

	TArray<FLinearColor> LinearPixels;
	FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
	if (!Resource || !Resource->ReadLinearColorPixels(LinearPixels))
	{
		return false;
	}

	TSet<uint32> IdColorKeys;
	IdColorKeys.Reserve(IdMapEntries.Num());
	for (const FIdMapEntry& Entry : IdMapEntries)
	{
		IdColorKeys.Add(MakeColorKey(Entry.Color));
	}

	OutResult.Pixels.Reset();
	OutResult.Pixels.Reserve(LinearPixels.Num());
	OutResult.VisibleColorKeys.Reset();
	for (const FLinearColor& Pixel : LinearPixels)
	{
		const FColor SnappedPixel = SnapIdMapPixel(QuantizeLinearColor(Pixel), IdMapEntries, IdColorKeys);
		OutResult.Pixels.Add(SnappedPixel);
		const uint32 ColorKey = MakeColorKey(SnappedPixel);
		if (ColorKey != 0 && IdColorKeys.Contains(ColorKey))
		{
			OutResult.VisibleColorKeys.Add(ColorKey);
		}
	}

	return true;
}

TArray<AActor*> FilterActorsByVisibleIdColors(const TArray<AActor*>& TargetActors, const TArray<FIdMapEntry>& IdMapEntries, const TSet<uint32>& VisibleColorKeys)
{
	TArray<AActor*> VisibleActors;
	VisibleActors.Reserve(TargetActors.Num());
	for (int32 Index = 0; Index < TargetActors.Num() && Index < IdMapEntries.Num(); ++Index)
	{
		if (VisibleColorKeys.Contains(MakeColorKey(IdMapEntries[Index].Color)))
		{
			VisibleActors.Add(TargetActors[Index]);
		}
	}
	return VisibleActors;
}

TArray<FIdMapEntry> FilterIdMapEntriesByVisibleColors(const TArray<FIdMapEntry>& IdMapEntries, const TSet<uint32>& VisibleColorKeys)
{
	TArray<FIdMapEntry> VisibleEntries;
	VisibleEntries.Reserve(IdMapEntries.Num());
	for (const FIdMapEntry& Entry : IdMapEntries)
	{
		if (VisibleColorKeys.Contains(MakeColorKey(Entry.Color)))
		{
			VisibleEntries.Add(Entry);
		}
	}
	return VisibleEntries;
}

void UpdateIdMapEntryPixelBounds(const TArray<FColor>& Pixels, const int32 Width, const int32 Height, TArray<FIdMapEntry>& IdMapEntries)
{
	for (FIdMapEntry& Entry : IdMapEntries)
	{
		Entry.bHasPixelBounds = false;
		Entry.PixelMinX = Width;
		Entry.PixelMinY = Height;
		Entry.PixelMaxX = -1;
		Entry.PixelMaxY = -1;
	}

	if (Width <= 0 || Height <= 0 || Pixels.Num() != Width * Height || IdMapEntries.IsEmpty())
	{
		return;
	}

	TMap<uint32, int32> ColorKeyToEntryIndex;
	ColorKeyToEntryIndex.Reserve(IdMapEntries.Num());
	for (int32 Index = 0; Index < IdMapEntries.Num(); ++Index)
	{
		ColorKeyToEntryIndex.Add(MakeColorKey(IdMapEntries[Index].Color), Index);
	}

	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const uint32 ColorKey = MakeColorKey(Pixels[Y * Width + X]);
			if (int32* EntryIndex = ColorKeyToEntryIndex.Find(ColorKey))
			{
				FIdMapEntry& Entry = IdMapEntries[*EntryIndex];
				if (!Entry.bHasPixelBounds)
				{
					Entry.bHasPixelBounds = true;
					Entry.PixelMinX = X;
					Entry.PixelMinY = Y;
					Entry.PixelMaxX = X;
					Entry.PixelMaxY = Y;
				}
				else
				{
					Entry.PixelMinX = FMath::Min(Entry.PixelMinX, X);
					Entry.PixelMinY = FMath::Min(Entry.PixelMinY, Y);
					Entry.PixelMaxX = FMath::Max(Entry.PixelMaxX, X);
					Entry.PixelMaxY = FMath::Max(Entry.PixelMaxY, Y);
				}
			}
		}
	}
}

bool CaptureIdMapPng(const FSceneCaptureCameraInfo& Camera, const TArray<AActor*>& TargetActors, TArray<FIdMapEntry>& IdMapEntries, const FString& PngPath, const bool bOnlyVisibleTargets)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Editor world is unavailable."));
		return false;
	}

	UTextureRenderTarget2D* RenderTarget = CreateRenderTarget(Camera, PF_FloatRGBA, true);
	if (!RenderTarget)
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Failed to create ID map render target."));
		return false;
	}

	ON_SCOPE_EXIT
	{
		RenderTarget = nullptr;
	};

	USceneCaptureComponent2D* CaptureComponent = CreateCaptureComponent(World, Camera, RenderTarget, SCS_SceneColorHDR);
	if (!CaptureComponent)
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Failed to create ID map capture component."));
		return false;
	}

	ConfigureIdCaptureComponent(CaptureComponent, bOnlyVisibleTargets, TargetActors);

	CaptureComponent->CaptureScene();
	FlushRenderingCommands();

	FIdCaptureRenderResult CaptureResult;
	const bool bReadOk = RenderIdCapture(CaptureComponent, RenderTarget, IdMapEntries, bOnlyVisibleTargets ? IdVisibilityWarmUpFrames : SceneCaptureWarmUpFrames, CaptureResult);
	DestroyCaptureComponent(CaptureComponent);

	if (!bReadOk)
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Failed to read ID map render target pixels."));
		return false;
	}

	if (bOnlyVisibleTargets)
	{
		const TArray<AActor*> VisibleActors = FilterActorsByVisibleIdColors(TargetActors, IdMapEntries, CaptureResult.VisibleColorKeys);
		IdMapEntries = FilterIdMapEntriesByVisibleColors(IdMapEntries, CaptureResult.VisibleColorKeys);
		if (VisibleActors.IsEmpty())
		{
			TArray<FColor> EmptyPixels;
			EmptyPixels.Init(FColor::Black, Camera.Width * Camera.Height);
			UpdateIdMapEntryPixelBounds(EmptyPixels, Camera.Width, Camera.Height, IdMapEntries);
			return SavePngFile(PngPath, EmptyPixels, Camera.Width, Camera.Height);
		}

		UTextureRenderTarget2D* FinalRenderTarget = CreateRenderTarget(Camera, PF_FloatRGBA, true);
		if (!FinalRenderTarget)
		{
			UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Failed to create final ID map render target."));
			return false;
		}

		ON_SCOPE_EXIT
		{
			FinalRenderTarget = nullptr;
		};

		USceneCaptureComponent2D* FinalCaptureComponent = CreateCaptureComponent(World, Camera, FinalRenderTarget, SCS_SceneColorHDR);
		if (!FinalCaptureComponent)
		{
			UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Failed to create final ID map capture component."));
			return false;
		}

		ConfigureIdCaptureComponent(FinalCaptureComponent, false, VisibleActors);
		FIdCaptureRenderResult FinalResult;
		const bool bFinalReadOk = RenderIdCapture(FinalCaptureComponent, FinalRenderTarget, IdMapEntries, IdFinalWarmUpFrames, FinalResult);
		DestroyCaptureComponent(FinalCaptureComponent);
		if (!bFinalReadOk)
		{
			UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Failed to read final ID map render target pixels."));
			return false;
		}

		CaptureResult = MoveTemp(FinalResult);
	}

	UpdateIdMapEntryPixelBounds(CaptureResult.Pixels, Camera.Width, Camera.Height, IdMapEntries);

	return SavePngFile(PngPath, CaptureResult.Pixels, Camera.Width, Camera.Height);
}

TSharedRef<FJsonObject> CreateBaseJson(const FSceneCaptureCameraInfo& Camera)
{
	const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("timestamp"), FDateTime::UtcNow().ToIso8601());
	Json->SetObjectField(TEXT("camera"), MakeCameraJson(Camera));

	const TSharedRef<FJsonObject> ImageSizeJson = MakeShared<FJsonObject>();
	ImageSizeJson->SetNumberField(TEXT("width"), Camera.Width);
	ImageSizeJson->SetNumberField(TEXT("height"), Camera.Height);
	Json->SetObjectField(TEXT("image_size"), ImageSizeJson);
	return Json;
}

bool WriteSceneJson(const FString& JsonPath, const FString& ImagePath, const FSceneCaptureCameraInfo& Camera)
{
	const TSharedRef<FJsonObject> Json = CreateBaseJson(Camera);
	Json->SetStringField(TEXT("image"), ImagePath);
	return WriteJsonFile(JsonPath, Json);
}

bool WriteIdMapJson(const FString& JsonPath, const FString& ImagePath, const FSceneCaptureCameraInfo& Camera, const TArray<FIdMapEntry>& IdMapEntries)
{
	const TSharedRef<FJsonObject> Json = CreateBaseJson(Camera);
	Json->SetStringField(TEXT("image"), ImagePath);
	Json->SetArrayField(TEXT("id_map"), MakeIdMapJson(IdMapEntries));
	return WriteJsonFile(JsonPath, Json);
}

bool WriteCombinedJson(const FString& JsonPath, const FString& SceneImagePath, const FString& IdMapImagePath, const FSceneCaptureCameraInfo& Camera, const TArray<FIdMapEntry>& IdMapEntries)
{
	const TSharedRef<FJsonObject> Json = CreateBaseJson(Camera);
	Json->SetStringField(TEXT("scene_image"), SceneImagePath);
	Json->SetStringField(TEXT("id_map_image"), IdMapImagePath);
	Json->SetArrayField(TEXT("id_map"), MakeIdMapJson(IdMapEntries));
	return WriteJsonFile(JsonPath, Json);
}

bool CaptureIdMapInternal(const FSceneCaptureCameraInfo& Camera, const TArray<TSubclassOf<AActor>>& TargetClasses, const FString& PngPath, const FString& JsonPath, TArray<FIdMapEntry>* OutIdMapEntries, const bool bOnlyVisibleTargets)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Editor world is unavailable."));
		return false;
	}

	TArray<AActor*> TargetActors;
	if (!CollectTargetActors(World, TargetClasses, TargetActors))
	{
		return false;
	}

	UMaterialInterface* IdMaterial = LoadIdMaterial();
	if (!IdMaterial)
	{
		return false;
	}

	TArray<FIdMapEntry> IdMapEntries;
	TArray<FMaterialOverrideRecord> Overrides;
	TSet<EMaterialUsage> RequiredUsages;
	if (!ApplyIdMaterialOverrides(World, TargetActors, IdMaterial, bOnlyVisibleTargets, IdMapEntries, Overrides, RequiredUsages))
	{
		RestoreMaterialOverrides(Overrides);
		return false;
	}

	EnsureIdMaterialUsages(IdMaterial, RequiredUsages);

	const bool bCaptureOk = CaptureIdMapPng(Camera, TargetActors, IdMapEntries, PngPath, bOnlyVisibleTargets);
	RestoreMaterialOverrides(Overrides);

	if (!bCaptureOk)
	{
		return false;
	}

	if (OutIdMapEntries)
	{
		*OutIdMapEntries = IdMapEntries;
	}

	return JsonPath.IsEmpty() || WriteIdMapJson(JsonPath, PngPath, Camera, IdMapEntries);
}

bool CaptureIdMapInternalFromActors(const FSceneCaptureCameraInfo& Camera, const TArray<AActor*>& TargetActors, const FString& PngPath, const FString& JsonPath, TArray<FIdMapEntry>* OutIdMapEntries, const bool bOnlyVisibleTargets)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Editor world is unavailable."));
		return false;
	}

	TArray<AActor*> ValidActors;
	ValidActors.Reserve(TargetActors.Num());
	TSet<AActor*> SeenActors;
	for (AActor* Actor : TargetActors)
	{
		if (!Actor || Actor->IsTemplate() || Actor->HasAnyFlags(RF_Transient) || SeenActors.Contains(Actor))
		{
			continue;
		}
		SeenActors.Add(Actor);
		ValidActors.Add(Actor);
	}

	ValidActors.Sort([](const AActor& Left, const AActor& Right)
	{
		return Left.GetPathName() < Right.GetPathName();
	});

	if (ValidActors.IsEmpty())
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("TargetActors is empty."));
		return false;
	}
	if (ValidActors.Num() > MaxRgbId)
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Too many target actors for RGB ID encoding: %d > %d."), ValidActors.Num(), MaxRgbId);
		return false;
	}

	UMaterialInterface* IdMaterial = LoadIdMaterial();
	if (!IdMaterial)
	{
		return false;
	}

	TArray<FIdMapEntry> IdMapEntries;
	TArray<FMaterialOverrideRecord> Overrides;
	TSet<EMaterialUsage> RequiredUsages;
	if (!ApplyIdMaterialOverrides(World, ValidActors, IdMaterial, bOnlyVisibleTargets, IdMapEntries, Overrides, RequiredUsages))
	{
		RestoreMaterialOverrides(Overrides);
		return false;
	}

	EnsureIdMaterialUsages(IdMaterial, RequiredUsages);

	const bool bCaptureOk = CaptureIdMapPng(Camera, ValidActors, IdMapEntries, PngPath, bOnlyVisibleTargets);
	RestoreMaterialOverrides(Overrides);

	if (!bCaptureOk)
	{
		return false;
	}

	if (OutIdMapEntries)
	{
		*OutIdMapEntries = IdMapEntries;
	}

	return JsonPath.IsEmpty() || WriteIdMapJson(JsonPath, PngPath, Camera, IdMapEntries);
}
}

bool USceneCaptureLibrary::CaptureSceneFromActiveViewport(const FString& OutputDir, const FString& BaseName, const int32 Width, const int32 Height)
{
	FString AbsoluteOutputDir;
	if (!EnsureOutputDirectory(OutputDir, AbsoluteOutputDir))
	{
		return false;
	}

	FSceneCaptureCameraInfo Camera;
	if (!ReadActiveViewportCamera(Camera, Width, Height))
	{
		return false;
	}

	const FString SafeBaseName = MakeSafeBaseName(BaseName);
	const FString PngPath = FPaths::Combine(AbsoluteOutputDir, SafeBaseName + TEXT("_scene.png"));
	const FString JsonPath = FPaths::Combine(AbsoluteOutputDir, SafeBaseName + TEXT("_scene.json"));

	return CaptureColorPng(Camera, PngPath) && WriteSceneJson(JsonPath, PngPath, Camera);
}

bool USceneCaptureLibrary::CaptureSceneFastFromActiveViewport(const FString& OutputDir, const FString& BaseName, const int32 Width, const int32 Height)
{
	FString AbsoluteOutputDir;
	if (!EnsureOutputDirectory(OutputDir, AbsoluteOutputDir))
	{
		return false;
	}

	FSceneCaptureCameraInfo Camera;
	if (!ReadActiveViewportCamera(Camera, Width, Height))
	{
		return false;
	}

	const FString SafeBaseName = MakeSafeBaseName(BaseName);
	const FString PngPath = FPaths::Combine(AbsoluteOutputDir, SafeBaseName + TEXT("_scene.png"));
	const FString JsonPath = FPaths::Combine(AbsoluteOutputDir, SafeBaseName + TEXT("_scene.json"));

	return CaptureColorPngFast(Camera, PngPath) && WriteSceneJson(JsonPath, PngPath, Camera);
}

bool USceneCaptureLibrary::CaptureIdMapFromActiveViewport(const FString& OutputDir, const FString& BaseName, const TArray<TSubclassOf<AActor>>& TargetClasses, const int32 Width, const int32 Height, const bool bOnlyVisibleTargets)
{
	FString AbsoluteOutputDir;
	if (!EnsureOutputDirectory(OutputDir, AbsoluteOutputDir))
	{
		return false;
	}

	FSceneCaptureCameraInfo Camera;
	if (!ReadActiveViewportCamera(Camera, Width, Height))
	{
		return false;
	}

	const FString SafeBaseName = MakeSafeBaseName(BaseName);
	const FString PngPath = FPaths::Combine(AbsoluteOutputDir, SafeBaseName + TEXT("_id.png"));
	const FString JsonPath = FPaths::Combine(AbsoluteOutputDir, SafeBaseName + TEXT("_id.json"));

	return CaptureIdMapInternal(Camera, TargetClasses, PngPath, JsonPath, nullptr, bOnlyVisibleTargets);
}

bool USceneCaptureLibrary::CaptureSceneAndIdMap(const FString& OutputDir, const FString& BaseName, const TArray<TSubclassOf<AActor>>& TargetClasses, const int32 Width, const int32 Height, const bool bOnlyVisibleTargets)
{
	FString AbsoluteOutputDir;
	if (!EnsureOutputDirectory(OutputDir, AbsoluteOutputDir))
	{
		return false;
	}

	FSceneCaptureCameraInfo Camera;
	if (!ReadActiveViewportCamera(Camera, Width, Height))
	{
		return false;
	}

	const FString SafeBaseName = MakeSafeBaseName(BaseName);
	const FString ScenePngPath = FPaths::Combine(AbsoluteOutputDir, SafeBaseName + TEXT("_scene.png"));
	const FString IdPngPath = FPaths::Combine(AbsoluteOutputDir, SafeBaseName + TEXT("_id.png"));
	const FString CombinedJsonPath = FPaths::Combine(AbsoluteOutputDir, SafeBaseName + TEXT(".json"));

	if (!CaptureColorPng(Camera, ScenePngPath))
	{
		return false;
	}

	TArray<FIdMapEntry> IdMapEntries;
	if (!CaptureIdMapInternal(Camera, TargetClasses, IdPngPath, FString(), &IdMapEntries, bOnlyVisibleTargets))
	{
		return false;
	}

	return WriteCombinedJson(CombinedJsonPath, ScenePngPath, IdPngPath, Camera, IdMapEntries);
}

bool USceneCaptureLibrary::CaptureSceneAndIdMapFromActors(const TArray<AActor*>& TargetActors, const FString& OutputDir, const FString& BaseName, const int32 Width, const int32 Height, const bool bOnlyVisibleTargets)
{
	FString AbsoluteOutputDir;
	if (!EnsureOutputDirectory(OutputDir, AbsoluteOutputDir))
	{
		return false;
	}

	FSceneCaptureCameraInfo Camera;
	if (!ReadActiveViewportCamera(Camera, Width, Height))
	{
		return false;
	}

	const FString SafeBaseName = MakeSafeBaseName(BaseName);
	const FString ScenePngPath = FPaths::Combine(AbsoluteOutputDir, SafeBaseName + TEXT("_scene.png"));
	const FString IdPngPath = FPaths::Combine(AbsoluteOutputDir, SafeBaseName + TEXT("_id.png"));
	const FString CombinedJsonPath = FPaths::Combine(AbsoluteOutputDir, SafeBaseName + TEXT(".json"));

	if (!CaptureColorPng(Camera, ScenePngPath))
	{
		return false;
	}

	TArray<FIdMapEntry> IdMapEntries;
	if (!CaptureIdMapInternalFromActors(Camera, TargetActors, IdPngPath, FString(), &IdMapEntries, bOnlyVisibleTargets))
	{
		return false;
	}

	return WriteCombinedJson(CombinedJsonPath, ScenePngPath, IdPngPath, Camera, IdMapEntries);
}

bool USceneCaptureLibrary::CaptureSceneAndIdMapFast(const FString& OutputDir, const FString& BaseName, const TArray<TSubclassOf<AActor>>& TargetClasses, const int32 Width, const int32 Height, const bool bOnlyVisibleTargets)
{
	FString AbsoluteOutputDir;
	if (!EnsureOutputDirectory(OutputDir, AbsoluteOutputDir))
	{
		return false;
	}

	FSceneCaptureCameraInfo Camera;
	if (!ReadActiveViewportCamera(Camera, Width, Height))
	{
		return false;
	}

	const FString SafeBaseName = MakeSafeBaseName(BaseName);
	const FString ScenePngPath = FPaths::Combine(AbsoluteOutputDir, SafeBaseName + TEXT("_scene.png"));
	const FString IdPngPath = FPaths::Combine(AbsoluteOutputDir, SafeBaseName + TEXT("_id.png"));
	const FString CombinedJsonPath = FPaths::Combine(AbsoluteOutputDir, SafeBaseName + TEXT(".json"));

	if (!CaptureColorPngFast(Camera, ScenePngPath))
	{
		return false;
	}

	TArray<FIdMapEntry> IdMapEntries;
	if (!CaptureIdMapInternal(Camera, TargetClasses, IdPngPath, FString(), &IdMapEntries, bOnlyVisibleTargets))
	{
		return false;
	}

	return WriteCombinedJson(CombinedJsonPath, ScenePngPath, IdPngPath, Camera, IdMapEntries);
}

void USceneCaptureLibrary::CropImageRegionToBase64(const FString& SourceImagePath, const int32 RefWidth, const int32 RefHeight, const int32 XMin, const int32 YMin, const int32 XMax, const int32 YMax, FString& OutDataUri)
{
	OutDataUri.Reset();
	TArray<FColor> SourcePixels;
	int32 SourceWidth = 0;
	int32 SourceHeight = 0;
	if (!LoadImageFileAsPixels(SourceImagePath, SourcePixels, SourceWidth, SourceHeight))
	{
		return;
	}

	TArray<FColor> WorkingPixels;
	int32 WorkingWidth = SourceWidth;
	int32 WorkingHeight = SourceHeight;
	if (RefWidth > 0 && RefHeight > 0 && (SourceWidth != RefWidth || SourceHeight != RefHeight))
	{
		WorkingWidth = RefWidth;
		WorkingHeight = RefHeight;
		WorkingPixels.SetNumUninitialized(WorkingWidth * WorkingHeight);
		FImageUtils::ImageResize(SourceWidth, SourceHeight, SourcePixels, WorkingWidth, WorkingHeight, WorkingPixels, false);
		for (FColor& Pixel : WorkingPixels)
		{
			Pixel.A = 255;
		}
	}
	else
	{
		WorkingPixels = MoveTemp(SourcePixels);
	}

	TArray<FColor> CroppedPixels;
	int32 CroppedWidth = 0;
	int32 CroppedHeight = 0;
	if (!CropBitmapRegion(WorkingPixels, WorkingWidth, WorkingHeight, XMin, YMin, XMax, YMax, CroppedPixels, CroppedWidth, CroppedHeight))
	{
		UE_LOG(LogSceneCaptureLibrary, Warning, TEXT("Failed to crop image region from %s."), *SourceImagePath);
		return;
	}

	TArray64<uint8> PngData;
	if (!EncodePngData(CroppedPixels, CroppedWidth, CroppedHeight, PngData))
	{
		return;
	}

	OutDataUri = FString(TEXT("data:image/png;base64,")) + FBase64::Encode(PngData.GetData(), static_cast<uint32>(PngData.Num()));
}
