#include "UI/SOrientValidationPanel.h"

#include "Brushes/SlateDynamicImageBrush.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "DrawDebugHelpers.h"
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
static constexpr int32 OrientValidationMaxCaptureEdge = 1280;
static constexpr double OrientValidationSingleImageAxisLength = 200.0;
static constexpr float OrientValidationSingleImageOriginPointSize = 12.0f;
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

FString OrientValidationPoseToText(const FVector& Pose)
{
	return FString::Printf(
		TEXT("azimuth %.3f, polar %.3f, rotation %.3f"),
		Pose.X,
		Pose.Y,
		Pose.Z);
}

FString OrientValidationRotatorToText(const FRotator& Rotation)
{
	return FString::Printf(
		TEXT("Pitch %+.3f, Yaw %+.3f, Roll %+.3f"),
		Rotation.Pitch,
		Rotation.Yaw,
		Rotation.Roll);
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

void OrientValidationCaptureResolution(int32& OutWidth, int32& OutHeight)
{
	OutWidth = 0;
	OutHeight = 0;
	if (!GCurrentLevelEditingViewportClient || !GCurrentLevelEditingViewportClient->Viewport)
	{
		return;
	}

	const FIntPoint ViewportSize = GCurrentLevelEditingViewportClient->Viewport->GetSizeXY();
	const int32 SourceWidth = ViewportSize.X;
	const int32 SourceHeight = ViewportSize.Y;
	const int32 LongEdge = FMath::Max(SourceWidth, SourceHeight);
	if (SourceWidth <= 0 || SourceHeight <= 0 || LongEdge <= OrientValidationMaxCaptureEdge)
	{
		return;
	}

	const float Scale = static_cast<float>(OrientValidationMaxCaptureEdge) / static_cast<float>(LongEdge);
	OutWidth = FMath::Max(16, FMath::RoundToInt(static_cast<float>(SourceWidth) * Scale));
	OutHeight = FMath::Max(16, FMath::RoundToInt(static_cast<float>(SourceHeight) * Scale));
}

bool OrientValidationPoseFromObject(const TSharedPtr<FJsonObject>& Pose, FVector& OutPose)
{
	if (!Pose.IsValid())
	{
		return false;
	}
	OutPose = FVector(
		OrientValidationGetNumberField(Pose, TEXT("azimuth"), 0.0),
		OrientValidationGetNumberField(Pose, TEXT("polar"), OrientValidationGetNumberField(Pose, TEXT("elevation"), 0.0)),
		OrientValidationGetNumberField(Pose, TEXT("rotation"), 0.0));
	return true;
}

FString OrientValidationMeshName(UStaticMesh* Mesh)
{
	return Mesh ? Mesh->GetName() : TEXT("Asset");
}

const TCHAR* OrientValidationAxisOrderLabel(EOrientValidationAxisOrder AxisOrder)
{
	switch (AxisOrder)
	{
	case EOrientValidationAxisOrder::XYZ:
		return TEXT("XYZ");
	case EOrientValidationAxisOrder::XZY:
		return TEXT("XZY");
	case EOrientValidationAxisOrder::YXZ:
		return TEXT("YXZ");
	case EOrientValidationAxisOrder::YZX:
		return TEXT("YZX");
	case EOrientValidationAxisOrder::ZXY:
		return TEXT("ZXY");
	case EOrientValidationAxisOrder::ZYX:
		return TEXT("ZYX");
	default:
		return TEXT("XYZ");
	}
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
	SettingsDetailsView->OnFinishedChangingProperties().AddSP(this, &SOrientValidationPanel::OnSettingsFinishedChangingProperties);

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
						.Text(LOCTEXT("Subtitle", "使用当前场景 RGB 整图和指定 Static Mesh，计算 Dual Image 摆放 Rotation，并可直接 Spawn 到场景中验证。"))
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
								.Text(LOCTEXT("ComputeSingleImage", "计算单图朝向"))
								.IsEnabled(this, &SOrientValidationPanel::CanComputeSingleImage)
								.OnClicked(this, &SOrientValidationPanel::OnComputeSingleImageClicked)
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
							[
								SNew(SButton)
								.Text(LOCTEXT("SpawnSingleImage", "Spawn 单图结果"))
								.IsEnabled(this, &SOrientValidationPanel::CanSpawnSingleImage)
								.OnClicked(this, &SOrientValidationPanel::OnSpawnSingleImageClicked)
							]
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
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SNew(SButton)
								.Text(LOCTEXT("Cleanup", "清理验证结果"))
								.OnClicked(this, &SOrientValidationPanel::OnCleanupClicked)
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
							[
								SNew(SButton)
								.Text(LOCTEXT("DrawSingleImageAxes", "绘制世界三轴"))
								.IsEnabled(this, &SOrientValidationPanel::CanDrawSingleImageAxes)
								.OnClicked(this, &SOrientValidationPanel::OnDrawSingleImageAxesClicked)
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
							[
								SNew(SButton)
								.Text(LOCTEXT("CycleSingleImageDirection", "切换 Direction"))
								.OnClicked(this, &SOrientValidationPanel::OnCycleSingleImageDirectionClicked)
							]
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SNew(SButton)
								.Text(LOCTEXT("ClearSingleImageAxes", "清理调试三轴"))
								.OnClicked(this, &SOrientValidationPanel::OnClearSingleImageAxesClicked)
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

FString SOrientValidationPanel::BuildSingleImagePayloadJson() const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("scene_image_path"), CapturedSceneImagePath);
	Root->SetBoolField(TEXT("do_rm_bkg"), true);

	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Root, Writer);
	return Output;
}

