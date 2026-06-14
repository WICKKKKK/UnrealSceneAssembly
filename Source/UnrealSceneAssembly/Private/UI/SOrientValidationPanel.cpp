#include "UI/SOrientValidationPanel.h"

#include "Brushes/SlateDynamicImageBrush.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "IDetailsView.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "LevelEditorViewport.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "SceneCaptureLibrary.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Solver/SceneAssemblySolverLibrary.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "UnrealSceneAssembly.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SOrientValidationPanel"

namespace
{
static const FName OrientValidationResultTag(TEXT("OrientValidationResult"));
static const FLinearColor OrientValidationMutedColor(0.58f, 0.58f, 0.58f, 1.0f);

TSharedRef<SWidget> OrientValidationMakeCard(const TSharedRef<SWidget>& Content)
{
	return SNew(SBorder)
		.Padding(16.0f)
		.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
		[
			Content
		];
}

FString OrientValidationTimestampBaseName()
{
	return FString::Printf(TEXT("orient_validation_%s"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
}

FString OrientValidationGetStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, const FString& DefaultValue = FString())
{
	FString Value;
	return Object.IsValid() && Object->TryGetStringField(FieldName, Value) ? Value : DefaultValue;
}

bool OrientValidationGetBoolField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, bool DefaultValue = false)
{
	bool Value = DefaultValue;
	return Object.IsValid() && Object->TryGetBoolField(FieldName, Value) ? Value : DefaultValue;
}

int32 OrientValidationGetIntField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, int32 DefaultValue = 0)
{
	double Value = 0.0;
	return Object.IsValid() && Object->TryGetNumberField(FieldName, Value) ? static_cast<int32>(Value) : DefaultValue;
}

double OrientValidationGetNumberField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, double DefaultValue = 0.0)
{
	double Value = DefaultValue;
	return Object.IsValid() && Object->TryGetNumberField(FieldName, Value) ? Value : DefaultValue;
}

TSharedPtr<FJsonObject> OrientValidationGetObjectField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	const TSharedPtr<FJsonObject>* Child = nullptr;
	return Object.IsValid() && Object->TryGetObjectField(FieldName, Child) && Child != nullptr ? *Child : nullptr;
}

bool OrientValidationJsonVector(const TSharedPtr<FJsonObject>& Object, FVector& OutValue)
{
	if (!Object.IsValid())
	{
		return false;
	}
	OutValue.X = OrientValidationGetNumberField(Object, TEXT("x"), 0.0);
	OutValue.Y = OrientValidationGetNumberField(Object, TEXT("y"), 0.0);
	OutValue.Z = OrientValidationGetNumberField(Object, TEXT("z"), 0.0);
	return true;
}

bool OrientValidationJsonRotator(const TSharedPtr<FJsonObject>& Object, FRotator& OutValue)
{
	if (!Object.IsValid())
	{
		return false;
	}
	OutValue.Pitch = OrientValidationGetNumberField(Object, TEXT("pitch"), 0.0);
	OutValue.Yaw = OrientValidationGetNumberField(Object, TEXT("yaw"), 0.0);
	OutValue.Roll = OrientValidationGetNumberField(Object, TEXT("roll"), 0.0);
	return true;
}

