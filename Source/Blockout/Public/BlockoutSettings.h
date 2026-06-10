// BlockoutSettings.h
#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "BlockoutSettings.generated.h"


UCLASS(config = Blockout, defaultconfig, meta=(DisplayName="Blockout"))
class BLOCKOUT_API UBlockoutSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UBlockoutSettings();

	UPROPERTY(config, EditAnywhere,DisplayName="启用/禁用Mode", Category = "全局设置")
	bool bEnableEdMode = true;
	
	UPROPERTY(config, EditAnywhere,DisplayName="调试", Category = "全局设置")
	bool bEnableDebug = false;
	
	// 默认白盒材质路径
	UPROPERTY(Config, BlueprintReadOnly, EditAnywhere, Category="Materials", meta=(AllowedClasses="/Script/Engine.Material"))
	FSoftObjectPath DefaultBlockoutMaterialPath;

	// 半透明材质路径(布尔减法)
	UPROPERTY(Config, BlueprintReadOnly, EditAnywhere, Category="Materials", meta=(AllowedClasses="/Script/Engine.Material"))
	FSoftObjectPath DefaultSubtractiveMaterialPath;
	
	// 默认白盒导出路径
	UPROPERTY(Config, BlueprintReadOnly, EditAnywhere, Category="Export", meta=(LongPackageName))
	FDirectoryPath DefaultExportPath;

	// 默认白盒导出路径
	UPROPERTY(Config, BlueprintReadOnly, EditAnywhere, Category="编辑器", DisplayName="允许连续创建?")
	bool bContinuousCreation = false;
	
	// 添加获取单例的静态方法
	static const UBlockoutSettings* Get()
	{
		return GetDefault<UBlockoutSettings>();
	}
	
	static UBlockoutSettings* GetPtr()
	{
		return GetMutableDefault<UBlockoutSettings>();
	}

	UFUNCTION(BlueprintPure, Category="Blockout")
	static UBlockoutSettings* GetBlockoutSettings()
	{
		return GetMutableDefault<UBlockoutSettings>();
	}

	UFUNCTION(BlueprintPure, Category="Blockout")
	UMaterialInterface* GetBlockoutMaterial() const;

	UFUNCTION(BlueprintPure, Category="Blockout")
	UMaterialInterface* GetSubtractiveMaterial() const;
	
protected:
	virtual FName GetContainerName() const override      { return TEXT("Project"); }
	virtual FName GetCategoryName() const override       { return TEXT("UnrealSceneAssembly"); }
	virtual FName GetSectionName() const override        { return TEXT("Blockout"); }
};
