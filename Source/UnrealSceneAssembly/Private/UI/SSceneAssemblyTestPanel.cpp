#include "UI/SSceneAssemblyTestPanel.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "DesktopPlatformModule.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Actor.h"
#include "GenericPlatform/GenericWindow.h"
#include "HAL/PlatformProcess.h"
#include "IDetailsView.h"
#include "IDesktopPlatform.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "LevelEditorViewport.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "SceneCaptureLibrary.h"
#include "SceneView.h"
#include "ConvexVolume.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "UnrealSceneAssembly.h"
#include "EngineUtils.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SSceneAssemblyTestPanel"

namespace
{
static const FLinearColor TestMutedColor(0.58f, 0.58f, 0.58f, 1.0f);

FString TestGetStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, const FString& DefaultValue = FString())
{
	FString Value;
	return Object.IsValid() && Object->TryGetStringField(FieldName, Value) ? Value : DefaultValue;
}

bool TestGetBoolField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, bool DefaultValue = false)
{
	bool Value = DefaultValue;
	return Object.IsValid() && Object->TryGetBoolField(FieldName, Value) ? Value : DefaultValue;
}

int32 TestGetIntField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, int32 DefaultValue = 0)
{
	double Value = 0.0;
	return Object.IsValid() && Object->TryGetNumberField(FieldName, Value) ? static_cast<int32>(Value) : DefaultValue;
}

double TestGetNumberField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, double DefaultValue = 0.0)
{
	double Value = DefaultValue;
	return Object.IsValid() && Object->TryGetNumberField(FieldName, Value) ? Value : DefaultValue;
}

TSharedPtr<FJsonObject> TestGetObjectField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	const TSharedPtr<FJsonObject>* Child = nullptr;
	return Object.IsValid() && Object->TryGetObjectField(FieldName, Child) && Child != nullptr ? *Child : nullptr;
}

TSharedRef<SWidget> TestMakeCard(const TSharedRef<SWidget>& Content)
{
	return SNew(SBorder)
		.Padding(16.0f)
		.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
		[
			Content
		];
}

TSharedRef<SWidget> TestMakeActionButton(const FText& Text, const FOnClicked& OnClicked)
{
	return SNew(SButton)
		.Text(Text)
		.OnClicked(OnClicked);
}

FString TestScaleModeToString(ESceneAssemblyScaleMode ScaleMode)
{
	return ScaleMode == ESceneAssemblyScaleMode::MatchHeight ? TEXT("MatchHeight") : TEXT("FitIoU");
}

FString TestCombineModeToString(ESceneAssemblyScoreCombineMode CombineMode)
{
	return CombineMode == ESceneAssemblyScoreCombineMode::Additive ? TEXT("Additive") : TEXT("Multiplicative");
}

FString TestRetrievalModelToString(ESceneAssemblyRetrievalModel Model)
{
	return Model == ESceneAssemblyRetrievalModel::DINOv3 ? TEXT("DINOv3") : TEXT("CLIP");
}

FString TestCropBboxSourceToString(ESceneAssemblyCropBboxSource Source)
{
	return Source == ESceneAssemblyCropBboxSource::VisiblePixels ? TEXT("pixel_bbox") : TEXT("full_bbox");
}

FString TestCropBboxSourceDisplayName(ESceneAssemblyCropBboxSource Source)
{
	return Source == ESceneAssemblyCropBboxSource::VisiblePixels ? TEXT("pixel_bbox") : TEXT("full_bbox");
}

bool TestReadBboxField(const TSharedPtr<FJsonObject>& EntryObject, const TCHAR* FieldName, FIntRect& OutBounds)
{
	const TArray<TSharedPtr<FJsonValue>>* BboxValues = nullptr;
	if (!EntryObject.IsValid() || !EntryObject->TryGetArrayField(FieldName, BboxValues) || BboxValues == nullptr || BboxValues->Num() != 4)
	{
		return false;
	}

	OutBounds.Min.X = static_cast<int32>((*BboxValues)[0]->AsNumber());
	OutBounds.Min.Y = static_cast<int32>((*BboxValues)[1]->AsNumber());
	OutBounds.Max.X = static_cast<int32>((*BboxValues)[2]->AsNumber());
	OutBounds.Max.Y = static_cast<int32>((*BboxValues)[3]->AsNumber());
	return OutBounds.Min.X <= OutBounds.Max.X && OutBounds.Min.Y <= OutBounds.Max.Y;
}