int32 SOrientValidationPanel::GetSingleImageBasisCandidateIndex() const
{
	const UOrientValidationSettings* SettingsPtr = Settings.IsValid() ? Settings.Get() : nullptr;
	const int32 PermutationIndex = SettingsPtr ? static_cast<int32>(SettingsPtr->SingleImageAxisOrder) : 0;
	int32 SignMask = 0;
	if (SettingsPtr && SettingsPtr->bSingleImageFlipColumn0)
	{
		SignMask |= 4;
	}
	if (SettingsPtr && SettingsPtr->bSingleImageFlipColumn1)
	{
		SignMask |= 2;
	}
	if (SettingsPtr && SettingsPtr->bSingleImageFlipColumn2)
	{
		SignMask |= 1;
	}
	return FMath::Clamp(PermutationIndex, 0, 5) * 8 + SignMask;
}

FString SOrientValidationPanel::GetSingleImageBasisSummary() const
{
	const UOrientValidationSettings* SettingsPtr = Settings.IsValid() ? Settings.Get() : nullptr;
	const int32 BasisIndex = GetSingleImageBasisCandidateIndex();
	return FString::Printf(
		TEXT("[%d/%d] %s | AxisOrder=%s, FlipCol0=%s, FlipCol1=%s, FlipCol2=%s, SwapFR=%s"),
		BasisIndex + 1,
		FMath::Max(1, USceneAssemblySolverLibrary::GetSingleImageBasisCandidateCount()),
		*USceneAssemblySolverLibrary::GetSingleImageBasisCandidateLabel(BasisIndex),
		OrientValidationAxisOrderLabel(SettingsPtr ? SettingsPtr->SingleImageAxisOrder : EOrientValidationAxisOrder::XYZ),
		SettingsPtr && SettingsPtr->bSingleImageFlipColumn0 ? TEXT("true") : TEXT("false"),
		SettingsPtr && SettingsPtr->bSingleImageFlipColumn1 ? TEXT("true") : TEXT("false"),
		SettingsPtr && SettingsPtr->bSingleImageFlipColumn2 ? TEXT("true") : TEXT("false"),
		SettingsPtr && SettingsPtr->bSingleImageSwapFrontRight ? TEXT("true") : TEXT("false"));
}

FString SOrientValidationPanel::BuildAxesText(const FVector& FrontWorld, const FVector& RightWorld, const FVector& UpWorld) const
{
	const auto AngleToUnitAxisDeg = [](const FVector& Vector, const FVector& UnitAxis) -> double
	{
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Vector, UnitAxis), -1.0, 1.0)));
	};
	const double FrontAngleX = AngleToUnitAxisDeg(FrontWorld, FVector::ForwardVector);
	const double RightAngleY = AngleToUnitAxisDeg(RightWorld, FVector::RightVector);
	const double UpAngleZ = AngleToUnitAxisDeg(UpWorld, FVector::UpVector);
	const FRotator FrontRotation = FrontWorld.ToOrientationRotator();
	const FRotator RightRotation = RightWorld.ToOrientationRotator();
	const FRotator UpRotation = UpWorld.ToOrientationRotator();
	return FString::Printf(
		TEXT("Front(红): Vec(%+.3f, %+.3f, %+.3f), 与世界X夹角 %.2f°, Pitch %+.2f, Yaw %+.2f\nRight(绿): Vec(%+.3f, %+.3f, %+.3f), 与世界Y夹角 %.2f°, Pitch %+.2f, Yaw %+.2f\nUp  (蓝): Vec(%+.3f, %+.3f, %+.3f), 与世界Z夹角 %.2f°, Pitch %+.2f, Yaw %+.2f"),
		FrontWorld.X, FrontWorld.Y, FrontWorld.Z,
		FrontAngleX, FrontRotation.Pitch, FrontRotation.Yaw,
		RightWorld.X, RightWorld.Y, RightWorld.Z,
		RightAngleY, RightRotation.Pitch, RightRotation.Yaw,
		UpWorld.X, UpWorld.Y, UpWorld.Z,
		UpAngleZ, UpRotation.Pitch, UpRotation.Yaw);
}

void SOrientValidationPanel::DrawSingleImageAxes(bool bClearExisting)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World || !bHasSingleImageResult || !bHasCaptureCamera)
	{
		return;
	}

	if (bClearExisting)
	{
		FlushPersistentDebugLines(World);
	}

	FVector PoseWithOffset = SingleImageOrientPose;
	PoseWithOffset.X = FMath::Fmod(SingleImageOrientPose.X + GetSingleImageDirectionAzimuthOffset() + 360.0, 360.0);

	const int32 BasisCandidateIndex = GetSingleImageBasisCandidateIndex();
	const bool bComputeRotation = !Settings.IsValid() || Settings->bSingleImageComputeRotation;
	const bool bSwapFrontRight = !Settings.IsValid() || Settings->bSingleImageSwapFrontRight;
	FVector FrontWorld = FVector::ForwardVector;
	FVector RightWorld = FVector::RightVector;
	FVector UpWorld = FVector::UpVector;
	if (bComputeRotation)
	{
		SingleImageWorldRotation = USceneAssemblySolverLibrary::ComputeSingleImageWorldRotation(PoseWithOffset, CaptureCameraRotation, BasisCandidateIndex, bSwapFrontRight);
		const FRotationMatrix RotationMatrix(SingleImageWorldRotation);
		FrontWorld = RotationMatrix.GetScaledAxis(EAxis::X).GetSafeNormal();
		RightWorld = RotationMatrix.GetScaledAxis(EAxis::Y).GetSafeNormal();
		UpWorld = RotationMatrix.GetScaledAxis(EAxis::Z).GetSafeNormal();
	}
	else
	{
		USceneAssemblySolverLibrary::ComputeSingleImageWorldAxes(PoseWithOffset, CaptureCameraRotation, BasisCandidateIndex, bSwapFrontRight, FrontWorld, RightWorld, UpWorld);
		SingleImageWorldRotation = FRotationMatrix::MakeFromXZ(FrontWorld, UpWorld).Rotator();
	}
	SingleImageAxesText = BuildAxesText(FrontWorld, RightWorld, UpWorld);

	const FVector Origin = FVector::ZeroVector;
	DrawDebugPoint(World, Origin, OrientValidationSingleImageOriginPointSize, FColor::White, true, -1.0f, SDPG_World);
	DrawDebugLine(World, Origin, Origin + FrontWorld * OrientValidationSingleImageAxisLength, FColor::Red, true, -1.0f, SDPG_World, 3.0f);
	DrawDebugLine(World, Origin, Origin + RightWorld * OrientValidationSingleImageAxisLength, FColor::Green, true, -1.0f, SDPG_World, 3.0f);
	DrawDebugLine(World, Origin, Origin + UpWorld * OrientValidationSingleImageAxisLength, FColor::Blue, true, -1.0f, SDPG_World, 3.0f);
}