bool OrientValidationLoadImageAsBgra(const FString& ImagePath, TArray<uint8>& OutRawData, int32& OutWidth, int32& OutHeight)
{
	OutRawData.Reset();
	OutWidth = 0;
	OutHeight = 0;

	TArray<uint8> CompressedData;
	if (!FFileHelper::LoadFileToArray(CompressedData, *ImagePath) || CompressedData.IsEmpty())
	{
		return false;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	const EImageFormat ImageFormat = ImageWrapperModule.DetectImageFormat(CompressedData.GetData(), CompressedData.Num());
	if (ImageFormat == EImageFormat::Invalid)
	{
		return false;
	}

	const TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat, *ImagePath);
	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(CompressedData.GetData(), CompressedData.Num()))
	{
		return false;
	}

	TArray64<uint8> RawData64;
	if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData64))
	{
		return false;
	}

	OutWidth = static_cast<int32>(ImageWrapper->GetWidth());
	OutHeight = static_cast<int32>(ImageWrapper->GetHeight());
	if (OutWidth <= 0 || OutHeight <= 0 || RawData64.Num() != static_cast<int64>(OutWidth) * OutHeight * 4)
	{
		OutWidth = 0;
		OutHeight = 0;
		return false;
	}

	OutRawData.SetNumUninitialized(static_cast<int32>(RawData64.Num()));
	FMemory::Memcpy(OutRawData.GetData(), RawData64.GetData(), RawData64.Num());
	return true;
}

TSharedPtr<FSlateDynamicImageBrush> OrientValidationLoadBrushFromImage(const FString& ImagePath, const FString& ResourceSuffix)
{
	TArray<uint8> RawData;
	int32 Width = 0;
	int32 Height = 0;
	if (!OrientValidationLoadImageAsBgra(ImagePath, RawData, Width, Height))
	{
		return nullptr;
	}

	const FString ResourceName = FString::Printf(TEXT("SceneAssembly_OrientValidation_%s_%s"), *ResourceSuffix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	return FSlateDynamicImageBrush::CreateWithImageData(FName(*ResourceName), FVector2D(static_cast<float>(Width), static_cast<float>(Height)), RawData);
}

TSharedRef<FJsonObject> OrientValidationVectorJson(const FVector& Value)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("x"), Value.X);
	Object->SetNumberField(TEXT("y"), Value.Y);
	Object->SetNumberField(TEXT("z"), Value.Z);
	return Object;
}

TSharedRef<FJsonObject> OrientValidationRotatorJson(const FRotator& Value)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("pitch"), Value.Pitch);
	Object->SetNumberField(TEXT("yaw"), Value.Yaw);
	Object->SetNumberField(TEXT("roll"), Value.Roll);
	return Object;
}

FString OrientValidationJsonObjectToString(const TSharedPtr<FJsonObject>& Object)
{
	if (!Object.IsValid())
	{
		return TEXT("");
	}

	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
	return Output;
}

bool OrientValidationTryGetTimingMs(const TSharedPtr<FJsonObject>& Timings, const TCHAR* FieldName, double& OutValue)
{
	return Timings.IsValid() && Timings->TryGetNumberField(FieldName, OutValue);
}

void OrientValidationAppendTimingLine(FString& Output, const TCHAR* Label, const TSharedPtr<FJsonObject>& Timings, const TCHAR* FieldName)
{
	double Value = 0.0;
	if (OrientValidationTryGetTimingMs(Timings, FieldName, Value))
	{
		Output += FString::Printf(TEXT("\n  %s: %.1f ms"), Label, Value);
	}
}

FVector OrientValidationVectorFromJsonArray(const TArray<TSharedPtr<FJsonValue>>* Values, const FVector& Fallback)
{
	if (Values == nullptr || Values->Num() != 3)
	{
		return Fallback;
	}
	return FVector((*Values)[0]->AsNumber(), (*Values)[1]->AsNumber(), (*Values)[2]->AsNumber());
}

FVector OrientValidationAxisFromObject(const TSharedPtr<FJsonObject>& Axes, const TCHAR* FieldName, const FVector& Fallback)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Axes.IsValid() || !Axes->TryGetArrayField(FieldName, Values))
	{
		return Fallback;
	}
	return OrientValidationVectorFromJsonArray(Values, Fallback);
}

FString OrientValidationMeshName(UStaticMesh* Mesh)
{
	return Mesh ? Mesh->GetName() : TEXT("Asset");
}
}