FString TestTimestampBaseName()
{
	return FString::Printf(TEXT("aesthetic_ref_%s"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
}

bool TestLoadImageAsBgra(const FString& ImagePath, TArray<uint8>& OutRawData, int32& OutWidth, int32& OutHeight)
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

TSharedPtr<FSlateDynamicImageBrush> TestLoadBrushFromImage(const FString& ImagePath, const FString& ResourceSuffix, const int32 TargetWidth = 0, const int32 TargetHeight = 0)
{
	TArray<uint8> RawData;
	int32 Width = 0;
	int32 Height = 0;
	if (!TestLoadImageAsBgra(ImagePath, RawData, Width, Height))
	{
		return nullptr;
	}

	if (TargetWidth > 0 && TargetHeight > 0 && (Width != TargetWidth || Height != TargetHeight))
	{
		TArray<FColor> SourcePixels;
		SourcePixels.SetNumUninitialized(Width * Height);
		FMemory::Memcpy(SourcePixels.GetData(), RawData.GetData(), RawData.Num());

		TArray<FColor> ResizedPixels;
		ResizedPixels.SetNumUninitialized(TargetWidth * TargetHeight);
		FImageUtils::ImageResize(Width, Height, SourcePixels, TargetWidth, TargetHeight, ResizedPixels, false);
		for (FColor& Pixel : ResizedPixels)
		{
			Pixel.A = 255;
		}

		RawData.SetNumUninitialized(TargetWidth * TargetHeight * 4);
		FMemory::Memcpy(RawData.GetData(), ResizedPixels.GetData(), RawData.Num());
		Width = TargetWidth;
		Height = TargetHeight;
	}

	const FString ResourceName = FString::Printf(TEXT("SceneAssembly_%s_%s"), *ResourceSuffix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	return FSlateDynamicImageBrush::CreateWithImageData(FName(*ResourceName), FVector2D(static_cast<float>(Width), static_cast<float>(Height)), RawData);
}

bool TestJsonVector(const TSharedPtr<FJsonObject>& Object, FVector& OutValue)
{
	if (!Object.IsValid())
	{
		return false;
	}
	OutValue.X = TestGetNumberField(Object, TEXT("x"), 0.0);
	OutValue.Y = TestGetNumberField(Object, TEXT("y"), 0.0);
	OutValue.Z = TestGetNumberField(Object, TEXT("z"), 0.0);
	return true;
}

bool TestJsonRotator(const TSharedPtr<FJsonObject>& Object, FRotator& OutValue)
{
	if (!Object.IsValid())
	{
		return false;
	}
	OutValue.Pitch = TestGetNumberField(Object, TEXT("pitch"), 0.0);
	OutValue.Yaw = TestGetNumberField(Object, TEXT("yaw"), 0.0);
	OutValue.Roll = TestGetNumberField(Object, TEXT("roll"), 0.0);
	return true;
}

FString TestSanitizeForFileName(FString Value)
{
	if (Value.IsEmpty())
	{
		return TEXT("Actor");
	}
	for (TCHAR& Ch : Value)
	{
		if (!FChar::IsAlnum(Ch))
		{
			Ch = TEXT('_');
		}
	}
	return Value.Left(80);
}
}

void SSceneAssemblyTestPanel::Construct(const FArguments& InArgs)
{
	Settings.Reset(NewObject<USceneAssemblyTestSettings>(GetTransientPackage(), USceneAssemblyTestSettings::StaticClass()));

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	SettingsDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	SettingsDetailsView->SetObject(Settings.Get());

	JobProgressBarStyle
		.SetBackgroundImage(FSlateColorBrush(FStyleColors::Recessed))
		.SetFillImage(FSlateColorBrush(FStyleColors::Primary))
		.SetMarqueeImage(FSlateColorBrush(FStyleColors::Primary))
		.SetEnableFillAnimation(false);

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
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Title", "场景装配测试"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 22))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Subtitle", "截取白盒场景和 ID Map，上传原画后按每个白盒区域进行 CLIP 图搜图，批量求解摆放并生成结果。"))
						.ColorAndOpacity(FSlateColor(TestMutedColor))
						.AutoWrapText(true)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 18.0f, 0.0f, 12.0f)
				[
					TestMakeCard(
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("SpawnHeader", "摆放 / 清理")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
							[
								SNew(SButton)
								.Text(LOCTEXT("SolvePlace", "求解并摆放"))
								.IsEnabled(this, &SSceneAssemblyTestPanel::CanRun)
								.OnClicked(this, &SSceneAssemblyTestPanel::OnSolvePlaceClicked)
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
							[
								SNew(SButton)
								.Text(LOCTEXT("CleanupOnly", "仅清理"))
								.IsEnabled(this, &SSceneAssemblyTestPanel::CanCleanup)
								.OnClicked(this, &SSceneAssemblyTestPanel::OnCleanupClicked)
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
							[
								TestMakeActionButton(LOCTEXT("SelectAllWhiteboxes", "全选白盒"), FOnClicked::CreateSP(this, &SSceneAssemblyTestPanel::OnSelectAllWhiteboxesClicked))
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
							[
								TestMakeActionButton(LOCTEXT("SelectVisibleWhiteboxes", "选择可视范围白盒"), FOnClicked::CreateSP(this, &SSceneAssemblyTestPanel::OnSelectVisibleWhiteboxesClicked))
							]
							+ SHorizontalBox::Slot().AutoWidth()
							[
								TestMakeActionButton(LOCTEXT("DeselectAll", "取消选择"), FOnClicked::CreateSP(this, &SSceneAssemblyTestPanel::OnDeselectClicked))
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
							[
								SNew(SButton)
								.Text(LOCTEXT("CancelAssembly", "取消任务"))
								.IsEnabled(this, &SSceneAssemblyTestPanel::CanCancelJob)
								.OnClicked(this, &SSceneAssemblyTestPanel::OnCancelJobClicked)
							]
							+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
							[
								SNew(SBox)
								.HeightOverride(8.0f)
								.Visibility(this, &SSceneAssemblyTestPanel::GetJobProgressVisibility)
								[
									SNew(SProgressBar)
									.Style(&JobProgressBarStyle)
									.Percent(this, &SSceneAssemblyTestPanel::GetJobProgress)
								]
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock)
							.Visibility(this, &SSceneAssemblyTestPanel::GetJobProgressVisibility)
							.Text(this, &SSceneAssemblyTestPanel::GetJobProgressText)
							.ColorAndOpacity(FSlateColor(TestMutedColor))
							.AutoWrapText(true)
						]
					)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 12.0f)
				[
					TestMakeCard(
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("AestheticHeader", "美学参考")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("AestheticDesc", "截取当前选中的 Blockout 白盒作为 ID map，上传外部生成的原画后用局部裁剪进行 CLIP 图搜图。"))
							.ColorAndOpacity(FSlateColor(TestMutedColor))
							.AutoWrapText(true)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
							[
								TestMakeActionButton(LOCTEXT("CaptureAesthetic", "截取当前白盒场景"), FOnClicked::CreateSP(this, &SSceneAssemblyTestPanel::OnCaptureAestheticReferenceClicked))
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
							[
								TestMakeActionButton(LOCTEXT("CaptureSelectedAesthetic", "截取选中白盒场景"), FOnClicked::CreateSP(this, &SSceneAssemblyTestPanel::OnCaptureSelectedAestheticReferenceClicked))
							]
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SNew(SButton)
								.Text(LOCTEXT("JumpCaptureCamera", "跳转相机视角"))
								.IsEnabled(this, &SSceneAssemblyTestPanel::HasCaptureCamera)
								.OnClicked(this, &SSceneAssemblyTestPanel::OnJumpToCaptureCameraClicked)
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(this, &SSceneAssemblyTestPanel::GetCaptureInfoText)
							.AutoWrapText(true)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(this, &SSceneAssemblyTestPanel::GetConceptArtInfoText)
							.AutoWrapText(true)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 8.0f, 0.0f)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
									[
										SNew(STextBlock).Text(LOCTEXT("ScenePreviewLabel", "白盒截图"))
									]
									+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
									[
										SNew(SButton)
										.Text(LOCTEXT("OpenSceneCaptureFolder", "打开目录"))
										.IsEnabled(this, &SSceneAssemblyTestPanel::HasSceneCapturePath)
										.OnClicked(this, &SSceneAssemblyTestPanel::OnOpenSceneCaptureFolderClicked)
									]
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
								[
									SNew(SBox)
									.HeightOverride(180.0f)
									[
										SNew(SScaleBox)
										.Stretch(EStretch::ScaleToFit)
										.StretchDirection(EStretchDirection::Both)
										[
											SNew(SImage).Image(this, &SSceneAssemblyTestPanel::GetSceneCaptureBrush)
										]
									]
								]
							]
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(8.0f, 0.0f, 0.0f, 0.0f)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
									[
										SNew(STextBlock).Text(LOCTEXT("IdPreviewLabel", "ID Map"))
									]
									+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
									[
										SNew(SButton)
										.Text(LOCTEXT("OpenIdMapFolder", "打开目录"))
										.IsEnabled(this, &SSceneAssemblyTestPanel::HasIdMapPath)
										.OnClicked(this, &SSceneAssemblyTestPanel::OnOpenIdMapFolderClicked)
									]
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
								[
									SNew(SBox)
									.HeightOverride(180.0f)
									[
										SNew(SScaleBox)
										.Stretch(EStretch::ScaleToFit)
										.StretchDirection(EStretchDirection::Both)
										[
											SNew(SImage).Image(this, &SSceneAssemblyTestPanel::GetIdMapBrush)
										]
									]
								]
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
								[
									SNew(STextBlock).Text(LOCTEXT("ConceptPreviewLabel", "原画"))
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
								[
									SNew(SButton)
									.Text(LOCTEXT("OpenConceptArtFolder", "打开目录"))
									.IsEnabled(this, &SSceneAssemblyTestPanel::HasConceptArtPath)
									.OnClicked(this, &SSceneAssemblyTestPanel::OnOpenConceptArtFolderClicked)
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
								[
									SNew(SButton)
									.Text(LOCTEXT("UploadConceptArt", "上传原画"))
									.IsEnabled(this, &SSceneAssemblyTestPanel::CanUploadConceptArt)
									.OnClicked(this, &SSceneAssemblyTestPanel::OnUploadConceptArtClicked)
								]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
							[
								SNew(SBox)
								.HeightOverride(220.0f)
								[
									SNew(SScaleBox)
									.Stretch(EStretch::ScaleToFit)
									.StretchDirection(EStretchDirection::Both)
									[
										SNew(SImage).Image(this, &SSceneAssemblyTestPanel::GetConceptArtBrush)
									]
								]
								]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
						[
							SNew(SExpandableArea)
							.InitiallyCollapsed(true)
							.HeaderContent()
							[
								SNew(STextBlock)
								.Text(this, &SSceneAssemblyTestPanel::GetCropPreviewSummaryText)
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
							]
							.BodyContent()
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
									[
										SNew(SButton)
										.Text(LOCTEXT("RefreshCropPreviews", "刷新预览"))
										.IsEnabled(this, &SSceneAssemblyTestPanel::CanRun)
										.OnClicked(this, &SSceneAssemblyTestPanel::OnRefreshCropPreviewsClicked)
									]
									+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
									[
										SNew(STextBlock)
									.Text(LOCTEXT("CropPreviewHint", "用于检查所选 bbox 与原画是否对齐；切换裁剪框来源或修改裁剪框扩展后点击刷新预览。"))
										.ColorAndOpacity(FSlateColor(TestMutedColor))
										.AutoWrapText(true)
									]
								]
								+ SVerticalBox::Slot().AutoHeight()
								[
									SAssignNew(CropPreviewContainer, SVerticalBox)
								]
							]
						]
					)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 12.0f)
				[
					TestMakeCard(
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("SettingsHeader", "参数")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
						[
							SettingsDetailsView.ToSharedRef()
						]
					)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SExpandableArea)
					.InitiallyCollapsed(false)
					.HeaderContent()[SNew(STextBlock).Text(this, &SSceneAssemblyTestPanel::GetLastResultText).Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))]
					.BodyContent()
					[
						SNew(SBox)
						.MinDesiredHeight(240.0f)
						[
							SNew(SMultiLineEditableTextBox)
							.IsReadOnly(true)
							.AutoWrapText(true)
							.Text(this, &SSceneAssemblyTestPanel::GetLogText)
						]
					]
				]
			]
		]
	];

	UpdateSelectionSummaryFromEditor();
	RefreshCropPreviewWidget();
	RegisterActiveTimer(0.5f, FWidgetActiveTimerDelegate::CreateSP(this, &SSceneAssemblyTestPanel::RefreshStatusTick));
}