FRotator SOrientValidationPanel::ComputeSingleImageWorldRotationForCurrentDirection() const
{
	if (!bHasSingleImageResult || !bHasCaptureCamera)
	{
		return FRotator::ZeroRotator;
	}

	FVector PoseWithOffset = SingleImageOrientPose;
	PoseWithOffset.X = FMath::Fmod(SingleImageOrientPose.X + GetSingleImageDirectionAzimuthOffset() + 360.0, 360.0);

	const int32 BasisCandidateIndex = GetSingleImageBasisCandidateIndex();
	const bool bComputeRotation = !Settings.IsValid() || Settings->bSingleImageComputeRotation;
	const bool bSwapFrontRight = !Settings.IsValid() || Settings->bSingleImageSwapFrontRight;
	if (bComputeRotation)
	{
		return USceneAssemblySolverLibrary::ComputeSingleImageWorldRotation(PoseWithOffset, CaptureCameraRotation, BasisCandidateIndex, bSwapFrontRight);
	}

	FVector FrontWorld = FVector::ForwardVector;
	FVector RightWorld = FVector::RightVector;
	FVector UpWorld = FVector::UpVector;
	USceneAssemblySolverLibrary::ComputeSingleImageWorldAxes(PoseWithOffset, CaptureCameraRotation, BasisCandidateIndex, bSwapFrontRight, FrontWorld, RightWorld, UpWorld);
	return FRotationMatrix::MakeFromXZ(FrontWorld, UpWorld).Rotator();
}

void SOrientValidationPanel::DrawRotationResultAxes(const FComputedRotation& RotationResult, const FString& Label, bool bClearExisting)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World || !RotationResult.bValid)
	{
		return;
	}

	if (bClearExisting)
	{
		FlushPersistentDebugLines(World);
	}

	const FRotationMatrix RotationMatrix(RotationResult.WorldRotation);
	const FVector FrontWorld = RotationMatrix.GetScaledAxis(EAxis::X).GetSafeNormal();
	const FVector RightWorld = RotationMatrix.GetScaledAxis(EAxis::Y).GetSafeNormal();
	const FVector UpWorld = RotationMatrix.GetScaledAxis(EAxis::Z).GetSafeNormal();
	const FString AxesText = BuildAxesText(FrontWorld, RightWorld, UpWorld);
	if (Label.Equals(TEXT("Dual Image"), ESearchCase::IgnoreCase))
	{
		DualImageAxesText = AxesText;
	}

	const FVector Origin = FVector::ZeroVector;
	DrawDebugPoint(World, Origin, OrientValidationSingleImageOriginPointSize, FColor::White, true, -1.0f, SDPG_World);
	DrawDebugLine(World, Origin, Origin + FrontWorld * OrientValidationSingleImageAxisLength, FColor::Red, true, -1.0f, SDPG_World, 2.0f);
	DrawDebugLine(World, Origin, Origin + RightWorld * OrientValidationSingleImageAxisLength, FColor::Green, true, -1.0f, SDPG_World, 2.0f);
	DrawDebugLine(World, Origin, Origin + UpWorld * OrientValidationSingleImageAxisLength, FColor::Blue, true, -1.0f, SDPG_World, 2.0f);
}

void SOrientValidationPanel::DrawActiveAxes(bool bClearExisting)
{
	if (ActiveResult == EActiveOrientResult::SingleImage)
	{
		DrawSingleImageAxes(bClearExisting);
		return;
	}
	if (ActiveResult == EActiveOrientResult::DualImage)
	{
		SyncDualImageSelectedBranch();
		DrawRotationResultAxes(DualImageResult, TEXT("Dual Image"), bClearExisting);
	}
}

void SOrientValidationPanel::SyncDualImageSelectedBranch()
{
	if (!DualImageResult.bValid || DualImageResult.BranchRotations.IsEmpty())
	{
		DualImageDirectionIndex = 0;
		return;
	}

	DualImageDirectionIndex = FMath::Clamp(DualImageDirectionIndex, 0, DualImageResult.BranchRotations.Num() - 1);
	DualImageResult.WorldRotation = DualImageResult.BranchRotations[DualImageDirectionIndex];
	DualImageResult.TargetOrientPose = DualImageResult.BranchTargetOrientPoses.IsValidIndex(DualImageDirectionIndex)
		? DualImageResult.BranchTargetOrientPoses[DualImageDirectionIndex]
		: FVector::ZeroVector;
	DualImageResult.bHasTargetOrientPose = DualImageResult.BranchTargetOrientPoses.IsValidIndex(DualImageDirectionIndex);
}