void SOrientValidationPanel::Construct(const FArguments& InArgs)
{
	Settings.Reset(NewObject<UOrientValidationSettings>(GetTransientPackage(), UOrientValidationSettings::StaticClass()));

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	SettingsDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	SettingsDetailsView->SetObject(Settings.Get());

	ChildSlot
	[
		SNew(SBorder)
		.Padding(18.0f)
		.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Title", "朝向验证"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 22))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Subtitle", "使用当前场景 RGB 整图和指定 Static Mesh，分别计算 Dual Image / Precomputed 的摆放 Rotation，并可直接 Spawn 到场景中对比。"))
						.ColorAndOpacity(FSlateColor(OrientValidationMutedColor))
						.AutoWrapText(true)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 18.0f, 0.0f, 12.0f)
				[
					OrientValidationMakeCard(
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("RotationHeader", "Rotation 计算")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
							[
								SNew(SButton)
								.Text(LOCTEXT("ComputeDualImage", "计算 Dual Image"))
								.IsEnabled(this, &SOrientValidationPanel::CanCompute)
								.OnClicked(this, &SOrientValidationPanel::OnComputeDualImageClicked)
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
							[
								SNew(SButton)
								.Text(LOCTEXT("SpawnDualImage", "Spawn Dual Image 结果"))
								.IsEnabled(this, &SOrientValidationPanel::CanSpawnDualImage)
								.OnClicked(this, &SOrientValidationPanel::OnSpawnDualImageClicked)
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
							[
								SNew(SButton)
								.Text(LOCTEXT("ComputePrecomputed", "计算 Precomputed"))
								.IsEnabled(this, &SOrientValidationPanel::CanCompute)
								.OnClicked(this, &SOrientValidationPanel::OnComputePrecomputedClicked)
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
							[
								SNew(SButton)
								.Text(LOCTEXT("SpawnPrecomputed", "Spawn Precomputed 结果"))
								.IsEnabled(this, &SOrientValidationPanel::CanSpawnPrecomputed)
								.OnClicked(this, &SOrientValidationPanel::OnSpawnPrecomputedClicked)
							]
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SNew(SButton)
								.Text(LOCTEXT("Cleanup", "清理验证结果"))
								.OnClicked(this, &SOrientValidationPanel::OnCleanupClicked)
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
						[
							SettingsDetailsView.ToSharedRef()
						]
					)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
				[
					OrientValidationMakeCard(
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("ReferenceHeader", "摆放参考")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("ReferenceDesc", "截取当前编辑器透视视口的 RGB 整图，并记录相机位置、旋转、FOV 和分辨率。该整图会直接作为 orient 输入。"))
							.ColorAndOpacity(FSlateColor(OrientValidationMutedColor))
							.AutoWrapText(true)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
							[
								SNew(SButton)
								.Text(LOCTEXT("CaptureScene", "截取当前场景"))
								.OnClicked(this, &SOrientValidationPanel::OnCaptureSceneClicked)
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
							[
								SNew(SButton)
								.Text(LOCTEXT("JumpCamera", "跳转到截图相机视角"))
								.IsEnabled(this, &SOrientValidationPanel::HasCaptureCamera)
								.OnClicked(this, &SOrientValidationPanel::OnJumpToCaptureCameraClicked)
							]
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SNew(SButton)
								.Text(LOCTEXT("OpenCaptureFolder", "打开目录"))
								.IsEnabled(this, &SOrientValidationPanel::HasSceneCapturePath)
								.OnClicked(this, &SOrientValidationPanel::OnOpenCaptureFolderClicked)
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(this, &SOrientValidationPanel::GetCaptureInfoText)
							.AutoWrapText(true)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
						[
							SNew(SBox)
							.HeightOverride(260.0f)
							[
								SNew(SScaleBox)
								.Stretch(EStretch::ScaleToFit)
								.StretchDirection(EStretchDirection::Both)
								[
									SNew(SImage).Image(this, &SOrientValidationPanel::GetSceneBrush)
								]
							]
						]
					)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SExpandableArea)
					.InitiallyCollapsed(false)
					.HeaderContent()[SNew(STextBlock).Text(this, &SOrientValidationPanel::GetLastResultText).Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))]
					.BodyContent()
					[
						SNew(SBox)
						.MinDesiredHeight(180.0f)
						[
							SNew(SMultiLineEditableTextBox)
							.IsReadOnly(true)
							.AutoWrapText(true)
							.Text(this, &SOrientValidationPanel::GetLogText)
						]
					]
				]
			]
		]
	];
}