bool SSceneAssemblyTestPanel::CallController(const FString& FunctionCall, TSharedPtr<FJsonObject>& OutObject)
{
	const FString Json = FUnrealSceneAssemblyModule::ExecutePythonControllerCommand(FunctionCall);
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, OutObject) || !OutObject.IsValid())
	{
		AppendLog(FString::Printf(TEXT("无效的控制器响应：%s"), *Json));
		return false;
	}
	if (!TestGetBoolField(OutObject, TEXT("ok"), false))
	{
		AppendLog(TestGetStringField(OutObject, TEXT("error"), TestGetStringField(OutObject, TEXT("message"), TEXT("控制器命令执行失败。"))));
	}
	return true;
}

FString SSceneAssemblyTestPanel::BuildPayloadJson() const
{
	const USceneAssemblyTestSettings* CurrentSettings = Settings.Get();
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("capture_json_path"), CapturedJsonPath);
	Root->SetStringField(TEXT("concept_art_path"), ConceptArtPath);
	Root->SetStringField(TEXT("retrieval_model"), CurrentSettings ? TestRetrievalModelToString(CurrentSettings->RetrievalModel) : TEXT("DINOv3"));
	Root->SetStringField(TEXT("crop_bbox_source"), CurrentSettings ? TestCropBboxSourceToString(CurrentSettings->CropBboxSource) : TEXT("full_bbox"));
	Root->SetNumberField(TEXT("crop_expand_pixels"), CurrentSettings ? FMath::Max(0, CurrentSettings->CropExpandPixels) : 20);
	Root->SetNumberField(TEXT("candidate_limit"), CurrentSettings ? CurrentSettings->CandidateLimit : 20);
	Root->SetNumberField(TEXT("score_threshold"), CurrentSettings ? CurrentSettings->ScoreThreshold : 0.0f);
	Root->SetStringField(TEXT("result_tag"), GetResultTag());
	Root->SetBoolField(TEXT("whitebox_only"), CurrentSettings ? CurrentSettings->bWhiteboxOnly : true);

	Root->SetStringField(TEXT("scale_mode"), CurrentSettings ? TestScaleModeToString(CurrentSettings->ScaleMode) : TEXT("FitIoU"));
	Root->SetStringField(TEXT("combine_mode"), CurrentSettings ? TestCombineModeToString(CurrentSettings->CombineMode) : TEXT("Multiplicative"));
	Root->SetNumberField(TEXT("weight_semantic"), CurrentSettings ? CurrentSettings->WeightSemantic : 1.0f);
	Root->SetNumberField(TEXT("weight_geometry"), CurrentSettings ? CurrentSettings->WeightGeometry : 1.0f);
	Root->SetNumberField(TEXT("scale_sensitivity"), CurrentSettings ? CurrentSettings->ScaleSensitivity : 0.5f);
	Root->SetNumberField(TEXT("aspect_sensitivity"), CurrentSettings ? CurrentSettings->AspectSensitivity : 1.0f);
	Root->SetBoolField(TEXT("normalize_semantic"), CurrentSettings ? CurrentSettings->bNormalizeSemantic : false);
	Root->SetNumberField(TEXT("top_k"), CurrentSettings ? CurrentSettings->TopK : 1);
	Root->SetNumberField(TEXT("final_score_threshold"), CurrentSettings ? CurrentSettings->FinalScoreThreshold : 0.0f);
	Root->SetNumberField(TEXT("random_seed"), CurrentSettings ? CurrentSettings->RandomSeed : 0);

	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Root, Writer);
	return Output;
}

