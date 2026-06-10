// Fill out your copyright notice in the Description page of Project Settings.


#include "Functions/BlockoutLibrary_BasicFunctions.h"

#include "UDynamicMesh.h"
#include "Components/SplineComponent.h"
#include "Components/TextRenderComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInstance.h"
#include "AssetToolsModule.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "AssetRegistry/AssetRegistryModule.h"



UMaterialInterface* UBlockoutLibrary_BasicFunctions::LoadBlockoutMaterial(const FString& Path)
{
	ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(*Path);
	if (MaterialFinder.Succeeded())
	{
		return MaterialFinder.Object;
	}
	return nullptr;
}

void UBlockoutLibrary_BasicFunctions::ClearBlockoutMaterial(UDynamicMeshComponent* _Comp)
{
	if (!IsValid(_Comp) || _Comp->GetNumMaterials() == 0)
		return;

	for (int32 Index = 0; Index < _Comp->GetNumMaterials(); ++Index)
	{
		_Comp->SetMaterial(Index, nullptr);
	}
}


UMaterialInstanceConstant* UBlockoutLibrary_BasicFunctions::ExportDynamicMaterialInstanceToMaterialInstance(
    UMaterialInstanceDynamic* DynamicMaterialInstance,
    const FString& SaveFolderPath,
    const FString& AssetName
) {
    if (!DynamicMaterialInstance) return nullptr;

    // 获取父材质
    UMaterialInterface* ParentMaterial = DynamicMaterialInstance->GetMaterial();
    if (!ParentMaterial) {
        UE_LOG(LogTemp, Error, TEXT("MID has no valid parent material."));
        return nullptr;
    }

    // 创建材质实例工厂并设置父材质
    UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
    Factory->InitialParent = ParentMaterial;

    // 创建资产工具模块
    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
    
    // 生成唯一资产名称和路径
    FString FinalPackageName = FPaths::Combine(SaveFolderPath, AssetName);
    FString OutAssetName;
    FString UniquePackageName;
    AssetToolsModule.Get().CreateUniqueAssetName(FinalPackageName, "", UniquePackageName, OutAssetName);

    // 创建新材质实例资产
    UObject* NewAsset = AssetToolsModule.Get().CreateAsset(
        OutAssetName,
        FPackageName::GetLongPackagePath(UniquePackageName),
        UMaterialInstanceConstant::StaticClass(),
        Factory
    );

    UMaterialInstanceConstant* NewDynamicMaterialInstance = Cast<UMaterialInstanceConstant>(NewAsset);
    if (!NewDynamicMaterialInstance) {
        UE_LOG(LogTemp, Error, TEXT("Failed to create material instance asset."));
        return nullptr;
    }

    // 复制标量参数
    TArray<FMaterialParameterInfo> ScalarParams;
    TArray<FGuid> ScalarGuids;
    DynamicMaterialInstance->GetAllScalarParameterInfo(ScalarParams, ScalarGuids);
    for (const FMaterialParameterInfo& Param : ScalarParams) {
        float Value;
        if (DynamicMaterialInstance->GetScalarParameterValue(Param, Value)) {
            NewDynamicMaterialInstance->SetScalarParameterValueEditorOnly(Param, Value);
        }
    }

    // 复制向量参数
    TArray<FMaterialParameterInfo> VectorParams;
    TArray<FGuid> VectorGuids;
    DynamicMaterialInstance->GetAllVectorParameterInfo(VectorParams, VectorGuids);
    for (const FMaterialParameterInfo& Param : VectorParams) {
        FLinearColor Value;
        if (DynamicMaterialInstance->GetVectorParameterValue(Param, Value)) {
            NewDynamicMaterialInstance->SetVectorParameterValueEditorOnly(Param, Value);
        }
    }

    // 复制纹理参数
    TArray<FMaterialParameterInfo> TextureParams;
    TArray<FGuid> TextureGuids;
    DynamicMaterialInstance->GetAllTextureParameterInfo(TextureParams, TextureGuids);
    for (const FMaterialParameterInfo& Param : TextureParams) {
        UTexture* Value;
        if (DynamicMaterialInstance->GetTextureParameterValue(Param, Value)) {
            NewDynamicMaterialInstance->SetTextureParameterValueEditorOnly(Param, Value);
        }
    }

    // 通知引擎资源已更新
    NewDynamicMaterialInstance->PostEditChange();
    NewDynamicMaterialInstance->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(NewDynamicMaterialInstance);

	return NewDynamicMaterialInstance;
}

bool UBlockoutLibrary_BasicFunctions::IsDynamicMeshValid(UDynamicMesh* DynamicMesh)
{
	if (!IsValid(DynamicMesh) || DynamicMesh->IsEmpty())
	{
		return false;
	}
	
	return true;
}

// void UBlockoutLibrary_BasicFunctions::GetSplinePointLocationWithInterp(
// 	USplineComponent* Spline,
// 	int InterpolateNum,
// 	TArray<int>& OutSplineIndexes,
// 	TArray<FVector>& OutSplinePoints,
// 	FVector& OutSplineCenter
// 	)
// {
// 	OutSplineCenter = FVector(0.0f, 0.0f, 0.0f);
// 	int PointNum = Spline->GetNumberOfSplinePoints();
// 	
// 	for (int i=0; i<PointNum; ++i)
// 	{
// 		FVector CurPos = Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Type::Local);
// 		
// 		OutSplineIndexes.Add(i);
// 		OutSplinePoints.Add(CurPos);
// 		OutSplineCenter += CurPos;
//
// 		int PrevIndex;
// 		int NextIndex;
// 		int NeighbourCount;
// 		GetSplinePointNeighbours(Spline, i, 0, PrevIndex, NextIndex, NeighbourCount);
//
// 		if (NextIndex >= 0 && InterpolateNum >= 1)
// 		{
// 			float SegTime;
// 			float CurTime;
// 			if (Spline->IsClosedLoop())
// 			{
// 				SegTime = 1.0f / float(PointNum);
// 				CurTime = float(i) / float(PointNum);
// 			}else
// 			{
// 				SegTime = 1.0f / float(PointNum-1);
// 				CurTime = float(i) / float(PointNum-1);
// 			}
//
// 			float InterpSegTime = SegTime / float(InterpolateNum+1);
//
// 			for (int j=1; j<InterpolateNum+1; ++j)
// 			{
// 				float CurInterpTime = float(j) * InterpSegTime + CurTime;
// 				FVector CurInterpPos = Spline->GetLocationAtTime(CurInterpTime, ESplineCoordinateSpace::Type::Local, false);
// 				OutSplinePoints.Add(CurInterpPos);
// 			}
// 		}
// 	}
//
// 	OutSplineCenter /= PointNum;
// }