void SOrientValidationPanel::OnSettingsFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	const bool bChangedSingleImageSettings =
		PropertyName == GET_MEMBER_NAME_CHECKED(UOrientValidationSettings, bSingleImageComputeRotation) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UOrientValidationSettings, bSingleImageSwapFrontRight) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UOrientValidationSettings, SingleImageAxisOrder) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UOrientValidationSettings, bSingleImageFlipColumn0) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UOrientValidationSettings, bSingleImageFlipColumn1) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UOrientValidationSettings, bSingleImageFlipColumn2);
	if (bChangedSingleImageSettings)
	{
		if (DualImageResult.bValid)
		{
			RecomputeDualImageResultForCurrentSettings(false);
		}
		const bool bRedrewAxes = CanDrawActiveAxes();
		if (bRedrewAxes)
		{
			DrawActiveAxes(true);
		}
		if (bRedrewAxes || DualImageResult.bValid || bHasSingleImageResult)
		{
			UpdateSettingsResults();
			LastResult = bRedrewAxes ? TEXT("朝向参数已更新，已自动重绘当前结果的世界三轴。 ") : TEXT("朝向参数已更新。 ");
			AppendLog(FString::Printf(TEXT("%s\n%s"), *LastResult, *GetSingleImageInfoText().ToString()));
		}
	}
}

int32 SOrientValidationPanel::GetSingleImageDirectionCount() const
{
	if (SingleImageNumDirections == 2)
	{
		return 2;
	}
	if (SingleImageNumDirections == 4)
	{
		return 4;
	}
	return 1;
}

double SOrientValidationPanel::GetSingleImageDirectionAzimuthOffset() const
{
	const int32 DirectionCount = FMath::Max(1, GetSingleImageDirectionCount());
	const int32 DirectionIndex = FMath::Clamp(SingleImageDirectionIndex, 0, DirectionCount - 1);
	return 360.0 * static_cast<double>(DirectionIndex) / static_cast<double>(DirectionCount);
}

int32 SOrientValidationPanel::GetDualImageDirectionCount() const
{
	return FMath::Max(1, DualImageResult.BranchRotations.Num());
}

int32 SOrientValidationPanel::GetDualImageAzimuthOffset() const
{
	return DualImageResult.BranchAzimuthOffsets.IsValidIndex(DualImageDirectionIndex)
		? DualImageResult.BranchAzimuthOffsets[DualImageDirectionIndex]
		: 0;
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
	OutRotation = FComputedRotation();
	const TSharedPtr<FJsonObject> ThumbnailCameraObject = OrientValidationGetObjectField(Response, TEXT("thumbnail_camera"));
	const TSharedPtr<FJsonObject> RefPose = OrientValidationGetObjectField(Response, TEXT("ref_pose"));

	const FString Mode = OrientValidationGetStringField(Response, TEXT("mode"));
	const bool bIsDualImage = Mode.Equals(TEXT("DualImage"), ESearchCase::IgnoreCase);
	if (!bIsDualImage)
	{
		return false;
	}

	// Shared thumbnail camera (asset-local space) for all branches.
	FRotator ThumbnailCameraRotation = USceneAssemblySolverLibrary::GetDefaultThumbnailCameraRotation();
	const bool bHasThumbnailCamera = ThumbnailCameraObject.IsValid() && OrientValidationJsonRotator(ThumbnailCameraObject, ThumbnailCameraRotation);
	FVector RefOrientPose = FVector::ZeroVector;
	const bool bHasRefPose = OrientValidationPoseFromObject(RefPose, RefOrientPose);
	if (!bHasRefPose)
	{
		return false;
	}

	FSolverSettings SolverSettings;
	SolverSettings.OrientMode = ESceneAssemblyOrientMode::DualImage;
	SolverSettings.ConceptCameraRotation = CaptureCameraRotation;
	SolverSettings.OrientBasisCandidateIndex = GetSingleImageBasisCandidateIndex();
	SolverSettings.bOrientSwapFrontRight = !Settings.IsValid() || Settings->bSingleImageSwapFrontRight;

	OutRotation.BranchRotations.Reset();
	OutRotation.BranchAzimuthOffsets.Reset();
	OutRotation.BranchTargetOrientPoses.Reset();
	OutRotation.bHasRefOrientPose = bHasRefPose;
	OutRotation.RefOrientPose = RefOrientPose;
	OutRotation.bHasThumbnailCamera = bHasThumbnailCamera;
	OutRotation.ThumbnailCameraRotation = ThumbnailCameraRotation;
	OutRotation.bHasTargetOrientPose = false;

	// Dual Image: rebuild both ref/target through the validated single-image basis,
	// then enumerate symmetry branches so the user can pick the one matching the concept art.
	const TArray<TSharedPtr<FJsonValue>>* Branches = nullptr;
	if (Response.IsValid() && Response->TryGetArrayField(TEXT("branches"), Branches) && Branches != nullptr)
	{
		for (const TSharedPtr<FJsonValue>& BranchValue : *Branches)
		{
			const TSharedPtr<FJsonObject> Branch = BranchValue.IsValid() ? BranchValue->AsObject() : nullptr;
			if (!Branch.IsValid())
			{
				continue;
			}
			const TSharedPtr<FJsonObject> BranchPose = OrientValidationGetObjectField(Branch, TEXT("target_orientation"));
			if (!BranchPose.IsValid())
			{
				continue;
			}

			FVector TargetOrientPose = FVector::ZeroVector;
			if (!OrientValidationPoseFromObject(BranchPose, TargetOrientPose))
			{
				continue;
			}

			FAssetCandidate Candidate;
			Candidate.AssetPath = GetTargetMeshAssetPath();
			Candidate.BboxCenter = FVector::ZeroVector;
			Candidate.BboxHalfExtents = FVector::OneVector;
			Candidate.SemanticScore = 1.0f;
			Candidate.bHasOrientation = true;
			Candidate.bHasRefPose = bHasRefPose;
			Candidate.RefOrientationPose = RefOrientPose;
			Candidate.bHasTargetPose = true;
			Candidate.TargetOrientationPose = TargetOrientPose;
			Candidate.bHasThumbnailCamera = bHasThumbnailCamera;
			Candidate.ThumbnailCameraRotation = ThumbnailCameraRotation;

			const FRotator BranchWorld = USceneAssemblySolverLibrary::ResolveImageOrientationWorldRotation(Candidate, SolverSettings);
			OutRotation.BranchRotations.Add(BranchWorld);
			OutRotation.BranchAzimuthOffsets.Add(OrientValidationGetIntField(Branch, TEXT("azimuth_offset"), 0));
			OutRotation.BranchTargetOrientPoses.Add(TargetOrientPose);
		}
	}

	if (OutRotation.BranchRotations.Num() > 0)
	{
		// Primary world rotation = branch with azimuth offset 0 (or first branch).
		int32 PrimaryIndex = OutRotation.BranchAzimuthOffsets.IndexOfByKey(0);
		if (PrimaryIndex == INDEX_NONE)
		{
			PrimaryIndex = 0;
		}
		if (!OutRotation.BranchRotations.IsValidIndex(PrimaryIndex))
		{
			return false;
		}
		DualImageDirectionIndex = PrimaryIndex;
		OutRotation.WorldRotation = OutRotation.BranchRotations[PrimaryIndex];
		OutRotation.TargetOrientPose = OutRotation.BranchTargetOrientPoses.IsValidIndex(PrimaryIndex) ? OutRotation.BranchTargetOrientPoses[PrimaryIndex] : FVector::ZeroVector;
		OutRotation.bHasTargetOrientPose = true;
		FString BranchSummary;
		for (int32 i = 0; i < OutRotation.BranchRotations.Num(); ++i)
		{
			const FRotator& R = OutRotation.BranchRotations[i];
			BranchSummary += FString::Printf(
				TEXT("\n  [az+%d] Pitch %.3f, Yaw %.3f, Roll %.3f"),
				OutRotation.BranchAzimuthOffsets[i], R.Pitch, R.Yaw, R.Roll);
		}
		OutRotation.MetadataText = FString::Printf(
			TEXT("资产：%s\n场景相机：Pitch %.3f, Yaw %.3f, Roll %.3f\n缩略图相机：Pitch %.3f, Yaw %.3f, Roll %.3f%s\nRef Pose：azimuth %.3f, polar %.3f, rotation %.3f\nTarget Pose：azimuth %.3f, polar %.3f, rotation %.3f\n对称分支（共 %d 个，请对照原画挑选）：%s"),
			*GetTargetMeshAssetPath(),
			CaptureCameraRotation.Pitch,
			CaptureCameraRotation.Yaw,
			CaptureCameraRotation.Roll,
			ThumbnailCameraRotation.Pitch,
			ThumbnailCameraRotation.Yaw,
			ThumbnailCameraRotation.Roll,
			bHasThumbnailCamera ? TEXT("") : TEXT("（默认回退）"),
			RefOrientPose.X,
			RefOrientPose.Y,
			RefOrientPose.Z,
			OutRotation.TargetOrientPose.X,
			OutRotation.TargetOrientPose.Y,
			OutRotation.TargetOrientPose.Z,
			OutRotation.BranchRotations.Num(),
			*BranchSummary);
		OutRotation.bValid = true;
		return true;
	}
	return false;
}