void SSceneAssemblyTestPanel::ApplyRunResponse(const TSharedPtr<FJsonObject>& Response)
{
	const TSharedPtr<FJsonObject> Cleanup = TestGetObjectField(Response, TEXT("cleanup"));
	const int32 Deleted = Cleanup.IsValid() ? TestGetIntField(Cleanup, TEXT("deleted_count"), 0) : 0;
	const int32 ActorCount = TestGetIntField(Response, TEXT("actor_count"), 0);
	const int32 Succeeded = TestGetIntField(Response, TEXT("succeeded"), 0);
	const int32 SpawnedCount = TestGetIntField(Response, TEXT("spawned_count"), 0);

	LastResult = FString::Printf(
		TEXT("%s | 批量：%d 个 | 成功：%d | 摆放：%d | 已删除：%d"),
		TestGetBoolField(Response, TEXT("ok"), false) ? TEXT("成功") : TEXT("失败"),
		ActorCount,
		Succeeded,
		SpawnedCount,
		Deleted);

	FString Lines = LastResult;
	double SeedValue = 0.0;
	if (Response->TryGetNumberField(TEXT("random_seed"), SeedValue))
	{
		Lines += FString::Printf(TEXT("\n批量随机种子：%lld"), static_cast<int64>(SeedValue));
	}

	const TArray<TSharedPtr<FJsonValue>>* SkippedActors = nullptr;
	if (Response->TryGetArrayField(TEXT("skipped_non_whitebox"), SkippedActors) && SkippedActors->Num() > 0)
	{
		Lines += FString::Printf(TEXT("\n跳过非白盒：%d 个"), SkippedActors->Num());
		for (const TSharedPtr<FJsonValue>& Value : *SkippedActors)
		{
			const TSharedPtr<FJsonObject> Skipped = Value.IsValid() ? Value->AsObject() : nullptr;
			if (Skipped.IsValid())
			{
				Lines += FString::Printf(TEXT("\n  - %s"), *TestGetStringField(Skipped, TEXT("label"), TEXT("Actor")));
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	if (Response->TryGetArrayField(TEXT("items"), Items))
	{
		int32 ItemIndex = 0;
		for (const TSharedPtr<FJsonValue>& ItemValue : *Items)
		{
			const TSharedPtr<FJsonObject> Item = ItemValue.IsValid() ? ItemValue->AsObject() : nullptr;
			if (!Item.IsValid())
			{
				continue;
			}

			const TSharedPtr<FJsonObject> Actor = TestGetObjectField(Item, TEXT("actor"));
			const TSharedPtr<FJsonObject> Search = TestGetObjectField(Item, TEXT("search"));
			const int32 HitCount = Search.IsValid() ? TestGetIntField(Search, TEXT("hit_count"), 0) : 0;
			const int32 CandidateCount = Search.IsValid() ? TestGetIntField(Search, TEXT("candidate_count"), 0) : 0;
			const int32 ChosenIndex = TestGetIntField(Item, TEXT("chosen_index"), 0);
			Lines += FString::Printf(
				TEXT("\n\n%d. %s | 状态：%s | 图搜命中：%d | 候选：%d"),
				ItemIndex + 1,
				*TestGetStringField(Actor, TEXT("label"), TEXT("Actor")),
				*TestGetStringField(Item, TEXT("status"), TEXT("unknown")),
				HitCount,
				CandidateCount);

			const FString Error = TestGetStringField(Item, TEXT("error"));
			if (!Error.IsEmpty())
			{
				Lines += FString::Printf(TEXT("\n  原因：%s"), *Error);
			}

			double ItemSeed = 0.0;
			if (Item->TryGetNumberField(TEXT("random_seed"), ItemSeed))
			{
				Lines += FString::Printf(TEXT("\n  随机：第 %d 项（种子 %lld）"), ChosenIndex + 1, static_cast<int64>(ItemSeed));
			}

			const TArray<TSharedPtr<FJsonValue>>* Results = nullptr;
			if (Item->TryGetArrayField(TEXT("results"), Results))
			{
				int32 ResultIndex = 0;
				for (const TSharedPtr<FJsonValue>& ResultValue : *Results)
				{
					const TSharedPtr<FJsonObject> Result = ResultValue.IsValid() ? ResultValue->AsObject() : nullptr;
					if (!Result.IsValid())
					{
						continue;
					}
					Lines += FString::Printf(
						TEXT("\n  %s%d. %.4f | IoU %.4f | 缩放 %.4f | %s"),
						ResultIndex == ChosenIndex ? TEXT("» ") : TEXT("  "),
						ResultIndex + 1,
						TestGetNumberField(Result, TEXT("final_score"), 0.0),
						TestGetNumberField(Result, TEXT("fit_iou"), 0.0),
						TestGetNumberField(Result, TEXT("scale_factor"), 1.0),
						*TestGetStringField(Result, TEXT("asset_path")));
					++ResultIndex;
				}
			}
			++ItemIndex;
		}
	}
	AppendLog(Lines);
}

void SSceneAssemblyTestPanel::ApplyAsyncStatusResponse(const TSharedPtr<FJsonObject>& Response)
{
	if (!Response.IsValid())
	{
		return;
	}

	JobState = TestGetStringField(Response, TEXT("state"), JobState);
	JobTotal = TestGetIntField(Response, TEXT("total"), JobTotal);
	JobCompleted = TestGetIntField(Response, TEXT("completed"), JobCompleted);
	JobSpawned = TestGetIntField(Response, TEXT("spawned_count"), JobSpawned);
	JobSucceeded = TestGetIntField(Response, TEXT("succeeded"), JobSucceeded);
	JobFailed = TestGetIntField(Response, TEXT("failed"), JobFailed);
	bJobRunning = TestGetBoolField(Response, TEXT("running"), JobState == TEXT("running") || JobState == TEXT("preparing") || JobState == TEXT("cancelling"));

	if (JobState == TEXT("running") || JobState == TEXT("preparing") || JobState == TEXT("cancelling"))
	{
		LastResult = FString::Printf(TEXT("后台装配中：%d/%d，已摆放 %d。"), JobCompleted, JobTotal, JobSpawned);
	}
	else if (JobState == TEXT("done") || JobState == TEXT("cancelled") || JobState == TEXT("error"))
	{
		LastResult = FString::Printf(TEXT("后台装配%s：%d/%d，已摆放 %d，失败 %d。"), JobState == TEXT("done") ? TEXT("完成") : (JobState == TEXT("cancelled") ? TEXT("已取消") : TEXT("失败")), JobCompleted, JobTotal, JobSpawned, JobFailed);
	}

	FString Lines = LastResult;
	const TSharedPtr<FJsonObject> Cleanup = TestGetObjectField(Response, TEXT("cleanup"));
	if (Cleanup.IsValid())
	{
		Lines += FString::Printf(TEXT("\n启动时已清理：%d 个旧结果。"), TestGetIntField(Cleanup, TEXT("deleted_count"), 0));
	}

	const FString Error = TestGetStringField(Response, TEXT("error"));
	if (!Error.IsEmpty())
	{
		Lines += FString::Printf(TEXT("\n错误：%s"), *Error);
	}

	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	if (Response->TryGetArrayField(TEXT("items"), Items))
	{
		int32 ItemIndex = 0;
		for (const TSharedPtr<FJsonValue>& ItemValue : *Items)
		{
			const TSharedPtr<FJsonObject> Item = ItemValue.IsValid() ? ItemValue->AsObject() : nullptr;
			if (!Item.IsValid())
			{
				continue;
			}

			const TSharedPtr<FJsonObject> Actor = TestGetObjectField(Item, TEXT("actor"));
			const TSharedPtr<FJsonObject> Search = TestGetObjectField(Item, TEXT("search"));
			const int32 HitCount = Search.IsValid() ? TestGetIntField(Search, TEXT("hit_count"), 0) : 0;
			const int32 CandidateCount = Search.IsValid() ? TestGetIntField(Search, TEXT("candidate_count"), 0) : 0;
			Lines += FString::Printf(
				TEXT("\n\n%d. %s | 状态：%s | 图搜命中：%d | 候选：%d"),
				ItemIndex + 1,
				*TestGetStringField(Actor, TEXT("label"), TEXT("Actor")),
				*TestGetStringField(Item, TEXT("status"), TEXT("unknown")),
				HitCount,
				CandidateCount);

			const FString ItemError = TestGetStringField(Item, TEXT("error"));
			if (!ItemError.IsEmpty())
			{
				Lines += FString::Printf(TEXT("\n  原因：%s"), *ItemError);
			}
			++ItemIndex;
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* LogLines = nullptr;
	if (Response->TryGetArrayField(TEXT("log_lines"), LogLines) && LogLines->Num() > 0)
	{
		Lines += TEXT("\n\n任务日志：");
		for (const TSharedPtr<FJsonValue>& LineValue : *LogLines)
		{
			Lines += FString::Printf(TEXT("\n  %s"), *LineValue->AsString());
		}
	}

	AppendLog(Lines);
}

void SSceneAssemblyTestPanel::AppendLog(const FString& Message)
{
	LogText = Message;
}

void SSceneAssemblyTestPanel::LoadCaptureMetadataFromJson()
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

	const TSharedPtr<FJsonObject> Camera = TestGetObjectField(Root, TEXT("camera"));
	if (Camera.IsValid())
	{
		const TSharedPtr<FJsonObject> Location = TestGetObjectField(Camera, TEXT("location"));
		const TSharedPtr<FJsonObject> Rotation = TestGetObjectField(Camera, TEXT("rotation"));
		bHasCaptureCamera = TestJsonVector(Location, CaptureCameraLocation) && TestJsonRotator(Rotation, CaptureCameraRotation);
		CaptureCameraFov = TestGetNumberField(Camera, TEXT("fov_horizontal"), 90.0);
		if (const TSharedPtr<FJsonObject> Resolution = TestGetObjectField(Camera, TEXT("resolution")))
		{
			CaptureImageWidth = TestGetIntField(Resolution, TEXT("width"), 0);
			CaptureImageHeight = TestGetIntField(Resolution, TEXT("height"), 0);
		}
	}

	if (CaptureImageWidth <= 0 || CaptureImageHeight <= 0)
	{
		if (const TSharedPtr<FJsonObject> ImageSize = TestGetObjectField(Root, TEXT("image_size")))
		{
			CaptureImageWidth = TestGetIntField(ImageSize, TEXT("width"), 0);
			CaptureImageHeight = TestGetIntField(ImageSize, TEXT("height"), 0);
		}
	}
}

void SSceneAssemblyTestPanel::RefreshCaptureBrushes()
{
	SceneCaptureBrush.Reset();
	IdMapBrush.Reset();
	if (!CapturedSceneImagePath.IsEmpty())
	{
		SceneCaptureBrush = TestLoadBrushFromImage(CapturedSceneImagePath, TEXT("Scene"));
	}
	if (!CapturedIdMapPath.IsEmpty())
	{
		IdMapBrush = TestLoadBrushFromImage(CapturedIdMapPath, TEXT("IdMap"));
	}
}

void SSceneAssemblyTestPanel::RefreshConceptBrush()
{
	ConceptArtBrush.Reset();
	if (!ConceptArtPath.IsEmpty())
	{
		ConceptArtBrush = TestLoadBrushFromImage(ConceptArtPath, TEXT("Concept"), CaptureImageWidth, CaptureImageHeight);
	}
}

FString SSceneAssemblyTestPanel::CropPreviewDirectory() const
{
	return CaptureOutputDir.IsEmpty() ? FString() : FPaths::Combine(CaptureOutputDir, TEXT("actor_crops"));
}

FString SSceneAssemblyTestPanel::CropPreviewPathForActor(const FString& ActorPath, const int32 Index) const
{
	return FPaths::Combine(CropPreviewDirectory(), FString::Printf(TEXT("%03d_%s.png"), Index + 1, *TestSanitizeForFileName(ActorPath)));
}

void SSceneAssemblyTestPanel::RebuildCropPreviews()
{
	CropPreviews.Reset();
	if (CapturedJsonPath.IsEmpty() || ConceptArtPath.IsEmpty() || CaptureImageWidth <= 0 || CaptureImageHeight <= 0)
	{
		RefreshCropPreviewWidget();
		return;
	}

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *CapturedJsonPath))
	{
		AppendLog(FString::Printf(TEXT("无法读取裁剪预览 JSON：%s"), *CapturedJsonPath));
		RefreshCropPreviewWidget();
		return;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		AppendLog(FString::Printf(TEXT("裁剪预览 JSON 无法解析：%s"), *CapturedJsonPath));
		RefreshCropPreviewWidget();
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
	if (!Root->TryGetArrayField(TEXT("id_map"), Entries) || Entries == nullptr)
	{
		RefreshCropPreviewWidget();
		return;
	}

	const USceneAssemblyTestSettings* CurrentSettings = Settings.Get();
	const int32 ExpandPixels = CurrentSettings ? FMath::Max(0, CurrentSettings->CropExpandPixels) : 20;
	const ESceneAssemblyCropBboxSource BboxSource = CurrentSettings ? CurrentSettings->CropBboxSource : ESceneAssemblyCropBboxSource::FullProjected;
	const TCHAR* BboxFieldName = BboxSource == ESceneAssemblyCropBboxSource::VisiblePixels ? TEXT("pixel_bbox") : TEXT("full_bbox");
	int32 EntryIndex = 0;
	for (const TSharedPtr<FJsonValue>& Value : *Entries)
	{
		const TSharedPtr<FJsonObject> EntryObject = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!EntryObject.IsValid())
		{
			continue;
		}

		FIntRect PixelBounds;
		if (!TestReadBboxField(EntryObject, BboxFieldName, PixelBounds))
		{
			continue;
		}

		const FString ActorPath = TestGetStringField(EntryObject, TEXT("actor_path"), FString());
		if (ActorPath.IsEmpty())
		{
			continue;
		}

		const FString CropPath = CropPreviewPathForActor(ActorPath, EntryIndex);
		if (!USceneCaptureLibrary::CropImageRegionToFile(ConceptArtPath, CaptureImageWidth, CaptureImageHeight, PixelBounds.Min.X, PixelBounds.Min.Y, PixelBounds.Max.X, PixelBounds.Max.Y, ExpandPixels, CropPath))
		{
			++EntryIndex;
			continue;
		}

		TSharedPtr<FCropPreviewEntry> Preview = MakeShared<FCropPreviewEntry>();
		Preview->ActorPath = ActorPath;
		Preview->ActorLabel = FPaths::GetBaseFilename(ActorPath);
		Preview->CropPath = CropPath;
		Preview->PixelBounds = PixelBounds;
		Preview->Brush = TestLoadBrushFromImage(CropPath, FString::Printf(TEXT("Crop_%d"), EntryIndex));
		CropPreviews.Add(Preview);
		++EntryIndex;
	}

	RefreshCropPreviewWidget();
}

void SSceneAssemblyTestPanel::RefreshCropPreviewWidget()
{
	if (!CropPreviewContainer.IsValid())
	{
		return;
	}

	CropPreviewContainer->ClearChildren();
	if (CropPreviews.IsEmpty())
	{
		CropPreviewContainer->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoCropPreviews", "暂无裁剪预览。请先截取白盒场景并上传原画。"))
			.ColorAndOpacity(FSlateColor(TestMutedColor))
			.AutoWrapText(true)
		];
		return;
	}

	for (const TSharedPtr<FCropPreviewEntry>& Preview : CropPreviews)
	{
		if (!Preview.IsValid())
		{
			continue;
		}

		const FString ActorPath = Preview->ActorPath;
		CropPreviewContainer->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(128.0f)
				.HeightOverride(96.0f)
				[
					SNew(SScaleBox)
					.Stretch(EStretch::ScaleToFit)
					.StretchDirection(EStretchDirection::Both)
					[
						SNew(SImage).Image(Preview->Brush.IsValid() ? Preview->Brush.Get() : FAppStyle::GetBrush("Brushes.Recessed"))
					]
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(
					TEXT("%s\n[%d,%d,%d,%d]"),
					*Preview->ActorLabel,
					Preview->PixelBounds.Min.X,
					Preview->PixelBounds.Min.Y,
					Preview->PixelBounds.Max.X,
					Preview->PixelBounds.Max.Y)))
				.AutoWrapText(true)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("SelectPreviewActor", "选择"))
				.OnClicked(this, &SSceneAssemblyTestPanel::OnSelectPreviewActorClicked, ActorPath)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("FocusPreviewActor", "Focus"))
				.OnClicked(this, &SSceneAssemblyTestPanel::OnFocusPreviewActorClicked, ActorPath)
			]
		];
	}
}