bool SOrientValidationPanel::CallController(const FString& FunctionCall, TSharedPtr<FJsonObject>& OutObject)
{
	const FString Json = FUnrealSceneAssemblyModule::ExecutePythonControllerCommand(FunctionCall);
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, OutObject) || !OutObject.IsValid())
	{
		AppendLog(FString::Printf(TEXT("无效的控制器响应：%s"), *Json));
		return false;
	}
	if (!OrientValidationGetBoolField(OutObject, TEXT("ok"), false))
	{
		AppendLog(OrientValidationGetStringField(OutObject, TEXT("error"), OrientValidationGetStringField(OutObject, TEXT("message"), TEXT("控制器命令执行失败。"))));
	}
	return true;
}

FString SOrientValidationPanel::BuildPayloadJson() const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("scene_image_path"), CapturedSceneImagePath);
	Root->SetStringField(TEXT("asset_path"), GetTargetMeshAssetPath());
	Root->SetObjectField(TEXT("concept_camera_rotation"), OrientValidationRotatorJson(CaptureCameraRotation));

	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Root, Writer);
	return Output;
}

bool SOrientValidationPanel::ComputeRotation(const TCHAR* ControllerFunctionName, const FText& ModeLabel, FComputedRotation& OutRotation)
{
	if (!CanCompute())
	{
		LastResult = TEXT("请先截取场景并设置当前摆放资产。");
		AppendLog(LastResult);
		return false;
	}

	const FString PayloadJson = BuildPayloadJson();
	FTCHARToUTF8 PayloadUtf8(*PayloadJson);
	const FString PayloadBase64 = FBase64::Encode(reinterpret_cast<const uint8*>(PayloadUtf8.Get()), PayloadUtf8.Length());

	TSharedPtr<FJsonObject> Response;
	const double StartedSeconds = FPlatformTime::Seconds();
	if (!CallController(FString::Printf(TEXT("%s('%s')"), ControllerFunctionName, *PayloadBase64), Response) || !OrientValidationGetBoolField(Response, TEXT("ok"), false))
	{
		LastResult = FString::Printf(TEXT("%s 计算失败。"), *ModeLabel.ToString());
		return false;
	}
	const double RoundTripMs = (FPlatformTime::Seconds() - StartedSeconds) * 1000.0;

	if (!ApplyRotationResponse(Response, OutRotation))
	{
		LastResult = FString::Printf(TEXT("%s 响应缺少可用朝向数据。"), *ModeLabel.ToString());
		AppendLog(LastResult);
		return false;
	}

	UpdateSettingsResults();
	LastResult = FString::Printf(TEXT("%s Rotation 计算完成。"), *ModeLabel.ToString());
	AppendLog(FString::Printf(TEXT("%s\n%s\n%s"), *LastResult, *OutRotation.MetadataText, *BuildTimingReport(Response, ModeLabel, RoundTripMs)));
	return true;
}