void SOrientValidationPanel::RecomputeDualImageResultForCurrentSettings(bool bRedrawAxes)
{
	if (!DualImageResult.bValid || !DualImageResult.bHasRefOrientPose || DualImageResult.BranchTargetOrientPoses.IsEmpty())
	{
		return;
	}

	FSolverSettings SolverSettings;
	SolverSettings.OrientMode = ESceneAssemblyOrientMode::DualImage;
	SolverSettings.ConceptCameraRotation = CaptureCameraRotation;
	SolverSettings.OrientBasisCandidateIndex = GetSingleImageBasisCandidateIndex();
	SolverSettings.bOrientSwapFrontRight = !Settings.IsValid() || Settings->bSingleImageSwapFrontRight;

	DualImageResult.BranchRotations.Reset();
	for (const FVector& TargetPose : DualImageResult.BranchTargetOrientPoses)
	{
		FAssetCandidate Candidate;
		Candidate.AssetPath = GetTargetMeshAssetPath();
		Candidate.BboxCenter = FVector::ZeroVector;
		Candidate.BboxHalfExtents = FVector::OneVector;
		Candidate.SemanticScore = 1.0f;
		Candidate.bHasOrientation = true;
		Candidate.bHasRefPose = true;
		Candidate.RefOrientationPose = DualImageResult.RefOrientPose;
		Candidate.bHasTargetPose = true;
		Candidate.TargetOrientationPose = TargetPose;
		Candidate.bHasThumbnailCamera = DualImageResult.bHasThumbnailCamera;
		Candidate.ThumbnailCameraRotation = DualImageResult.ThumbnailCameraRotation;
		DualImageResult.BranchRotations.Add(USceneAssemblySolverLibrary::ResolveImageOrientationWorldRotation(Candidate, SolverSettings));
	}

	SyncDualImageSelectedBranch();
	if (bRedrawAxes)
	{
		DrawRotationResultAxes(DualImageResult, TEXT("Dual Image"), true);
	}

	FString BranchSummary;
	for (int32 i = 0; i < DualImageResult.BranchRotations.Num(); ++i)
	{
		const FRotator& R = DualImageResult.BranchRotations[i];
		BranchSummary += FString::Printf(
			TEXT("\n  [az+%d] Pitch %.3f, Yaw %.3f, Roll %.3f"),
			DualImageResult.BranchAzimuthOffsets.IsValidIndex(i) ? DualImageResult.BranchAzimuthOffsets[i] : 0,
			R.Pitch,
			R.Yaw,
			R.Roll);
	}
	DualImageResult.MetadataText = FString::Printf(
		TEXT("已使用当前 M_basis 重算 Dual Image。\nRef Pose：azimuth %.3f, polar %.3f, rotation %.3f\nTarget Pose：azimuth %.3f, polar %.3f, rotation %.3f\n对称分支：%s"),
		DualImageResult.RefOrientPose.X,
		DualImageResult.RefOrientPose.Y,
		DualImageResult.RefOrientPose.Z,
		DualImageResult.TargetOrientPose.X,
		DualImageResult.TargetOrientPose.Y,
		DualImageResult.TargetOrientPose.Z,
		*BranchSummary);
}