bool SSceneAssemblyTestPanel::IsBlockoutActor(const AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}
	static TWeakObjectPtr<UClass> CachedBlockoutBaseClass;
	if (!CachedBlockoutBaseClass.IsValid())
	{
		CachedBlockoutBaseClass = StaticLoadClass(AActor::StaticClass(), nullptr, TEXT("/Script/Blockout.BlockoutBaseDynamicMeshActor"));
	}
	if (UClass* BlockoutBaseClass = CachedBlockoutBaseClass.Get())
	{
		return Actor->GetClass() && Actor->GetClass()->IsChildOf(BlockoutBaseClass);
	}
	return Actor->Tags.Contains(FName(TEXT("BlockoutActor")));
}

bool SSceneAssemblyTestPanel::IsActorInViewFrustum(const AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}
	if (!GCurrentLevelEditingViewportClient || !GCurrentLevelEditingViewportClient->Viewport)
	{
		return true;
	}

	FLevelEditorViewportClient* ViewportClient = GCurrentLevelEditingViewportClient;
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		ViewportClient->Viewport,
		ViewportClient->GetScene(),
		ViewportClient->EngineShowFlags).SetRealtimeUpdate(ViewportClient->IsRealtime()));
	const FSceneView* View = ViewportClient->CalcSceneView(&ViewFamily);
	if (!View)
	{
		return true;
	}

	FVector Origin = FVector::ZeroVector;
	FVector Extent = FVector::ZeroVector;
	Actor->GetActorBounds(false, Origin, Extent);
	return View->ViewFrustum.IntersectBox(Origin, Extent);
}

