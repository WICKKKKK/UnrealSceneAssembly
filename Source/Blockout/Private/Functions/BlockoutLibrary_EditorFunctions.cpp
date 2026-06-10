#include "Functions/BlockoutLibrary_EditorFunctions.h"
#include "BlockoutLog.h"
#include "DrawDebugHelpers.h"
#include "BlockoutInstancer.h"
#include "Components/SplineMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EditorLevelUtils.h"
#include "EditorUtilityLibrary.h"
#include "EngineUtils.h"
#include "Engine/Selection.h"

#if ENGINE_MAJOR_VERSION >= 5
#include "PhysicsEngine/BodySetup.h"
#endif

ULevel* UBlockoutLibrary_EditorFunctions::GetLevelFromPackagePath(const FString& PackagePath, bool& bOutSuccess)
{
	bOutSuccess = false;

	if (PackagePath.IsEmpty())
	{
		UE_LOG(LogBlockout, Warning, TEXT("Get Level From Package Path Failed! PackagePath is empty."));
		return nullptr;
	}

	// 尝试加载 UWorld
	UWorld* LoadedWorld = LoadObject<UWorld>(nullptr, *PackagePath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!IsValid(LoadedWorld))
	{
		UE_LOG(LogBlockout, Warning, TEXT("Get Level From Package Path Failed! Failed to load World from path: '%s'"), *PackagePath);
		return nullptr;
	}

	// 从 UWorld 获取 PersistentLevel
	ULevel* Level = LoadedWorld->PersistentLevel;
	if (!IsValid(Level))
	{
		UE_LOG(LogBlockout, Warning, TEXT("Get Level From Package Path Failed! PersistentLevel is not valid for path: '%s'"), *PackagePath);
		return nullptr;
	}

	bOutSuccess = true;
	return Level;
}


ULevel* UBlockoutLibrary_EditorFunctions::GetSublevelFromWorld(UWorld* World, FString SublevelName, bool& bOutSuccess)
{
	if (World == nullptr)
	{
		bOutSuccess = false;
		UE_LOG(LogBlockout, Warning, TEXT("Get Sublevel From World Failed! World is not valid. '%s'"), *SublevelName);
		return nullptr;
	}

	// Find level
	ULevel* Sublevel = nullptr;
	for (ULevel* Level : World->GetLevels())
	{
		if (Level->GetOuter()->GetName() == SublevelName)
		{
			Sublevel = Level;
			break;
		}
	}

	if (Sublevel == nullptr)
	{
		bOutSuccess = false;
		UE_LOG(LogBlockout, Warning, TEXT("Get Sublevel From World Failed! Sublevel '%s' not found."), *SublevelName);
		return nullptr;
	}

	bOutSuccess = true;
	// UE_LOG(LogBlockout, Warning, TEXT("Get Sublevel From World Success! Sublevel '%s' found."), *SublevelName);
	return Sublevel;
}

bool UBlockoutLibrary_EditorFunctions::GetLevelVisibility(ULevel* Level)
{
	if (!IsValid(Level))
	{
		UE_LOG(LogBlockout, Warning, TEXT("Get Level Visibility Failed! Level is not valid."));
		return false;
	}

	return Level->bIsVisible;
}

ULevelStreaming* UBlockoutLibrary_EditorFunctions::GetStreaminglevelByActor(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		UE_LOG(LogBlockout, Warning, TEXT("Get Sublevel By Actor Failed! Actor is not valid."));
		return nullptr;
	}
	ULevelStreaming* StreamingLevel = nullptr;
	
	UWorld* World = Actor->GetWorld();
	FString PackageName = FStreamLevelAction::MakeSafeLevelName(FName(Actor->GetPackage()->GetName()), World);
	if (FPackageName::IsShortPackageName(PackageName))
	{
		// Make sure MyMap1 and Map1 names do not resolve to a same streaming level
		PackageName = TEXT("/") + PackageName;
	}

	UE_LOG(LogBlockout, Warning, TEXT("Actor Package Name: %s"), *PackageName);

	for (ULevelStreaming* LevelStreaming : World->GetStreamingLevels())
	{
		// We check only suffix of package name, to handle situations when packages were saved for play into a temporary folder
		// Like Saved/Autosaves/PackageName
		if (LevelStreaming && LevelStreaming->GetWorldAssetPackageName().EndsWith(PackageName, ESearchCase::IgnoreCase))
		{
			StreamingLevel = LevelStreaming;
			break;
		}
	}

	// if (!IsValid(StreamingLevel))
	// {
	// 	UE_LOG(LogBlockout, Warning, TEXT("Get Sublevel By Actor Failed! Actor Package Name: %s"), *PackageName);
	// }

	return StreamingLevel;
}