bool SOrientValidationPanel::ApplyRotationResponse(const TSharedPtr<FJsonObject>& Response, FComputedRotation& OutRotation)
{
	const TSharedPtr<FJsonObject> RelativeAxes = OrientValidationGetObjectField(Response, TEXT("relative_orientation_axes"));
	const TSharedPtr<FJsonObject> ThumbnailCameraObject = OrientValidationGetObjectField(Response, TEXT("thumbnail_camera"));
	const TSharedPtr<FJsonObject> RelativePose = OrientValidationGetObjectField(Response, TEXT("relative_orientation"));
	if (!RelativeAxes.IsValid())
	{
		return false;
	}

	FAssetCandidate Candidate;
	Candidate.AssetPath = GetTargetMeshAssetPath();
	Candidate.BboxCenter = FVector::ZeroVector;
	Candidate.BboxHalfExtents = FVector::OneVector;
	Candidate.SemanticScore = 1.0f;
	Candidate.bHasOrientation = true;
	Candidate.RelativeOrientationX = OrientValidationAxisFromObject(RelativeAxes, TEXT("x"), FVector::ForwardVector);
	Candidate.RelativeOrientationY = OrientValidationAxisFromObject(RelativeAxes, TEXT("y"), FVector::RightVector);
	Candidate.RelativeOrientationZ = OrientValidationAxisFromObject(RelativeAxes, TEXT("z"), FVector::UpVector);
	if (ThumbnailCameraObject.IsValid() && OrientValidationJsonRotator(ThumbnailCameraObject, Candidate.ThumbnailCameraRotation))
	{
		Candidate.bHasThumbnailCamera = true;
	}

	FSolverSettings SolverSettings;
	SolverSettings.OrientMode = ESceneAssemblyOrientMode::Precomputed;
	SolverSettings.ConceptCameraRotation = CaptureCameraRotation;
	OutRotation.WorldRotation = USceneAssemblySolverLibrary::ResolveImageOrientationWorldRotation(Candidate, SolverSettings);
	OutRotation.RelativePoseText = OrientValidationJsonObjectToString(RelativePose);
	OutRotation.MetadataText = FString::Printf(
		TEXT("资产：%s\n相机：Pitch %.3f, Yaw %.3f, Roll %.3f\nRelative Pose：%s"),
		*GetTargetMeshAssetPath(),
		CaptureCameraRotation.Pitch,
		CaptureCameraRotation.Yaw,
		CaptureCameraRotation.Roll,
		*OutRotation.RelativePoseText);
	OutRotation.bValid = true;
	return true;
}

FString SOrientValidationPanel::BuildTimingReport(const TSharedPtr<FJsonObject>& Response, const FText& ModeLabel, double RoundTripMs) const
{
	const TSharedPtr<FJsonObject> Timings = OrientValidationGetObjectField(Response, TEXT("timings"));
	FString Output = FString::Printf(TEXT("%s 计算耗时："), *ModeLabel.ToString());
	OrientValidationAppendTimingLine(Output, TEXT("读取场景图"), Timings, TEXT("read_scene_ms"));
	OrientValidationAppendTimingLine(Output, TEXT("查询元数据"), Timings, TEXT("query_metadata_ms"));
	OrientValidationAppendTimingLine(Output, TEXT("下载缩略图"), Timings, TEXT("download_thumbnail_ms"));
	OrientValidationAppendTimingLine(Output, TEXT("Orient 推理(含去背)"), Timings, TEXT("orient_predict_ms"));
	OrientValidationAppendTimingLine(Output, TEXT("服务端模型(纯推理)"), Timings, TEXT("service_latency_ms"));
	OrientValidationAppendTimingLine(Output, TEXT("Python 合计"), Timings, TEXT("total_ms"));
	Output += FString::Printf(TEXT("\n  往返(含桥接): %.1f ms"), RoundTripMs);
	return Output;
}

void SOrientValidationPanel::UpdateSettingsResults()
{
	if (!Settings.IsValid())
	{
		return;
	}

	Settings->DualImageWorldRotation = DualImageResult.WorldRotation;
	Settings->DualImageRelativePose = DualImageResult.RelativePoseText;
	Settings->DualImageStatus = DualImageResult.bValid ? TEXT("已计算。") : TEXT("尚未计算。");
	Settings->PrecomputedWorldRotation = PrecomputedResult.WorldRotation;
	Settings->PrecomputedRelativePose = PrecomputedResult.RelativePoseText;
	Settings->PrecomputedStatus = PrecomputedResult.bValid ? TEXT("已计算。") : TEXT("尚未计算。");
	if (SettingsDetailsView.IsValid())
	{
		SettingsDetailsView->ForceRefresh();
	}
}