void SSceneAssemblyTestPanel::CollectSelectedBlockoutActors(TArray<AActor*>& OutActors) const
{
	OutActors.Reset();
	if (!GEditor)
	{
		return;
	}
	if (USelection* SelectedActors = GEditor->GetSelectedActors())
	{
		for (FSelectionIterator It(*SelectedActors); It; ++It)
		{
			AActor* Actor = Cast<AActor>(*It);
			if (IsBlockoutActor(Actor))
			{
				OutActors.Add(Actor);
			}
		}
	}
}

void SSceneAssemblyTestPanel::CollectVisibleBlockoutActors(TArray<AActor*>& OutActors) const
{
	OutActors.Reset();
	if (!GEditor)
	{
		return;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (IsBlockoutActor(Actor) && IsActorInViewFrustum(Actor))
		{
			OutActors.Add(Actor);
		}
	}
}

bool SSceneAssemblyTestPanel::CaptureAestheticReference(const TArray<AActor*>& TargetActors)
{
	if (TargetActors.IsEmpty())
	{
		LastResult = TEXT("当前相机可视范围内没有可截取的白盒 Actor。");
		AppendLog(LastResult);
		return false;
	}

	CaptureBaseName = TestTimestampBaseName();
	CaptureOutputDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealSceneAssembly"), TEXT("AestheticRef"), CaptureBaseName);
	CapturedSceneImagePath = FPaths::Combine(CaptureOutputDir, CaptureBaseName + TEXT("_scene.png"));
	CapturedIdMapPath = FPaths::Combine(CaptureOutputDir, CaptureBaseName + TEXT("_id.png"));
	CapturedJsonPath = FPaths::Combine(CaptureOutputDir, CaptureBaseName + TEXT(".json"));

	if (!USceneCaptureLibrary::CaptureSceneAndIdMapFromActors(TargetActors, CaptureOutputDir, CaptureBaseName, 0, 0, true))
	{
		LastResult = TEXT("美学参考截取失败。请确认当前有活动透视视口且 ID 材质可用。");
		AppendLog(LastResult);
		return false;
	}

	LoadCaptureMetadataFromJson();
	RefreshCaptureBrushes();
	RefreshConceptBrush();
	RebuildCropPreviews();
	LastResult = FString::Printf(TEXT("已截取美学参考：%d 个白盒"), TargetActors.Num());
	AppendLog(LastResult);
	return true;
}

FReply SSceneAssemblyTestPanel::OpenContainingFolder(const FString& FilePath)
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

void SSceneAssemblyTestPanel::UpdateSelectionSummaryFromEditor()
{
	SelectedCount = 0;
	SelectedWhiteboxCount = 0;
	if (GEditor != nullptr)
	{
		if (USelection* SelectedActors = GEditor->GetSelectedActors())
		{
			for (FSelectionIterator It(*SelectedActors); It; ++It)
			{
				AActor* Actor = Cast<AActor>(*It);
				if (Actor == nullptr)
				{
					continue;
				}
				++SelectedCount;
				if (IsBlockoutActor(Actor))
				{
					++SelectedWhiteboxCount;
				}
			}
		}
	}
	SelectionSummary = FString::Printf(TEXT("已选 %d 个 Actor（白盒 %d 个）。"), SelectedCount, SelectedWhiteboxCount);
}

EActiveTimerReturnType SSceneAssemblyTestPanel::RefreshStatusTick(double InCurrentTime, float InDeltaTime)
{
	UpdateSelectionSummaryFromEditor();
	if (bJobRunning)
	{
		TSharedPtr<FJsonObject> Response;
		if (CallController(TEXT("poll_assembly_status_json()"), Response))
		{
			ApplyAsyncStatusResponse(Response);
		}
	}
	return EActiveTimerReturnType::Continue;
}

FReply SSceneAssemblyTestPanel::OnSelectAllWhiteboxesClicked()
{
	if (GEditor != nullptr)
	{
		TArray<AActor*> WhiteboxActors;
		if (UWorld* World = GEditor->GetEditorWorldContext().World())
		{
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				AActor* Actor = *It;
				if (IsBlockoutActor(Actor))
				{
					WhiteboxActors.Add(Actor);
				}
			}
		}

		const FScopedTransaction Transaction(LOCTEXT("SelectAllWhiteboxesTransaction", "Scene Assembly: Select All Whiteboxes"));
		GEditor->SelectNone(false, true, false);
		for (AActor* Actor : WhiteboxActors)
		{
			GEditor->SelectActor(Actor, true, false, true, true);
		}
		GEditor->NoteSelectionChange();
		UpdateSelectionSummaryFromEditor();
		LastResult = TEXT("已全选白盒。");
	}
	return FReply::Handled();
}