void UBlockoutLibrary_EditorFunctions::SelectTargetActors(TArray<AActor*> Actors)
{
	GEditor->GetSelectedActors()->BeginBatchSelectOperation();
	for (AActor* Actor : Actors)
	{
		if (IsValid(Actor))
		{
			GEditor->SelectActor(Actor, true, false);
		}
	}
	GEditor->GetSelectedActors()->EndBatchSelectOperation(false/*bNotify*/);
	// 更新编辑器以反映新的选中状态
	GEditor->NoteSelectionChange();
}


TArray<AActor*> UBlockoutLibrary_EditorFunctions::GetOrSelectAllActorsByClass(UClass* TargetClass, bool bAddToSelection)
{
	TArray<AActor*> AllActorsOfTargetClass = TArray<AActor*>();
	// 获取当前编辑的世界
	UWorld* World = GEditor->GetEditorWorldContext().World();
	// 确保世界存在
	if (!World)
		return AllActorsOfTargetClass;
	
	if (bAddToSelection)
		GEditor->SelectNone(true, true, false);
	
	GEditor->GetSelectedActors()->BeginBatchSelectOperation();
	// 遍历世界中的所有Actor
	for (TActorIterator<AActor> It(World, TargetClass); It; ++It)
	{
		AActor* Actor = *It;
		AllActorsOfTargetClass.Add(Actor);
		if (bAddToSelection)
			GEditor->SelectActor(Actor, true, false);
	}
	GEditor->GetSelectedActors()->EndBatchSelectOperation(false/*bNotify*/);
	// 更新编辑器以反映新的选中状态
	if (bAddToSelection)
		GEditor->NoteSelectionChange();

	return AllActorsOfTargetClass;
}


TArray<AActor*> UBlockoutLibrary_EditorFunctions::GetOrSelectAllActorsByTag(const FName& Tag, bool bAddToSelection)
{
	TArray<AActor*> AllActorsWithTargetTag = TArray<AActor*>();
	// 获取当前编辑的世界
	UWorld* World = GEditor->GetEditorWorldContext().World();
	// 确保世界存在
	if (!World)
		return AllActorsWithTargetTag;
	
	if (bAddToSelection)
		GEditor->SelectNone(true, true, false);

	GEditor->GetSelectedActors()->BeginBatchSelectOperation();
	// 遍历世界中的所有Actor
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;

		// 检查Actor是否有指定的标签
		if (Actor->Tags.Contains(Tag))
		{
			AllActorsWithTargetTag.Add(Actor);
			if (bAddToSelection)
				GEditor->SelectActor(Actor, true, false);
		}
	}
	GEditor->GetSelectedActors()->EndBatchSelectOperation(false/*bNotify*/);
	// 更新编辑器以反映新的选中状态
	if (bAddToSelection)
		GEditor->NoteSelectionChange();

	return AllActorsWithTargetTag;
}


AActor* UBlockoutLibrary_EditorFunctions::SpawnActorInSublevel(TSubclassOf<AActor> ActorClass, ULevel* Sublevel,
	FTransform InTransform, bool& bOutSuccess)
{
	// Make sure the actor class is valid
	if (!IsValid(ActorClass))
	{
		bOutSuccess = false;
		UE_LOG(LogBlockout, Warning, TEXT("Spawn Actor In Sublevel Failed! Actor Class is not valid."));
		return nullptr;
	}

	if (!IsValid(Sublevel))
	{
		bOutSuccess = false;
		UE_LOG(LogBlockout, Warning, TEXT("Spawn Actor In Sublevel Failed! Sublevel is not valid."));
		return nullptr;
	}

	// Spawn actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.OverrideLevel = Sublevel;

	if (!IsValid(Sublevel->OwningWorld))
	{
		bOutSuccess = false;
		UE_LOG(LogBlockout, Warning, TEXT("Spawn Actor In Sublevel Failed! Sublevel's World is not valid."));
		return nullptr;
	}
	
	AActor* SpawnedActor = Sublevel->OwningWorld->SpawnActor<AActor>(ActorClass, InTransform, SpawnParams);

	bOutSuccess = true;
	// UE_LOG(LogBlockout, Warning, TEXT("Spawn Actor In Sublevel Success! Actor '%s' spawned in Sublevel '%s'."), *ActorClass->GetName(), *Sublevel->GetName());
	return SpawnedActor;
}