void SOrientValidationPanel::LoadCaptureMetadataFromJson()
{
	bHasCaptureCamera = false;
	CaptureImageWidth = 0;
	CaptureImageHeight = 0;
	if (CapturedJsonPath.IsEmpty())
	{
		return;
	}

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *CapturedJsonPath))
	{
		AppendLog(FString::Printf(TEXT("无法读取捕获 JSON：%s"), *CapturedJsonPath));
		return;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		AppendLog(FString::Printf(TEXT("捕获 JSON 无法解析：%s"), *CapturedJsonPath));
		return;
	}

	const TSharedPtr<FJsonObject> Camera = OrientValidationGetObjectField(Root, TEXT("camera"));
	if (Camera.IsValid())
	{
		const TSharedPtr<FJsonObject> Location = OrientValidationGetObjectField(Camera, TEXT("location"));
		const TSharedPtr<FJsonObject> Rotation = OrientValidationGetObjectField(Camera, TEXT("rotation"));
		bHasCaptureCamera = OrientValidationJsonVector(Location, CaptureCameraLocation) && OrientValidationJsonRotator(Rotation, CaptureCameraRotation);
		CaptureCameraFov = OrientValidationGetNumberField(Camera, TEXT("fov_horizontal"), 90.0);
		if (const TSharedPtr<FJsonObject> Resolution = OrientValidationGetObjectField(Camera, TEXT("resolution")))
		{
			CaptureImageWidth = OrientValidationGetIntField(Resolution, TEXT("width"), 0);
			CaptureImageHeight = OrientValidationGetIntField(Resolution, TEXT("height"), 0);
		}
	}

	if (CaptureImageWidth <= 0 || CaptureImageHeight <= 0)
	{
		if (const TSharedPtr<FJsonObject> ImageSize = OrientValidationGetObjectField(Root, TEXT("image_size")))
		{
			CaptureImageWidth = OrientValidationGetIntField(ImageSize, TEXT("width"), 0);
			CaptureImageHeight = OrientValidationGetIntField(ImageSize, TEXT("height"), 0);
		}
	}
}

void SOrientValidationPanel::RefreshSceneBrush()
{
	SceneBrush.Reset();
	if (!CapturedSceneImagePath.IsEmpty())
	{
		SceneBrush = OrientValidationLoadBrushFromImage(CapturedSceneImagePath, TEXT("Scene"));
	}
}

void SOrientValidationPanel::AppendLog(const FString& Message)
{
	LogText = Message;
}

FString SOrientValidationPanel::GetTargetMeshAssetPath() const
{
	UStaticMesh* Mesh = Settings.IsValid() ? Settings->TargetMesh.Get() : nullptr;
	if (!Mesh)
	{
		return FString();
	}
	FString AssetPath = Mesh->GetPathName();
	int32 DotIndex = INDEX_NONE;
	if (AssetPath.FindChar(TEXT('.'), DotIndex))
	{
		AssetPath.LeftInline(DotIndex);
	}
	return AssetPath;
}

FString SOrientValidationPanel::GetTargetMeshName() const
{
	return OrientValidationMeshName(Settings.IsValid() ? Settings->TargetMesh.Get() : nullptr);
}

FReply SOrientValidationPanel::OpenContainingFolder(const FString& FilePath)
{
	if (!FilePath.IsEmpty())
	{
		const FString FolderPath = FPaths::ConvertRelativePathToFull(FPaths::GetPath(FilePath));
		if (!FolderPath.IsEmpty())
		{
			FPlatformProcess::ExploreFolder(*FolderPath);
		}
	}
	return FReply::Handled();
}