FString SOrientValidationPanel::BuildSingleImageStatusText() const
{
	if (!bHasSingleImageResult)
	{
		return TEXT("尚未计算。");
	}

	FString Status = FString::Printf(
		TEXT("已计算。\nDirection [%d/%d]（azimuth 偏移 +%.0f°）\nWorld Rotation：%s"),
		SingleImageDirectionIndex + 1,
		FMath::Max(1, GetSingleImageDirectionCount()),
		GetSingleImageDirectionAzimuthOffset(),
		*OrientValidationRotatorToText(SingleImageWorldRotation));
	if (!SingleImageAxesText.IsEmpty())
	{
		Status += TEXT("\n三轴：\n") + SingleImageAxesText;
	}
	if (SingleImageNumDirections != 1)
	{
		Status += FString::Printf(TEXT("\n注意：num_directions=%d，模型认为该目标可能存在多解或不明确朝向。"), SingleImageNumDirections);
	}
	return Status;
}

FString SOrientValidationPanel::BuildDualImageStatusText() const
{
	if (!DualImageResult.bValid)
	{
		return TEXT("尚未计算。");
	}

	FString Status = FString::Printf(
		TEXT("已计算。\nDirection [%d/%d]（azimuth 偏移 +%d°）\nWorld Rotation：%s"),
		FMath::Clamp(DualImageDirectionIndex, 0, GetDualImageDirectionCount() - 1) + 1,
		GetDualImageDirectionCount(),
		GetDualImageAzimuthOffset(),
		*OrientValidationRotatorToText(DualImageResult.WorldRotation));
	if (!DualImageAxesText.IsEmpty())
	{
		Status += TEXT("\n三轴：\n") + DualImageAxesText;
	}
	return Status;
}

FString SOrientValidationPanel::BuildTimingReport(const TSharedPtr<FJsonObject>& Response, const FText& ModeLabel, double RoundTripMs) const
{
	const TSharedPtr<FJsonObject> Timings = OrientValidationGetObjectField(Response, TEXT("timings"));
	FString Output = FString::Printf(TEXT("%s 计算耗时："), *ModeLabel.ToString());
	double OrientPredictMs = 0.0;
	double ServiceLatencyMs = 0.0;
	const bool bHasOrientPredictMs = OrientValidationTryGetTimingMs(Timings, TEXT("orient_predict_ms"), OrientPredictMs);
	const bool bHasServiceLatencyMs = OrientValidationTryGetTimingMs(Timings, TEXT("service_latency_ms"), ServiceLatencyMs);
	OrientValidationAppendTimingLine(Output, TEXT("读取场景图"), Timings, TEXT("read_scene_ms"));
	OrientValidationAppendTimingLine(Output, TEXT("查询元数据"), Timings, TEXT("query_metadata_ms"));
	OrientValidationAppendTimingLine(Output, TEXT("下载缩略图"), Timings, TEXT("download_thumbnail_ms"));
	OrientValidationAppendTimingLine(Output, TEXT("Orient 推理(含去背)"), Timings, TEXT("orient_predict_ms"));
	if (bHasOrientPredictMs && bHasServiceLatencyMs)
	{
		Output += FString::Printf(TEXT("\n  传输/排队/解码: %.1f ms"), FMath::Max(0.0, OrientPredictMs - ServiceLatencyMs));
	}
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

	Settings->SingleImageWorldRotation = SingleImageWorldRotation;
	Settings->SingleImageOrientPose = bHasSingleImageResult
		? FString::Printf(TEXT("%s, num_directions %d"), *OrientValidationPoseToText(SingleImageOrientPose), SingleImageNumDirections)
		: TEXT("");
	Settings->SingleImageStatus = BuildSingleImageStatusText();

	SyncDualImageSelectedBranch();
	Settings->DualImageWorldRotation = DualImageResult.WorldRotation;
	Settings->DualImageRefPose = DualImageResult.bHasRefOrientPose ? OrientValidationPoseToText(DualImageResult.RefOrientPose) : TEXT("");
	Settings->DualImageTargetPose = DualImageResult.bHasTargetOrientPose ? OrientValidationPoseToText(DualImageResult.TargetOrientPose) : TEXT("");
	Settings->DualImageStatus = BuildDualImageStatusText();
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

	int32 CaptureWidth = 0;
	int32 CaptureHeight = 0;
	OrientValidationCaptureResolution(CaptureWidth, CaptureHeight);

	if (!USceneCaptureLibrary::CaptureSceneFromActiveViewport(CaptureOutputDir, CaptureBaseName, CaptureWidth, CaptureHeight))
	{
		LastResult = TEXT("场景截图失败。请确认当前有活动透视视口。");
		AppendLog(LastResult);
		return FReply::Handled();
	}

	LoadCaptureMetadataFromJson();
	RefreshSceneBrush();
	DualImageResult = FComputedRotation();
	ActiveResult = EActiveOrientResult::None;
	DualImageDirectionIndex = 0;
	DualImageAxesText.Empty();
	bHasSingleImageResult = false;
	SingleImageOrientPose = FVector::ZeroVector;
	SingleImageNumDirections = 1;
	SingleImageDirectionIndex = 0;
	SingleImageWorldRotation = FRotator::ZeroRotator;
	SingleImageAxesText.Empty();
	UpdateSettingsResults();
	LastResult = CaptureWidth > 0 && CaptureHeight > 0
		? FString::Printf(TEXT("已截取当前场景（长边限制 %d，输出 %dx%d）。"), OrientValidationMaxCaptureEdge, CaptureWidth, CaptureHeight)
		: TEXT("已截取当前场景。 ");
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
	DualImageAxesText.Empty();
	if (ComputeRotation(TEXT("compute_dual_image_rotation_json"), LOCTEXT("DualImageLabel", "Dual Image"), DualImageResult))
	{
		ActiveResult = EActiveOrientResult::DualImage;
		DrawActiveAxes(true);
		UpdateSettingsResults();
	}
	return FReply::Handled();
}