void UBlockoutLibrary_EditorFunctions::MoveActorsToSublevel(TArray<AActor*> Actors, ULevel* Sublevel, bool& bOutSuccess)
{
	//Remove invalid actors
	Actors.Remove(nullptr);
	if (Actors.Num() == 0)
	{
		bOutSuccess = false;
		UE_LOG(LogBlockout, Warning, TEXT("Move Or Copy Actors To Sublevel Failed! No Valid Actors."));
		return;
	}

	if (Sublevel == nullptr)
	{
		bOutSuccess = false;
		UE_LOG(LogBlockout, Warning, TEXT("Move Or Copy Actor To Sublevel Failed! Sublevel is not valid. '%s'"), *Sublevel->GetName());
		return;
	}
	
	int NumProcessedActors = 0;
	NumProcessedActors = UEditorLevelUtils::MoveActorsToLevel(Actors, Sublevel);

	bOutSuccess = NumProcessedActors == Actors.Num();
	// UE_LOG(LogBlockout, Warning, TEXT("Move Or Copy Actors To Sublevel Success! %d Actors Moved To Sublevel '%s'."), NumProcessedActors, *Sublevel->GetName());
}

AActor* UBlockoutLibrary_EditorFunctions::DuplicateActor(AActor* InActor)
{
	AActor* SpawnedActor = nullptr;
	
	if (InActor)
	{
		UWorld* World = InActor->GetWorld();
		if (World)
		{
			// DuplicateObject 要求提供一个新对象的父对象，这里使用当前世界
			AActor* DuplicatedActor = Cast<AActor>(StaticDuplicateObject(InActor, World, NAME_None, RF_NoFlags, InActor->GetClass()));

			if (DuplicatedActor)
			{
				// 确保复制的Actor被正确地添加到世界中
				FActorSpawnParameters SpawnParams;
				SpawnParams.Template = DuplicatedActor;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

				// 设置位置和旋转
				FTransform Transform = InActor->GetActorTransform();

				// 在世界中生成新的Actor
				SpawnedActor = World->SpawnActor<AActor>(DuplicatedActor->GetClass(), Transform, SpawnParams);
            
				// 这里可以添加其他初始化代码
			}
		}
	}

	return SpawnedActor;
}

bool UBlockoutLibrary_EditorFunctions::CheckPresetActorReferenced(AActor* PresetActor)
{
	if (!IsValid(PresetActor))
	{
		UE_LOG(LogBlockout, Warning, TEXT("PresetActor is nullptr!"));
		return false;
	}
	
	for (TActorIterator<AActor> It(PresetActor->GetWorld(), ABlockoutInstancer::StaticClass()); It; ++It)
	{
		if (PresetActor == Cast<ABlockoutInstancer>(*It)->GetPresetBlockoutActor())
		{
			return true;
		}
	}
	
	return false;
}

void UBlockoutLibrary_EditorFunctions::DestroyNoReferencedPresetActors()
{
	TArray<AActor*> FoundPresetActors = GetOrSelectAllActorsByTag(FName("BlockoutPresetActor"), false);
	for(AActor* Actor: FoundPresetActors)
	{
		if (!CheckPresetActorReferenced(Actor))
		{
			Actor->Destroy();
		}
	}
}

TArray<ABlockoutBaseDynamicMeshActor*> UBlockoutLibrary_EditorFunctions::GetSelectedBlockoutActors()
{
	TArray<ABlockoutBaseDynamicMeshActor*> SelectedBlockoutActors = TArray<ABlockoutBaseDynamicMeshActor*>();
	SelectedBlockoutActors.Reset();
	TArray<AActor*> SelectedActors = UEditorUtilityLibrary::GetSelectionSet();
	for (AActor* Actor : SelectedActors)
	{
		ABlockoutBaseDynamicMeshActor* BlockoutActor = Cast<ABlockoutBaseDynamicMeshActor>(Actor);
		if (IsValid(BlockoutActor))
		{
			SelectedBlockoutActors.Add(BlockoutActor);
		}
	}

	return SelectedBlockoutActors;
}