AActor* SOrientValidationPanel::SpawnResultActor(const FComputedRotation& RotationResult, const FString& LabelSuffix, const FVector& Location)
{
	UStaticMesh* Mesh = Settings.IsValid() ? Settings->TargetMesh.Get() : nullptr;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!Mesh || !World || !RotationResult.bValid)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = MakeUniqueObjectName(World, AStaticMeshActor::StaticClass(), FName(*FString::Printf(TEXT("OrientValidation_%s"), *LabelSuffix)));
	AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, RotationResult.WorldRotation, SpawnParameters);
	if (!Actor)
	{
		return nullptr;
	}

	Actor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
	Actor->SetActorScale3D(FVector::OneVector);
	Actor->Tags.AddUnique(OrientValidationResultTag);
	Actor->SetActorLabel(FString::Printf(TEXT("OrientValidation_%s_%s"), *LabelSuffix, *GetTargetMeshName()));
	return Actor;
}

FReply SOrientValidationPanel::OnCaptureSceneClicked()
{
	CaptureBaseName = OrientValidationTimestampBaseName();
	CaptureOutputDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealSceneAssembly"), TEXT("OrientValidation"), CaptureBaseName);
	CapturedSceneImagePath = FPaths::Combine(CaptureOutputDir, CaptureBaseName + TEXT("_scene.png"));
	CapturedJsonPath = FPaths::Combine(CaptureOutputDir, CaptureBaseName + TEXT("_scene.json"));

	if (!USceneCaptureLibrary::CaptureSceneFromActiveViewport(CaptureOutputDir, CaptureBaseName, 0, 0))
	{
		LastResult = TEXT("场景截图失败。请确认当前有活动透视视口。");
		AppendLog(LastResult);
		return FReply::Handled();
	}

	LoadCaptureMetadataFromJson();
	RefreshSceneBrush();
	DualImageResult = FComputedRotation();
	PrecomputedResult = FComputedRotation();
	UpdateSettingsResults();
	LastResult = TEXT("已截取当前场景。 ");
	AppendLog(LastResult);
	return FReply::Handled();
}

FReply SOrientValidationPanel::OnJumpToCaptureCameraClicked()
{
	if (!bHasCaptureCamera || !GCurrentLevelEditingViewportClient)
	{
		return FReply::Handled();
	}

	FLevelEditorViewportClient* ViewportClient = GCurrentLevelEditingViewportClient;
	ViewportClient->SetViewLocation(CaptureCameraLocation);
	ViewportClient->SetViewRotation(CaptureCameraRotation);
	ViewportClient->ViewFOV = CaptureCameraFov;
	ViewportClient->FOVAngle = CaptureCameraFov;
	if (ViewportClient->Viewport)
	{
		ViewportClient->Viewport->Invalidate();
	}
	LastResult = TEXT("已跳转到截图相机视角。");
	AppendLog(LastResult);
	return FReply::Handled();
}

FReply SOrientValidationPanel::OnComputeDualImageClicked()
{
	ComputeRotation(TEXT("compute_dual_image_rotation_json"), LOCTEXT("DualImageLabel", "Dual Image"), DualImageResult);
	return FReply::Handled();
}

FReply SOrientValidationPanel::OnComputePrecomputedClicked()
{
	ComputeRotation(TEXT("compute_precomputed_rotation_json"), LOCTEXT("PrecomputedLabel", "Precomputed"), PrecomputedResult);
	return FReply::Handled();
}

FReply SOrientValidationPanel::OnSpawnDualImageClicked()
{
	const FScopedTransaction Transaction(LOCTEXT("SpawnDualImageTransaction", "Scene Assembly: Spawn Dual Image Orient Validation"));
	AActor* Actor = SpawnResultActor(DualImageResult, TEXT("DualImage"), FVector(-150.0, 0.0, 0.0));
	LastResult = Actor ? TEXT("已 Spawn Dual Image 结果到场景。") : TEXT("Spawn Dual Image 结果失败。");
	AppendLog(LastResult);
	return FReply::Handled();
}