FReply SSceneAssemblyTestPanel::OnSelectVisibleWhiteboxesClicked()
{
	if (GEditor != nullptr)
	{
		TArray<AActor*> WhiteboxActors;
		CollectVisibleBlockoutActors(WhiteboxActors);

		const FScopedTransaction Transaction(LOCTEXT("SelectVisibleWhiteboxesTransaction", "Scene Assembly: Select Visible Whiteboxes"));
		GEditor->SelectNone(false, true, false);
		for (AActor* Actor : WhiteboxActors)
		{
			GEditor->SelectActor(Actor, true, false, true, true);
		}
		GEditor->NoteSelectionChange();
		UpdateSelectionSummaryFromEditor();
		LastResult = FString::Printf(TEXT("已选择可视范围白盒：%d 个。"), WhiteboxActors.Num());
		AppendLog(LastResult);
	}
	return FReply::Handled();
}

FReply SSceneAssemblyTestPanel::OnDeselectClicked()
{
	if (GEditor != nullptr)
	{
		const FScopedTransaction Transaction(LOCTEXT("DeselectAllTransaction", "Scene Assembly: Deselect All"));
		GEditor->SelectNone(false, true, false);
		GEditor->NoteSelectionChange();
		UpdateSelectionSummaryFromEditor();
		LastResult = TEXT("已取消选择。");
	}
	return FReply::Handled();
}

FReply SSceneAssemblyTestPanel::OnCaptureAestheticReferenceClicked()
{
	TArray<AActor*> TargetActors;
	CollectVisibleBlockoutActors(TargetActors);
	if (TargetActors.IsEmpty())
	{
		LastResult = TEXT("当前相机可视范围内没有 Blockout 白盒 Actor。");
		AppendLog(LastResult);
		return FReply::Handled();
	}

	CaptureAestheticReference(TargetActors);
	return FReply::Handled();
}

FReply SSceneAssemblyTestPanel::OnCaptureSelectedAestheticReferenceClicked()
{
	TArray<AActor*> SelectedActors;
	CollectSelectedBlockoutActors(SelectedActors);

	TArray<AActor*> TargetActors;
	for (AActor* Actor : SelectedActors)
	{
		if (IsActorInViewFrustum(Actor))
		{
			TargetActors.Add(Actor);
		}
	}

	if (TargetActors.IsEmpty())
	{
		LastResult = TEXT("当前相机可视范围内没有已选中的 Blockout 白盒 Actor。");
		AppendLog(LastResult);
		return FReply::Handled();
	}

	CaptureAestheticReference(TargetActors);
	return FReply::Handled();
}

FReply SSceneAssemblyTestPanel::OnJumpToCaptureCameraClicked()
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
	LastResult = TEXT("已跳转到捕获相机视角。");
	return FReply::Handled();
}

FReply SSceneAssemblyTestPanel::OnUploadConceptArtClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		LastResult = TEXT("无法打开文件选择器。");
		AppendLog(LastResult);
		return FReply::Handled();
	}

	TArray<FString> SelectedFiles;
	const void* ParentWindowHandle = nullptr;
	if (FSlateApplication::IsInitialized())
	{
		const TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindBestParentWindowForDialogs(nullptr);
		ParentWindowHandle = ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid()
			? ParentWindow->GetNativeWindow()->GetOSWindowHandle()
			: nullptr;
	}

	const bool bOpened = DesktopPlatform->OpenFileDialog(
		ParentWindowHandle,
		TEXT("选择原画"),
		FPaths::ProjectSavedDir(),
		TEXT(""),
		TEXT("Image Files (*.png;*.jpg;*.jpeg;*.bmp;*.webp)|*.png;*.jpg;*.jpeg;*.bmp;*.webp|All Files (*.*)|*.*"),
		EFileDialogFlags::None,
		SelectedFiles);

	if (bOpened && SelectedFiles.Num() > 0)
	{
		ConceptArtPath = SelectedFiles[0];
		RefreshConceptBrush();
		RebuildCropPreviews();
	}
	return FReply::Handled();
}

FReply SSceneAssemblyTestPanel::OnOpenSceneCaptureFolderClicked()
{
	return OpenContainingFolder(CapturedSceneImagePath);
}

FReply SSceneAssemblyTestPanel::OnOpenIdMapFolderClicked()
{
	return OpenContainingFolder(CapturedIdMapPath);
}

FReply SSceneAssemblyTestPanel::OnOpenConceptArtFolderClicked()
{
	return OpenContainingFolder(ConceptArtPath);
}

FReply SSceneAssemblyTestPanel::OnRefreshCropPreviewsClicked()
{
	RebuildCropPreviews();
	LastResult = FString::Printf(TEXT("已刷新裁剪预览：%d 张。"), CropPreviews.Num());
	AppendLog(LastResult);
	return FReply::Handled();
}

FReply SSceneAssemblyTestPanel::OnSelectPreviewActorClicked(FString ActorPath)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("actor_path"), ActorPath);
	FString PayloadJson;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadJson);
	FJsonSerializer::Serialize(Root, Writer);
	FTCHARToUTF8 PayloadUtf8(*PayloadJson);
	const FString PayloadBase64 = FBase64::Encode(reinterpret_cast<const uint8*>(PayloadUtf8.Get()), PayloadUtf8.Length());

	TSharedPtr<FJsonObject> Response;
	if (CallController(FString::Printf(TEXT("select_actor_by_path_json('%s')"), *PayloadBase64), Response))
	{
		LastResult = TEXT("已选择裁剪预览对应白盒。");
		UpdateSelectionSummaryFromEditor();
	}
	return FReply::Handled();
}

FReply SSceneAssemblyTestPanel::OnFocusPreviewActorClicked(FString ActorPath)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("actor_path"), ActorPath);
	FString PayloadJson;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadJson);
	FJsonSerializer::Serialize(Root, Writer);
	FTCHARToUTF8 PayloadUtf8(*PayloadJson);
	const FString PayloadBase64 = FBase64::Encode(reinterpret_cast<const uint8*>(PayloadUtf8.Get()), PayloadUtf8.Length());

	TSharedPtr<FJsonObject> Response;
	if (CallController(FString::Printf(TEXT("focus_actor_by_path_json('%s')"), *PayloadBase64), Response))
	{
		LastResult = TEXT("已选择并 Focus 裁剪预览对应白盒。");
		UpdateSelectionSummaryFromEditor();
	}
	return FReply::Handled();
}

FReply SSceneAssemblyTestPanel::OnSolvePlaceClicked()
{
	if (bJobRunning)
	{
		LastResult = TEXT("后台装配任务正在运行。");
		AppendLog(LastResult);
		return FReply::Handled();
	}
	UpdateSelectionSummaryFromEditor();
	const FString PayloadJson = BuildPayloadJson();
	FTCHARToUTF8 PayloadUtf8(*PayloadJson);
	const FString PayloadBase64 = FBase64::Encode(reinterpret_cast<const uint8*>(PayloadUtf8.Get()), PayloadUtf8.Length());
	TSharedPtr<FJsonObject> Response;
	if (CallController(FString::Printf(TEXT("start_async_assembly_json('%s')"), *PayloadBase64), Response))
	{
		ApplyAsyncStatusResponse(Response);
	}
	return FReply::Handled();
}