FReply SOrientValidationPanel::OnComputeSingleImageClicked()
{
	if (!CanComputeSingleImage())
	{
		LastResult = TEXT("请先截取场景。单图朝向调试不需要设置当前摆放资产。 ");
		AppendLog(LastResult);
		return FReply::Handled();
	}

	const FString PayloadJson = BuildSingleImagePayloadJson();
	FTCHARToUTF8 PayloadUtf8(*PayloadJson);
	const FString PayloadBase64 = FBase64::Encode(reinterpret_cast<const uint8*>(PayloadUtf8.Get()), PayloadUtf8.Length());

	TSharedPtr<FJsonObject> Response;
	const double StartedSeconds = FPlatformTime::Seconds();
	if (!CallController(FString::Printf(TEXT("compute_single_image_orientation_json('%s')"), *PayloadBase64), Response) || !OrientValidationGetBoolField(Response, TEXT("ok"), false))
	{
		LastResult = TEXT("单图朝向计算失败。 ");
		return FReply::Handled();
	}
	const double RoundTripMs = (FPlatformTime::Seconds() - StartedSeconds) * 1000.0;

	const TSharedPtr<FJsonObject> OrientPose = OrientValidationGetObjectField(Response, TEXT("orient_pose"));
	if (!OrientPose.IsValid())
	{
		bHasSingleImageResult = false;
		if (ActiveResult == EActiveOrientResult::SingleImage)
		{
			ActiveResult = EActiveOrientResult::None;
		}
		LastResult = TEXT("单图朝向响应缺少 orient_pose。 ");
		AppendLog(LastResult);
		return FReply::Handled();
	}

	SingleImageOrientPose = FVector(
		OrientValidationGetNumberField(OrientPose, TEXT("azimuth"), 0.0),
		OrientValidationGetNumberField(OrientPose, TEXT("polar"), 0.0),
		OrientValidationGetNumberField(OrientPose, TEXT("rotation"), 0.0));
	SingleImageNumDirections = OrientValidationGetIntField(OrientPose, TEXT("num_directions"), 1);
	SingleImageDirectionIndex = 0;
	SingleImageWorldRotation = FRotator::ZeroRotator;
	SingleImageAxesText.Empty();
	bHasSingleImageResult = true;
	ActiveResult = EActiveOrientResult::SingleImage;

	DrawActiveAxes(true);
	UpdateSettingsResults();

	LastResult = TEXT("单图朝向计算完成，已绘制世界三轴。 ");
	FString SymmetryHint;
	if (SingleImageNumDirections != 1)
	{
		SymmetryHint = FString::Printf(TEXT("\n对称提示：num_directions=%d，模型认为该目标可能存在多解或不明确朝向。"), SingleImageNumDirections);
	}
	AppendLog(FString::Printf(TEXT("%s\n%s%s\n%s"), *LastResult, *GetSingleImageInfoText().ToString(), *SymmetryHint, *BuildTimingReport(Response, LOCTEXT("SingleImageLabel", "Single Image"), RoundTripMs)));
	return FReply::Handled();
}

FReply SOrientValidationPanel::OnDrawSingleImageAxesClicked()
{
	if (!CanDrawActiveAxes())
	{
		LastResult = TEXT("尚无可绘制的朝向结果。");
		AppendLog(LastResult);
		return FReply::Handled();
	}

	DrawActiveAxes(true);
	UpdateSettingsResults();
	LastResult = ActiveResult == EActiveOrientResult::DualImage
		? TEXT("已绘制 Dual Image 世界三轴。 ")
		: TEXT("已绘制单图朝向世界三轴。 ");
	AppendLog(FString::Printf(TEXT("%s\n%s"), *LastResult, *GetSingleImageInfoText().ToString()));
	return FReply::Handled();
}

FReply SOrientValidationPanel::OnCycleSingleImageDirectionClicked()
{
	if (ActiveResult == EActiveOrientResult::SingleImage)
	{
		if (!bHasSingleImageResult)
		{
			LastResult = TEXT("尚未计算单图朝向，无法切换 Direction。 ");
			AppendLog(LastResult);
			return FReply::Handled();
		}

		const int32 DirectionCount = FMath::Max(1, GetSingleImageDirectionCount());
		SingleImageDirectionIndex = (SingleImageDirectionIndex + 1) % DirectionCount;
		DrawActiveAxes(true);
		UpdateSettingsResults();
		LastResult = FString::Printf(TEXT("已切换单图朝向 Direction [%d/%d]。"), SingleImageDirectionIndex + 1, DirectionCount);
		AppendLog(FString::Printf(TEXT("%s\n%s"), *LastResult, *GetSingleImageInfoText().ToString()));
		return FReply::Handled();
	}

	if (ActiveResult == EActiveOrientResult::DualImage && DualImageResult.bValid)
	{
		const int32 DirectionCount = GetDualImageDirectionCount();
		DualImageDirectionIndex = (DualImageDirectionIndex + 1) % DirectionCount;
		DrawActiveAxes(true);
		UpdateSettingsResults();
		LastResult = FString::Printf(TEXT("已切换 Dual Image Direction [%d/%d]（azimuth 偏移 +%d°）。"), DualImageDirectionIndex + 1, DirectionCount, GetDualImageAzimuthOffset());
		AppendLog(FString::Printf(TEXT("%s\n%s"), *LastResult, *GetSingleImageInfoText().ToString()));
		return FReply::Handled();
	}

	LastResult = TEXT("尚无可切换的朝向结果。");
	AppendLog(LastResult);
	return FReply::Handled();
}