void UBlockoutLibrary_EditorFunctions::SetupStaticMeshComponent(
	UStaticMeshComponent*& OutMeshComp,
	UStaticMesh* InMesh,
	bool bEnableCollision,
	ECollisionTraceFlag CollisionTraceFlag
	)
{
	if (!IsValid(InMesh))
	{
		UE_LOG(LogBlockout, Warning, TEXT("InMesh is nullptr!"));
		return;
	}
	
	OutMeshComp->SetStaticMesh(InMesh);

	if (bEnableCollision)
	{
		OutMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		OutMeshComp->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
		OutMeshComp->SetCollisionResponseToAllChannels(ECR_Block);  // 设置所有通道为阻挡
		// 需要用简单碰撞,因为 SplineMeshComponent 无法使用复杂碰撞, 所以需要白盒 Mesh 资产配置好正确的简单碰撞
#if ENGINE_MAJOR_VERSION >= 5
		OutMeshComp->GetStaticMesh()->GetBodySetup()->CollisionTraceFlag = CollisionTraceFlag;
#else
		OutMeshComp->GetStaticMesh()->BodySetup->CollisionTraceFlag = CollisionTraceFlag;
#endif
		// MeshComp->SetCollisionProfileName(FName("BlockAll"), true);
	}else
	{
		OutMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	OutMeshComp->RecreatePhysicsState();
}



UStaticMeshComponent* UBlockoutLibrary_EditorFunctions::CreateStaticMeshComponetByClass(AActor* Actor,
	UStaticMesh* InMesh, const FTransform& InTransform, TSubclassOf<UStaticMeshComponent> Class,
	bool bEnableCollision, ECollisionTraceFlag CollisionTraceFlag)
{
	if (!IsValid(Actor))
	{
		UE_LOG(LogBlockout, Warning, TEXT("Actor is not valid!"));
		return nullptr;
	}
	if (!IsValid(InMesh))
	{
		UE_LOG(LogBlockout, Warning, TEXT("InMesh is nullptr! Current Actor: %s"), *Actor->GetName());
		return nullptr;
	}
	if (!IsValid(Class))
	{
		UE_LOG(LogBlockout, Warning, TEXT("Class is nullptr! Current Actor: %s"), *Actor->GetName());
		return nullptr;
	}

	UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(Actor->AddComponentByClass(Class , false, InTransform, false));
	if (!IsValid(MeshComp)) return nullptr;

	SetupStaticMeshComponent(MeshComp, InMesh, bEnableCollision, CollisionTraceFlag);

	return MeshComp;
}


void UBlockoutLibrary_EditorFunctions::CopyToStaticMeshComponent(UStaticMeshComponent* InMeshComp, UStaticMeshComponent* OutMeshComp)
{
	if (!IsValid(InMeshComp))
	{
		UE_LOG(LogBlockout, Warning, TEXT("InMeshComp is nullptr!"));
		return;
	}
	if (!IsValid(OutMeshComp))
	{
		UE_LOG(LogBlockout, Warning, TEXT("OutMeshComp is nullptr!"));
		return;
	}

	UStaticMesh* Mesh = InMeshComp->GetStaticMesh();
	if (!IsValid(Mesh))
	{
		UE_LOG(LogBlockout, Warning, TEXT("Mesh is nullptr!"));
		return;
	}
	bool bEnableCollision = InMeshComp->BodyInstance.GetCollisionEnabled(false) != ECollisionEnabled::NoCollision;
#if ENGINE_MAJOR_VERSION >= 5
	ECollisionTraceFlag CollisionTraceFlag = Mesh->GetBodySetup()->CollisionTraceFlag;
#else
	ECollisionTraceFlag CollisionTraceFlag = Mesh->BodySetup->CollisionTraceFlag;
#endif
	FTransform Transform = InMeshComp->GetComponentTransform();
	bool bVisible = InMeshComp->GetVisibleFlag();
	bool bCastShadow = InMeshComp->CastShadow;
	TArray<FName> CompTags = InMeshComp->ComponentTags;

	SetupStaticMeshComponent(OutMeshComp, Mesh, bEnableCollision, CollisionTraceFlag);

	// 拷贝材质
	const int32 MaterialCount = InMeshComp->GetNumMaterials();
	for(int32 i = 0; i < MaterialCount; ++i)
	{
		UMaterialInterface* Material = InMeshComp->GetMaterial(i);
		OutMeshComp->SetMaterial(i, Material);
	}

	OutMeshComp->SetRelativeTransform(Transform);
	OutMeshComp->SetVisibility(bVisible);
	OutMeshComp->SetCastShadow(bCastShadow);
	OutMeshComp->ComponentTags = CompTags;

	USplineMeshComponent* OutSplineMeshComp = Cast<USplineMeshComponent>(OutMeshComp);
	
	if (IsValid(OutSplineMeshComp))
	{
		USplineMeshComponent* InSplineMeshComp = Cast<USplineMeshComponent>(InMeshComp);
		if (IsValid(InSplineMeshComp))
		{
			OutSplineMeshComp->SetStartAndEnd(InSplineMeshComp->GetStartPosition(), InSplineMeshComp->GetStartTangent(), InSplineMeshComp->GetEndPosition(), InSplineMeshComp->GetEndTangent(), false);
			OutSplineMeshComp->SetStartScale(InSplineMeshComp->GetStartScale(), false);
			OutSplineMeshComp->SetEndScale(InSplineMeshComp->GetEndScale(), false);
			OutSplineMeshComp->SetStartOffset(InSplineMeshComp->GetStartOffset(), false);
			OutSplineMeshComp->SetEndOffset(InSplineMeshComp->GetEndOffset(), true);
		}
	}
}


UStaticMeshComponent* UBlockoutLibrary_EditorFunctions::CopyAndCreateStaticMeshComponet(AActor* Actor, UStaticMeshComponent* InMeshComp, FTransform InTransform)
{
	if (!IsValid(Actor))
	{
		UE_LOG(LogBlockout, Warning, TEXT("In Actor is not valid!"));
		return nullptr;
	}
	if (!IsValid(InMeshComp))
	{
		UE_LOG(LogBlockout, Warning, TEXT("InMeshComp is nullptr! Current Actor: %s"), *Actor->GetName());
		return nullptr;
	}

	TSubclassOf<UStaticMeshComponent> NewMeshCompClass = UStaticMeshComponent::StaticClass();
	USplineMeshComponent* SplineMeshComp = Cast<USplineMeshComponent>(InMeshComp);
	if (IsValid(SplineMeshComp)) NewMeshCompClass = USplineMeshComponent::StaticClass();
	
	UStaticMesh* Mesh = InMeshComp->GetStaticMesh();
	if (!IsValid(Mesh))
	{
		UE_LOG(LogBlockout, Warning, TEXT("Mesh is nullptr! Current Actor: %s"), *Actor->GetName());
		return nullptr;
	}
	bool bEnableCollision = InMeshComp->BodyInstance.GetCollisionEnabled(false) != ECollisionEnabled::NoCollision;
#if ENGINE_MAJOR_VERSION >= 5
	ECollisionTraceFlag CollisionTraceFlag = Mesh->GetBodySetup()->CollisionTraceFlag;
#else
	ECollisionTraceFlag CollisionTraceFlag = Mesh->BodySetup->CollisionTraceFlag;
#endif

	UStaticMeshComponent* NewMeshComp = CreateStaticMeshComponetByClass(Actor, Mesh, InTransform, NewMeshCompClass, bEnableCollision, CollisionTraceFlag);

	bool bVisible = InMeshComp->GetVisibleFlag();
	bool bCastShadow = InMeshComp->CastShadow;
	TArray<FName> CompTags = InMeshComp->ComponentTags;

	NewMeshComp->SetVisibility(bVisible);
	NewMeshComp->SetCastShadow(bCastShadow);
	NewMeshComp->ComponentTags = CompTags;

	if (IsValid(SplineMeshComp))
	{
		USplineMeshComponent* NewSplineMeshComp = Cast<USplineMeshComponent>(NewMeshComp);
		NewSplineMeshComp->SetStartAndEnd(SplineMeshComp->GetStartPosition(), SplineMeshComp->GetStartTangent(), SplineMeshComp->GetEndPosition(), SplineMeshComp->GetEndTangent(), false);
		NewSplineMeshComp->SetStartScale(SplineMeshComp->GetStartScale(), false);
		NewSplineMeshComp->SetEndScale(SplineMeshComp->GetEndScale(), false);
		NewSplineMeshComp->SetStartOffset(SplineMeshComp->GetStartOffset(), false);
		NewSplineMeshComp->SetEndOffset(SplineMeshComp->GetEndOffset(), true);
		NewMeshComp = Cast<UStaticMeshComponent>(NewSplineMeshComp);
	}
	
	return NewMeshComp;
}

void UBlockoutLibrary_EditorFunctions::DrawDebugBlockoutFace(const FBlockoutFace& Face, const FColor& Color, float Duration,
	float Thickness)
{
	UWorld* EditorWorld = GEditor->GetEditorWorldContext(false).World();
	if (!IsValid(EditorWorld))
		return;
	
	DrawDebugLine(EditorWorld, Face.Origin - Face.XAxis - Face.YAxis,
		Face.Origin + Face.XAxis - Face.YAxis, Color, false, Duration, SDPG_World, Thickness);
	DrawDebugLine(EditorWorld, Face.Origin + Face.XAxis - Face.YAxis,
		Face.Origin + Face.XAxis + Face.YAxis, Color, false, Duration, SDPG_World, Thickness);
	DrawDebugLine(EditorWorld, Face.Origin + Face.XAxis + Face.YAxis,
		Face.Origin - Face.XAxis + Face.YAxis, Color, false, Duration, SDPG_World, Thickness);
	DrawDebugLine(EditorWorld, Face.Origin - Face.XAxis + Face.YAxis,
		Face.Origin - Face.XAxis - Face.YAxis, Color, false, Duration, SDPG_World, Thickness);

	DrawDebugDirectionalArrow(EditorWorld, Face.Origin, Face.Origin + Face.XAxis, Thickness, FColor::Red,
		false, Duration, SDPG_World, Thickness);
	DrawDebugDirectionalArrow(EditorWorld, Face.Origin, Face.Origin + Face.YAxis, Thickness, FColor::Green,
		false, Duration, SDPG_World, Thickness);
	DrawDebugDirectionalArrow(EditorWorld, Face.Origin, Face.Origin + Face.Normal * 20.0f, Thickness, FColor::Blue,
		false, Duration, SDPG_World, Thickness);
}

void UBlockoutLibrary_EditorFunctions::DrawDebugBlockoutBox(const FBox& Box, const FTransform& Transform, const FColor& Color, float Duration,
	bool bWireframe, float Thickness)
{
	UWorld* EditorWorld = GEditor->GetEditorWorldContext(false).World();
	if (!IsValid(EditorWorld))
		return;

	FVector BoxCenter = Transform.TransformPosition(Box.GetCenter());
	FVector BoxExtent = Box.GetExtent() * Transform.GetScale3D();
	FQuat BoxRotation = Transform.GetRotation();
	
	if (bWireframe)
	{
		DrawDebugBox(EditorWorld, BoxCenter, BoxExtent, BoxRotation, Color,
			false, Duration, SDPG_World, Thickness);
	}else
	{
		DrawDebugSolidBox(EditorWorld, BoxCenter, BoxExtent, BoxRotation,
			Color, false, Duration, SDPG_World);
	}
}

void UBlockoutLibrary_EditorFunctions::DrawDebugBlockoutArrow(const FVector& Start, const FVector& End,
	const FColor& Color, float Duration, float Size, float Thickness)
{
	UWorld* EditorWorld = GEditor->GetEditorWorldContext(false).World();
	if (!IsValid(EditorWorld))
		return;

	DrawDebugDirectionalArrow(EditorWorld, Start, End, Size, Color, false, Duration, SDPG_World, Thickness);
}

void UBlockoutLibrary_EditorFunctions::DrawDebugBlockoutPoint(const FVector& InPoint, const FColor& Color, float Duration, float Size)
{
	UWorld* EditorWorld = GEditor->GetEditorWorldContext(false).World();
	if (!IsValid(EditorWorld))
		return;
	
	DrawDebugPoint(EditorWorld, InPoint, Size, Color, false, Duration, SDPG_World);
}