FReply SSceneAssemblyTestPanel::OnCancelJobClicked()
{
	TSharedPtr<FJsonObject> Response;
	if (CallController(TEXT("cancel_assembly_json()"), Response))
	{
		ApplyAsyncStatusResponse(Response);
	}
	return FReply::Handled();
}

FReply SSceneAssemblyTestPanel::OnCleanupClicked()
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("result_tag"), GetResultTag());
	FString PayloadJson;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadJson);
	FJsonSerializer::Serialize(Root, Writer);
	FTCHARToUTF8 PayloadUtf8(*PayloadJson);
	const FString PayloadBase64 = FBase64::Encode(reinterpret_cast<const uint8*>(PayloadUtf8.Get()), PayloadUtf8.Length());

	TSharedPtr<FJsonObject> Response;
	if (CallController(FString::Printf(TEXT("cleanup_assembly_results_json('%s')"), *PayloadBase64), Response))
	{
		const int32 Deleted = TestGetIntField(Response, TEXT("deleted_count"), 0);
		LastResult = FString::Printf(TEXT("清理删除了 %d 个 Actor。"), Deleted);
		AppendLog(LastResult);
	}
	return FReply::Handled();
}

FText SSceneAssemblyTestPanel::GetSelectionText() const
{
	return FText::FromString(SelectionSummary);
}

FText SSceneAssemblyTestPanel::GetLastResultText() const
{
	return FText::FromString(FString::Printf(TEXT("%s | %s"), *SelectionSummary, *LastResult));
}

FText SSceneAssemblyTestPanel::GetLogText() const
{
	return FText::FromString(FString::Printf(TEXT("%s\n\n%s"), *SelectionSummary, *LogText));
}

FText SSceneAssemblyTestPanel::GetCaptureInfoText() const
{
	if (CapturedJsonPath.IsEmpty())
	{
		return LOCTEXT("NoCaptureInfo", "尚未截取白盒场景。");
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

FText SSceneAssemblyTestPanel::GetConceptArtInfoText() const
{
	if (ConceptArtPath.IsEmpty())
	{
		return LOCTEXT("NoConceptArtInfo", "尚未上传原画。");
	}
	return FText::GetEmpty();
}

const FSlateBrush* SSceneAssemblyTestPanel::GetSceneCaptureBrush() const
{
	return SceneCaptureBrush.IsValid() ? SceneCaptureBrush.Get() : FAppStyle::GetBrush("Brushes.Recessed");
}

const FSlateBrush* SSceneAssemblyTestPanel::GetIdMapBrush() const
{
	return IdMapBrush.IsValid() ? IdMapBrush.Get() : FAppStyle::GetBrush("Brushes.Recessed");
}

const FSlateBrush* SSceneAssemblyTestPanel::GetConceptArtBrush() const
{
	return ConceptArtBrush.IsValid() ? ConceptArtBrush.Get() : FAppStyle::GetBrush("Brushes.Recessed");
}

FText SSceneAssemblyTestPanel::GetCropPreviewSummaryText() const
{
	const USceneAssemblyTestSettings* CurrentSettings = Settings.Get();
	const ESceneAssemblyCropBboxSource BboxSource = CurrentSettings ? CurrentSettings->CropBboxSource : ESceneAssemblyCropBboxSource::FullProjected;
	return FText::FromString(FString::Printf(TEXT("逐 Actor 裁剪预览（%s，%d 张）"), *TestCropBboxSourceDisplayName(BboxSource), CropPreviews.Num()));
}

bool SSceneAssemblyTestPanel::HasCaptureCamera() const
{
	return bHasCaptureCamera;
}

bool SSceneAssemblyTestPanel::HasSceneCapturePath() const
{
	return !CapturedSceneImagePath.IsEmpty();
}

bool SSceneAssemblyTestPanel::HasIdMapPath() const
{
	return !CapturedIdMapPath.IsEmpty();
}

bool SSceneAssemblyTestPanel::HasConceptArtPath() const
{
	return !ConceptArtPath.IsEmpty();
}

bool SSceneAssemblyTestPanel::CanUploadConceptArt() const
{
	return !CapturedJsonPath.IsEmpty() && CaptureImageWidth > 0 && CaptureImageHeight > 0;
}

bool SSceneAssemblyTestPanel::CanRun() const
{
	return !bJobRunning && !CapturedJsonPath.IsEmpty() && !ConceptArtPath.IsEmpty();
}

bool SSceneAssemblyTestPanel::CanCleanup() const
{
	return !bJobRunning;
}

bool SSceneAssemblyTestPanel::CanCancelJob() const
{
	return bJobRunning;
}

TOptional<float> SSceneAssemblyTestPanel::GetJobProgress() const
{
	return JobTotal > 0 ? TOptional<float>(FMath::Clamp(static_cast<float>(JobCompleted) / static_cast<float>(JobTotal), 0.0f, 1.0f)) : TOptional<float>(0.0f);
}

FText SSceneAssemblyTestPanel::GetJobProgressText() const
{
	if (JobState == TEXT("idle") || JobTotal <= 0)
	{
		return LOCTEXT("NoAssemblyJobProgress", "暂无后台装配任务。");
	}
	if (bJobRunning)
	{
		return FText::FromString(FString::Printf(TEXT("后台求解中：%d/%d | 已摆放 %d | 失败 %d"), JobCompleted, JobTotal, JobSpawned, JobFailed));
	}
	if (JobState == TEXT("done"))
	{
		return FText::FromString(FString::Printf(TEXT("完成：%d/%d | 已摆放 %d | 失败 %d"), JobCompleted, JobTotal, JobSpawned, JobFailed));
	}
	if (JobState == TEXT("cancelled"))
	{
		return FText::FromString(FString::Printf(TEXT("已取消：%d/%d | 已摆放 %d | 失败 %d"), JobCompleted, JobTotal, JobSpawned, JobFailed));
	}
	if (JobState == TEXT("error"))
	{
		return FText::FromString(FString::Printf(TEXT("失败：%d/%d | 已摆放 %d | 失败 %d"), JobCompleted, JobTotal, JobSpawned, JobFailed));
	}
	return FText::FromString(FString::Printf(TEXT("%s：%d/%d | 已摆放 %d"), *JobState, JobCompleted, JobTotal, JobSpawned));
}

EVisibility SSceneAssemblyTestPanel::GetJobProgressVisibility() const
{
	return (bJobRunning || JobState == TEXT("done") || JobState == TEXT("cancelled") || JobState == TEXT("error")) ? EVisibility::Visible : EVisibility::Collapsed;
}

FString SSceneAssemblyTestPanel::GetResultTag() const
{
	const FString ResultTag = Settings.IsValid() ? Settings->ResultTag.TrimStartAndEnd() : FString();
	return ResultTag.IsEmpty() ? TEXT("SceneAssemblyResult") : ResultTag;
}

#undef LOCTEXT_NAMESPACE