FReply SOrientValidationPanel::OnSpawnPrecomputedClicked()
{
	const FScopedTransaction Transaction(LOCTEXT("SpawnPrecomputedTransaction", "Scene Assembly: Spawn Precomputed Orient Validation"));
	AActor* Actor = SpawnResultActor(PrecomputedResult, TEXT("Precomputed"), FVector(150.0, 0.0, 0.0));
	LastResult = Actor ? TEXT("已 Spawn Precomputed 结果到场景。") : TEXT("Spawn Precomputed 结果失败。");
	AppendLog(LastResult);
	return FReply::Handled();
}

FReply SOrientValidationPanel::OnCleanupClicked()
{
	int32 Deleted = 0;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (World)
	{
		const FScopedTransaction Transaction(LOCTEXT("CleanupTransaction", "Scene Assembly: Cleanup Orient Validation Results"));
		TArray<AActor*> ActorsToDelete;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (Actor && Actor->Tags.Contains(OrientValidationResultTag))
			{
				ActorsToDelete.Add(Actor);
			}
		}

		for (AActor* Actor : ActorsToDelete)
		{
			World->EditorDestroyActor(Actor, true);
			++Deleted;
		}
	}

	LastResult = FString::Printf(TEXT("清理删除了 %d 个朝向验证 Actor。"), Deleted);
	AppendLog(LastResult);
	return FReply::Handled();
}

FReply SOrientValidationPanel::OnOpenCaptureFolderClicked()
{
	return OpenContainingFolder(CapturedSceneImagePath);
}

FText SOrientValidationPanel::GetCaptureInfoText() const
{
	if (CapturedJsonPath.IsEmpty())
	{
		return LOCTEXT("NoCaptureInfo", "尚未截取当前场景。");
	}
	return FText::FromString(FString::Printf(
		TEXT("分辨率：%dx%d\n相机：Loc(%.1f, %.1f, %.1f), Rot(%.1f, %.1f, %.1f), FOV %.1f"),
		CaptureImageWidth,
		CaptureImageHeight,
		CaptureCameraLocation.X,
		CaptureCameraLocation.Y,
		CaptureCameraLocation.Z,
		CaptureCameraRotation.Pitch,
		CaptureCameraRotation.Yaw,
		CaptureCameraRotation.Roll,
		CaptureCameraFov));
}

FText SOrientValidationPanel::GetLastResultText() const
{
	return FText::FromString(LastResult);
}

FText SOrientValidationPanel::GetLogText() const
{
	return FText::FromString(LogText);
}

const FSlateBrush* SOrientValidationPanel::GetSceneBrush() const
{
	return SceneBrush.IsValid() ? SceneBrush.Get() : FCoreStyle::Get().GetBrush("NoBrush");
}

bool SOrientValidationPanel::HasCaptureCamera() const
{
	return bHasCaptureCamera;
}

bool SOrientValidationPanel::HasSceneCapturePath() const
{
	return !CapturedSceneImagePath.IsEmpty();
}

bool SOrientValidationPanel::CanCompute() const
{
	return !CapturedSceneImagePath.IsEmpty() && bHasCaptureCamera && Settings.IsValid() && Settings->TargetMesh != nullptr;
}

bool SOrientValidationPanel::CanSpawnDualImage() const
{
	return DualImageResult.bValid && Settings.IsValid() && Settings->TargetMesh != nullptr;
}

bool SOrientValidationPanel::CanSpawnPrecomputed() const
{
	return PrecomputedResult.bValid && Settings.IsValid() && Settings->TargetMesh != nullptr;
}

#undef LOCTEXT_NAMESPACE
