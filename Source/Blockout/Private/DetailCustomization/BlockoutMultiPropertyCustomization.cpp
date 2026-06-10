#include "DetailCustomization/BlockoutMultiPropertyCustomization.h"

#include "BlockoutLog.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "DetailWidgetRow.h"


#define LOCTEXT_NAMESPACE "Blockout"

TSharedRef<IPropertyTypeCustomization> FBlockoutMultiPropertyCustomization::MakeInstance()
{
	return MakeShared<FBlockoutMultiPropertyCustomization>();
}

void FBlockoutMultiPropertyCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	const TMap<FName, FString>* MetaDataMap = PropertyHandle->GetProperty()->GetMetaDataMap();
	// UE_LOG(LogBlockout, Warning, TEXT("MetaDataMap->Num(): %d"), MetaDataMap->Num());
	
	TSharedRef<SHorizontalBox> CombinedValueWidget = SNew(SHorizontalBox);
	uint32 NumChildren;
	PropertyHandle->GetNumChildren(NumChildren);
	for (uint32 Index = 0; Index < NumChildren; ++Index)
	{
		TSharedRef<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(Index).ToSharedRef();
		uint32 NumChildChildren;
		ChildHandle->GetNumChildren(NumChildChildren);
		if (NumChildChildren > 0 &&
			ChildHandle->GetChildHandle(FName(TEXT("bShowTextLabel"))).IsValid() &&
			ChildHandle->GetChildHandle(FName(TEXT("TextLabel"))).IsValid())
		{
			// TSharedRef<IPropertyHandle> ValuePropertyHandle = ChildHandle->GetChildHandle(FName("Value")).ToSharedRef();
			// for (auto& MetaData : *MetaDataMap)
			// {
			// 	ValuePropertyHandle->SetInstanceMetaData(MetaData.Key, MetaData.Value);
			// 	// UE_LOG(LogBlockout, Warning, TEXT("MetaData.Key: %s, MetaData.Value: %s"), *MetaData.Key.ToString(), *MetaData.Value);
			// }
			// 	
			// CombinedValueWidget->AddSlot()
			// .Padding(FMargin(4.0, 0.0, 0.0, 0.0))
			// [
			// 	ValuePropertyHandle->CreatePropertyValueWidget()
			// ];
			
			for (uint32 ChildIndex = 0; ChildIndex < NumChildChildren; ++ChildIndex)
			{
				TSharedRef<IPropertyHandle> ChildChildHandle = ChildHandle->GetChildHandle(ChildIndex).ToSharedRef();
				
				if (ChildChildHandle->GetProperty()->GetName() == TEXT("bShowTextLabel") ||
					ChildChildHandle->GetProperty()->GetName() == TEXT("TextLabel"))
				{
					continue;
				}
			
				for (auto& MetaData : *MetaDataMap)
			 	{
					ChildChildHandle->SetInstanceMetaData(MetaData.Key, MetaData.Value);
					// UE_LOG(LogBlockout, Warning, TEXT("MetaData.Key: %s, MetaData.Value: %s"), *MetaData.Key.ToString(), *MetaData.Value);
				}
				
				CombinedValueWidget->AddSlot()
				.Padding(FMargin(4.0, 0.0, 0.0, 0.0))
				[
					ChildChildHandle->CreatePropertyValueWidget()
				];
			}
		}else
		{
			CombinedValueWidget->AddSlot()
			.Padding(FMargin(4.0, 0.0, 0.0, 0.0))
			[
				ChildHandle->CreatePropertyValueWidget()
			];
		}
	}
	
	HeaderRow
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(NumChildren*125.0f)
	[
		CombinedValueWidget
	];
}

void FBlockoutMultiPropertyCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	uint32 NumChildren;
	PropertyHandle->GetNumChildren(NumChildren);
	for (uint32 Index = 0; Index < NumChildren; ++Index)
	{
		TSharedRef<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(Index).ToSharedRef();
		ChildBuilder.AddProperty(ChildHandle);
	}
}

void FBlockoutMultiPropertyCustomization::MakeCustomizationUI(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& Row)
{
	
}