FReply SOrientValidationPanel::OnClearSingleImageAxesClicked()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (World)
	{
		FlushPersistentDebugLines(World);
	}
	LastResult = TEXT("已清理持久调试线。 ");
	AppendLog(LastResult);
	return FReply::Handled();
}

FReply SOrientValidationPanel::OnSpawnSingleImageClicked()
{
	FComputedRotation SingleImageResult;
	SingleImageResult.bValid = bHasSingleImageResult;
	SingleImageResult.WorldRotation = ComputeSingleImageWorldRotationForCurrentDirection();

	const FScopedTransaction Transaction(LOCTEXT("SpawnSingleImageTransaction", "Scene Assembly: Spawn Single Image Orient Validation"));
	AActor* Actor = SpawnResultActor(SingleImageResult, TEXT("SingleImage"), FVector::ZeroVector);
	LastResult = Actor ? TEXT("已 Spawn 单图结果到场景。") : TEXT("Spawn 单图结果失败。 ");
	AppendLog(LastResult);
	return FReply::Handled();
}

FReply SOrientValidationPanel::OnSpawnDualImageClicked()
{
	SyncDualImageSelectedBranch();
	const FScopedTransaction Transaction(LOCTEXT("SpawnDualImageTransaction", "Scene Assembly: Spawn Dual Image Orient Validation"));
	AActor* Actor = SpawnResultActor(DualImageResult, TEXT("DualImage"), FVector::ZeroVector);
	LastResult = Actor ? TEXT("已 Spawn Dual Image 结果到场景。") : TEXT("Spawn Dual Image 结果失败。");
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

FText SOrientValidationPanel::GetSingleImageInfoText() const
{
	const bool bComputeRotation = !Settings.IsValid() || Settings->bSingleImageComputeRotation;
	FString Text = FString::Printf(
		TEXT("计算模式：%s\n当前 M_basis：%s\n绘制：世界原点 (0,0,0)，轴长 %.0f，Front=红 / Right=绿 / Up=蓝，白点=原点"),
		bComputeRotation ? TEXT("Rotation / 世界 FRotator") : TEXT("逐轴向量 / 对照模式"),
		*GetSingleImageBasisSummary(),
		OrientValidationSingleImageAxisLength);
	if (!bHasSingleImageResult)
	{
		Text += TEXT("\n尚未计算单图朝向。先截取场景，再点击“计算单图朝向”。");
		return FText::FromString(Text);
	}

	Text += FString::Printf(
		TEXT("\nOrient Pose：azimuth %.3f, polar %.3f, rotation %.3f, num_directions %d"),
		SingleImageOrientPose.X,
		SingleImageOrientPose.Y,
		SingleImageOrientPose.Z,
		SingleImageNumDirections);
	Text += FString::Printf(
		TEXT("\nDirection：[%d/%d]（azimuth 偏移 +%.0f°）"),
		SingleImageDirectionIndex + 1,
		FMath::Max(1, GetSingleImageDirectionCount()),
		GetSingleImageDirectionAzimuthOffset());
	if (bComputeRotation)
	{
		Text += FString::Printf(
			TEXT("\nWorld Rotation：Pitch %+.3f, Yaw %+.3f, Roll %+.3f"),
			SingleImageWorldRotation.Pitch,
			SingleImageWorldRotation.Yaw,
			SingleImageWorldRotation.Roll);
	}
	else if (!SingleImageAxesText.IsEmpty())
	{
		Text += TEXT("\n逐轴向量三轴：\n") + SingleImageAxesText;
	}
	else
	{
		Text += TEXT("\n逐轴向量三轴：尚未绘制。");
	}
	if (SingleImageNumDirections != 1)
	{
		Text += TEXT("\n注意：num_directions 非 1，模型输出可能存在对称多解或无明确朝向。");
	}
	if (DualImageResult.bValid)
	{
		Text += FString::Printf(
			TEXT("\n\nDual Image Direction：[%d/%d]（azimuth 偏移 +%d°）\nRef Pose：%s\nTarget Pose：%s"),
			FMath::Clamp(DualImageDirectionIndex, 0, GetDualImageDirectionCount() - 1) + 1,
			GetDualImageDirectionCount(),
			GetDualImageAzimuthOffset(),
			*OrientValidationPoseToText(DualImageResult.RefOrientPose),
			*OrientValidationPoseToText(DualImageResult.TargetOrientPose));
		if (!DualImageAxesText.IsEmpty())
		{
			Text += TEXT("\nDual Image 三轴：\n") + DualImageAxesText;
		}
	}
	return FText::FromString(Text);
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

bool SOrientValidationPanel::CanComputeSingleImage() const
{
	return !CapturedSceneImagePath.IsEmpty() && bHasCaptureCamera;
}

bool SOrientValidationPanel::CanDrawSingleImageAxes() const
{
	return CanDrawActiveAxes();
}

bool SOrientValidationPanel::CanDrawActiveAxes() const
{
	if (!bHasCaptureCamera)
	{
		return false;
	}
	if (ActiveResult == EActiveOrientResult::SingleImage)
	{
		return bHasSingleImageResult;
	}
	if (ActiveResult == EActiveOrientResult::DualImage)
	{
		return DualImageResult.bValid;
	}
	return false;
}

bool SOrientValidationPanel::CanSpawnSingleImage() const
{
	return bHasSingleImageResult && Settings.IsValid() && Settings->TargetMesh != nullptr;
}

bool SOrientValidationPanel::CanSpawnDualImage() const
{
	return DualImageResult.bValid && Settings.IsValid() && Settings->TargetMesh != nullptr;
}

#undef LOCTEXT_NAMESPACE
