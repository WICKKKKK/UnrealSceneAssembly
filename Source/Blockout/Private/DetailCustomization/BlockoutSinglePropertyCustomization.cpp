#include "DetailCustomization/BlockoutSinglePropertyCustomization.h"

#include "BlockoutLog.h"
#include "PropertyHandle.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"

#define LOCTEXT_NAMESPACE "Blockout"

TSharedRef<IPropertyTypeCustomization> FBlockoutSinglePropertyCustomization::MakeInstance()
{
	return MakeShared<FBlockoutSinglePropertyCustomization>();
}

void FBlockoutSinglePropertyCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TSharedRef<IPropertyHandle> BoolPropertyHandle = PropertyHandle->GetChildHandle(FName("bShowTextLabel")).ToSharedRef();
	TSharedRef<IPropertyHandle> StringPropertyHandle = PropertyHandle->GetChildHandle(FName("TextLabel")).ToSharedRef();
	
	TSharedRef<SHorizontalBox> CombinedNameWidget = SNew(SHorizontalBox)
	+SHorizontalBox::Slot()
	.AutoWidth()
	[
		SNew(SBox)
		.ToolTipText(LOCTEXT("Toggle Text Preview", "Toggle Text Preview"))
		[
			BoolPropertyHandle->CreatePropertyValueWidget()
		]
	]
	+SHorizontalBox::Slot()
	.Padding(FMargin(4.0, 0.0, 0.0, 0.0))
	.FillWidth(0.5f)
	[
		SNew(SBox)
		.ToolTipText(LOCTEXT("Parameter Text DisplayName", "Parameter Text DisplayName"))
		.Visibility_Lambda([BoolPropertyHandle]() -> EVisibility
		{
			bool bShowTextLabel = false;
			BoolPropertyHandle->GetValue(bShowTextLabel);
			// UE_LOG(LogBlockout, Warning, TEXT("bShowTextLabel: %s"), bShowTextLabel?TEXT("True"):TEXT("False"));
			return bShowTextLabel ? EVisibility::Visible : EVisibility::Collapsed;
		})
		[
			StringPropertyHandle->CreatePropertyValueWidget()
		]
	]
	+SHorizontalBox::Slot()
	.Padding(FMargin(4.0, 0.0, 0.0, 0.0))
	.FillWidth(1.0f)
	[
		PropertyHandle->CreatePropertyNameWidget()
	];
	
	const TMap<FName, FString>* MetaDataMap = PropertyHandle->GetProperty()->GetMetaDataMap();
	// UE_LOG(LogBlockout, Warning, TEXT("MetaDataMap->Num(): %d"), MetaDataMap->Num());
	
	// TSharedRef<IPropertyHandle> ValuePropertyHandle = PropertyHandle->GetChildHandle(FName("Value")).ToSharedRef();
	//
	// for (auto& MetaData : *MetaDataMap)
	// {
	// 	ValuePropertyHandle->SetInstanceMetaData(MetaData.Key, MetaData.Value);
	// 	// UE_LOG(LogBlockout, Warning, TEXT("MetaData.Key: %s, MetaData.Value: %s"), *MetaData.Key.ToString(), *MetaData.Value);
	// }
	//
	// TSharedRef<IPropertyHandle> ModulePropertyHandle = PropertyHandle->GetChildHandle(FName("Module")).ToSharedRef();
	//
	// TSharedRef<SHorizontalBox> CombinedValueWidget = SNew(SHorizontalBox)
	// +SHorizontalBox::Slot()
	// .FillWidth(1.0)
	// [
	// 	ValuePropertyHandle->CreatePropertyValueWidget()
	// ];
	// +SHorizontalBox::Slot()
	// .Padding(FMargin(4.0, 0.0, 0.0, 0.0))
	// .FillWidth(1.0)
	// .HAlign(HAlign_Right)
	// [
	// 	SNew(STextBlock)
	// 	.Text(FText::FromString("Module: "))
	// ]
	// +SHorizontalBox::Slot()
	// .Padding(FMargin(4.0, 0.0, 0.0, 0.0))
	// [
	// 	ModulePropertyHandle->CreatePropertyValueWidget()
	// ];

	TSharedRef<SHorizontalBox> CombinedValueWidget = SNew(SHorizontalBox);
	uint32 NumChildren;
	PropertyHandle->GetNumChildren(NumChildren);
	for (uint32 Index = 0; Index < NumChildren; ++Index)
	{
		TSharedRef<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(Index).ToSharedRef();
		
		if (ChildHandle->GetProperty()->GetName() == BoolPropertyHandle->GetProperty()->GetName() ||
			ChildHandle->GetProperty()->GetName() == StringPropertyHandle->GetProperty()->GetName())
		{
			continue;
		}
	
		for (auto& MetaData : *MetaDataMap)
		{
			ChildHandle->SetInstanceMetaData(MetaData.Key, MetaData.Value);
			// UE_LOG(LogBlockout, Warning, TEXT("MetaData.Key: %s, MetaData.Value: %s"), *MetaData.Key.ToString(), *MetaData.Value);
		}
	
		// UE_LOG(LogBlockout, Warning, TEXT("ChildHandle->GetProperty()->GetName() = %s"), *ChildHandle->GetProperty()->GetName());
		CombinedValueWidget->AddSlot()
		.Padding(FMargin(4.0, 0.0, 0.0, 0.0))
		[
			ChildHandle->CreatePropertyValueWidget()
		];
	}
	
	HeaderRow
	.NameContent()
	[
		CombinedNameWidget
	]
	.ValueContent()
	[
		CombinedValueWidget
	];
}

void FBlockoutSinglePropertyCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// uint32 NumChildren;
	// PropertyHandle->GetNumChildren(NumChildren);
	//
	// for (uint32 Index = 0; Index < NumChildren; ++Index)
	// {
	// 	TSharedRef<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(Index).ToSharedRef();
	//
	// 	if (ChildHandle->GetProperty()->GetName() == GET_MEMBER_NAME_STRING_CHECKED(FBlockoutFloat, bShowTextLabel))
	// 	{
	// 		ChildBuilder.AddProperty(ChildHandle)
	// 			.DisplayName(LOCTEXT("BoolDisplayName", "Bool"))
	// 			.ToolTip(LOCTEXT("BoolToolTip", "Your tooltip text here."));
	// 	}
	// 	else if (ChildHandle->GetProperty()->GetName() == GET_MEMBER_NAME_STRING_CHECKED(FBlockoutFloat, TextLabel))
	// 	{
	// 		ChildBuilder.AddProperty(ChildHandle)
	// 			.DisplayName(LOCTEXT("StringDisplayName", "String"))
	// 			.ToolTip(LOCTEXT("StringToolTip", "Your tooltip text here."));
	// 	}
	// 	else if (ChildHandle->GetProperty()->GetName() == GET_MEMBER_NAME_STRING_CHECKED(FBlockoutFloat, Float))
	// 	{
	// 		ChildBuilder.AddProperty(ChildHandle)
	// 			.DisplayName(LOCTEXT("FloatDisplayName", "Float"))
	// 			.ToolTip(LOCTEXT("FloatToolTip", "Your tooltip text here."));
	// 	}
	// }
}


void FBlockoutSinglePropertyCustomization::MakeCustomizationUI(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& Row)
{
	
}

#undef LOCTEXT_NAMESPACE
