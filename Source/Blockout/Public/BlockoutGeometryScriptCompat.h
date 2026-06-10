#pragma once

#include "Components/DynamicMeshComponent.h"
#include "GeometryScript/CollisionFunctions.h"
#include "GeometryScript/CreateNewAssetUtilityFunctions.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshDecompositionFunctions.h"
#include "GeometryScript/MeshMaterialFunctions.h"
#include "GeometryScript/MeshModelingFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshQueryFunctions.h"
#include "GeometryScript/MeshRepairFunctions.h"
#include "GeometryScript/MeshSubdivideFunctions.h"
#include "GeometryScript/MeshTransformFunctions.h"
#include "GeometryScript/SceneUtilityFunctions.h"
#include "PhysicsEngine/BodySetup.h"
#include "UDynamicMesh.h"

using UFalconDynamicMesh = UDynamicMesh;
using UFalconDynamicMeshComponent = UDynamicMeshComponent;
using UFalconDynamicMeshPool = UDynamicMeshPool;

using FFalconGeometryScriptPrimitiveOptions = FGeometryScriptPrimitiveOptions;
using FFalconGeometryScriptAppendMeshOptions = FGeometryScriptAppendMeshOptions;
using FFalconGeometryScriptMeshBooleanOptions = FGeometryScriptMeshBooleanOptions;
using FFalconGeometryScriptMeshSelfUnionOptions = FGeometryScriptMeshSelfUnionOptions;
using FFalconGeometryScriptCopyMeshFromComponentOptions = FGeometryScriptCopyMeshFromComponentOptions;
using FFalconGeometryScriptUniqueAssetNameOptions = FGeometryScriptUniqueAssetNameOptions;
using FFalconGeometryScriptCreateNewStaticMeshAssetOptions = FGeometryScriptCreateNewStaticMeshAssetOptions;
using FFalconGeometryScriptCopyMeshToAssetOptions = FGeometryScriptCopyMeshToAssetOptions;
using FFalconGeometryScriptMeshWriteLOD = FGeometryScriptMeshWriteLOD;
using FFalconGeometryScriptSetSimpleCollisionOptions = FGeometryScriptSetSimpleCollisionOptions;
using FFalconGeometryScriptIndexList = FGeometryScriptIndexList;
using FFalconGeometryScriptMeshExtrudeOptions = FGeometryScriptMeshExtrudeOptions;
using FFalconGeometryScriptSplitNormalsOptions = FGeometryScriptSplitNormalsOptions;
using FFalconGeometryScriptCalculateNormalsOptions = FGeometryScriptCalculateNormalsOptions;
using FFalconGeometryScriptPNTessellateOptions = FGeometryScriptPNTessellateOptions;

using EFalconGeometryScriptBooleanOperation = EGeometryScriptBooleanOperation;
using EFalconGeometryScriptOutcomePins = EGeometryScriptOutcomePins;
using EFalconGeometryScriptIndexType = EGeometryScriptIndexType;

struct FFalconGeometryScriptEdgeData
{
};

inline UBodySetup* FalconGetMeshBodySetup(UStaticMesh* StaticMesh)
{
	return IsValid(StaticMesh) ? StaticMesh->GetBodySetup() : nullptr;
}

class UFalconGeometryLibrary_MeshDecomposition
{
public:
	static UDynamicMesh* CopyMeshToMesh(UDynamicMesh* CopyFromMesh, UDynamicMesh* CopyToMesh, UDynamicMesh*& CopyToMeshOut)
	{
		return UGeometryScriptLibrary_MeshDecompositionFunctions::CopyMeshToMesh(CopyFromMesh, CopyToMesh, CopyToMeshOut);
	}
};

class UFalconGeometryLibrary_MeshTransform
{
public:
	static UDynamicMesh* TransformMesh(UDynamicMesh* TargetMesh, FTransform Transform)
	{
		return UGeometryScriptLibrary_MeshTransformFunctions::TransformMesh(TargetMesh, Transform);
	}

	static UDynamicMesh* TranslateMesh(UDynamicMesh* TargetMesh, FVector Translation)
	{
		return UGeometryScriptLibrary_MeshTransformFunctions::TranslateMesh(TargetMesh, Translation);
	}
};

class UFalconGeometryLibrary_MeshPrimitive
{
public:
	static UDynamicMesh* AppendBox(UDynamicMesh* TargetMesh, FGeometryScriptPrimitiveOptions PrimitiveOptions, FTransform Transform, float DimensionX, float DimensionY, float DimensionZ)
	{
		return UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(TargetMesh, PrimitiveOptions, Transform, DimensionX, DimensionY, DimensionZ);
	}

	static UDynamicMesh* AppendTriangulatedPolygon(UDynamicMesh* TargetMesh, FGeometryScriptPrimitiveOptions PrimitiveOptions, FTransform Transform, const TArray<FVector2D>& PolygonVertices)
	{
		return UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendTriangulatedPolygon(TargetMesh, PrimitiveOptions, Transform, PolygonVertices);
	}

	static UDynamicMesh* AppendRoundRectangle_Compatibility_5_0(UDynamicMesh* TargetMesh, FGeometryScriptPrimitiveOptions PrimitiveOptions, FTransform Transform, float DimensionX, float DimensionY, float CornerRadius, int32 StepsWidth, int32 StepsHeight, int32 StepsRound)
	{
		return UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendRoundRectangle_Compatibility_5_0(TargetMesh, PrimitiveOptions, Transform, DimensionX, DimensionY, CornerRadius, StepsWidth, StepsHeight, StepsRound);
	}
};

class UFalconGeometryLibrary_MeshBoolean
{
public:
	static UDynamicMesh* ApplyMeshBoolean(UDynamicMesh* TargetMesh, FTransform TargetTransform, UDynamicMesh* ToolMesh, FTransform ToolTransform, EGeometryScriptBooleanOperation Operation, FGeometryScriptMeshBooleanOptions Options, FFalconGeometryScriptEdgeData& Edges)
	{
		return UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(TargetMesh, TargetTransform, ToolMesh, ToolTransform, Operation, Options);
	}

	static UDynamicMesh* ApplyMeshSelfUnion(UDynamicMesh* TargetMesh, FGeometryScriptMeshSelfUnionOptions Options)
	{
		return UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshSelfUnion(TargetMesh, Options);
	}
};

class UFalconGeometryLibrary_SceneUtility
{
public:
	static UDynamicMesh* CopyMeshFromComponent(USceneComponent* Component, UDynamicMesh* ToDynamicMesh, FGeometryScriptCopyMeshFromComponentOptions Options, bool bTransformToWorld, FTransform& LocalToWorld, EGeometryScriptOutcomePins& Outcome)
	{
		return UGeometryScriptLibrary_SceneUtilityFunctions::CopyMeshFromComponent(Component, ToDynamicMesh, Options, bTransformToWorld, LocalToWorld, Outcome);
	}
};

class UFalconGeometryLibrary_MeshBasicEdit
{
public:
	static UDynamicMesh* AppendMesh(UDynamicMesh* TargetMesh, UDynamicMesh* AppendMesh, FTransform AppendTransform, FGeometryScriptAppendMeshOptions AppendOptions, bool bDeferChangeNotifications = false)
	{
		return UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(TargetMesh, AppendMesh, AppendTransform, bDeferChangeNotifications, AppendOptions);
	}

	static UDynamicMesh* AppendMeshRepeated(UDynamicMesh* TargetMesh, UDynamicMesh* AppendMesh, FTransform AppendTransform, FGeometryScriptAppendMeshOptions AppendOptions, int RepeatCount = 1, bool bApplyTransformToFirstInstance = true, bool bDeferChangeNotifications = false)
	{
		return UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMeshRepeated(TargetMesh, AppendMesh, AppendTransform, RepeatCount, bApplyTransformToFirstInstance, bDeferChangeNotifications, AppendOptions);
	}

	static UDynamicMesh* AddVertexToMesh(UDynamicMesh* TargetMesh, FVector NewPosition, int& NewVertexIndex, bool bDeferChangeNotifications = false)
	{
		return UGeometryScriptLibrary_MeshBasicEditFunctions::AddVertexToMesh(TargetMesh, NewPosition, NewVertexIndex, bDeferChangeNotifications);
	}

	static UDynamicMesh* AddTriangleToMesh(UDynamicMesh* TargetMesh, FIntVector NewTriangle, int& NewTriangleIndex, int NewTriangleGroupID = 0, bool bDeferChangeNotifications = false)
	{
		return UGeometryScriptLibrary_MeshBasicEditFunctions::AddTriangleToMesh(TargetMesh, NewTriangle, NewTriangleIndex, NewTriangleGroupID, bDeferChangeNotifications);
	}

	static UDynamicMesh* DeleteTrianglesFromMesh(UDynamicMesh* TargetMesh, FGeometryScriptIndexList TriangleList, int& NumDeleted, bool bDeferChangeNotifications = false)
	{
		return UGeometryScriptLibrary_MeshBasicEditFunctions::DeleteTrianglesFromMesh(TargetMesh, TriangleList, NumDeleted, bDeferChangeNotifications);
	}
};

class UFalconGeometryLibrary_MeshRepair
{
public:
	static UDynamicMesh* CompactMesh(UDynamicMesh* TargetMesh)
	{
		return UGeometryScriptLibrary_MeshRepairFunctions::CompactMesh(TargetMesh);
	}
};

class UFalconGeometryLibrary_MeshNormal
{
public:
	static UDynamicMesh* ComputeSplitNormals(UDynamicMesh* TargetMesh, FGeometryScriptSplitNormalsOptions SplitOptions, FGeometryScriptCalculateNormalsOptions CalculateOptions)
	{
		return UGeometryScriptLibrary_MeshNormalsFunctions::ComputeSplitNormals(TargetMesh, SplitOptions, CalculateOptions);
	}

	static UDynamicMesh* FlipNormals(UDynamicMesh* TargetMesh)
	{
		return UGeometryScriptLibrary_MeshNormalsFunctions::FlipNormals(TargetMesh);
	}
};

class UFalconGeometryLibrary_MeshQuery
{
public:
	static UDynamicMesh* GetAllTriangleIDs(UDynamicMesh* TargetMesh, FGeometryScriptIndexList& TriangleIDList, bool& bHasTriangleIDGaps)
	{
		return UGeometryScriptLibrary_MeshQueryFunctions::GetAllTriangleIDs(TargetMesh, TriangleIDList, bHasTriangleIDGaps);
	}

	static void GetTrianglePositions(UDynamicMesh* TargetMesh, int32 TriangleID, bool& bIsValidTriangle, FVector& Vertex1, FVector& Vertex2, FVector& Vertex3)
	{
		UGeometryScriptLibrary_MeshQueryFunctions::GetTrianglePositions(TargetMesh, TriangleID, bIsValidTriangle, Vertex1, Vertex2, Vertex3);
	}

	static FBox GetMeshBoundingBox(UDynamicMesh* TargetMesh)
	{
		return UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(TargetMesh);
	}

	static FVector GetTriangleFaceNormal(UDynamicMesh* TargetMesh, int32 TriangleID, bool& bIsValidTriangle)
	{
		return UGeometryScriptLibrary_MeshQueryFunctions::GetTriangleFaceNormal(TargetMesh, TriangleID, bIsValidTriangle);
	}

	static int32 GetVertexCount(UDynamicMesh* TargetMesh)
	{
		return UGeometryScriptLibrary_MeshQueryFunctions::GetVertexCount(TargetMesh);
	}

	static int32 GetNumVertexIDs(UDynamicMesh* TargetMesh)
	{
		return UGeometryScriptLibrary_MeshQueryFunctions::GetNumVertexIDs(TargetMesh);
	}

	static bool GetIsDenseMesh(UDynamicMesh* TargetMesh)
	{
		return UGeometryScriptLibrary_MeshQueryFunctions::GetIsDenseMesh(TargetMesh);
	}

	static UDynamicMesh* GetAllVertexIDs(UDynamicMesh* TargetMesh, FGeometryScriptIndexList& VertexIDList, bool& bHasVertexIDGaps)
	{
		return UGeometryScriptLibrary_MeshQueryFunctions::GetAllVertexIDs(TargetMesh, VertexIDList, bHasVertexIDGaps);
	}

	static FVector GetVertexPosition(UDynamicMesh* TargetMesh, int32 VertexID, bool& bIsValidVertex)
	{
		return UGeometryScriptLibrary_MeshQueryFunctions::GetVertexPosition(TargetMesh, VertexID, bIsValidVertex);
	}
};

class UFalconGeometryLibrary_MeshModeling
{
public:
	static UDynamicMesh* ApplyMeshExtrude_Compatibility_5p0(UDynamicMesh* TargetMesh, FGeometryScriptMeshExtrudeOptions Options)
	{
		return UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshExtrude_Compatibility_5p0(TargetMesh, Options);
	}
};

class UFalconGeometryLibrary_MeshSubdivide
{
public:
	static UDynamicMesh* ApplyPNTessellation(UDynamicMesh* TargetMesh, FGeometryScriptPNTessellateOptions Options, int TessellationLevel = 3)
	{
		return UGeometryScriptLibrary_MeshSubdivideFunctions::ApplyPNTessellation(TargetMesh, Options, TessellationLevel);
	}
};

class UFalconGeometryLibrary_MeshMaterial
{
public:
	static UDynamicMesh* EnableMaterialIDs(UDynamicMesh* TargetMesh)
	{
		return UGeometryScriptLibrary_MeshMaterialFunctions::EnableMaterialIDs(TargetMesh);
	}

	static UDynamicMesh* RemapMaterialIDs(UDynamicMesh* TargetMesh, int FromMaterialID, int ToMaterialID)
	{
		return UGeometryScriptLibrary_MeshMaterialFunctions::RemapMaterialIDs(TargetMesh, FromMaterialID, ToMaterialID);
	}
};

class UFalconGeometryLibrary_MeshConvert
{
public:
	static UDynamicMesh* CopyMeshToStaticMesh(UDynamicMesh* FromDynamicMesh, UStaticMesh* ToStaticMeshAsset, FGeometryScriptCopyMeshToAssetOptions Options, FGeometryScriptMeshWriteLOD TargetLOD, EGeometryScriptOutcomePins& Outcome)
	{
		return UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToStaticMesh(FromDynamicMesh, ToStaticMeshAsset, Options, TargetLOD, Outcome, true, nullptr);
	}
};

class UFalconGeometryLibrary_CreateNewAssetUtility
{
public:
	static void CreateUniqueNewAssetPathName(FString AssetFolderPath, FString BaseAssetName, FString& UniqueAssetPathAndName, FString& UniqueAssetName, FGeometryScriptUniqueAssetNameOptions Options, EGeometryScriptOutcomePins& Outcome)
	{
		UGeometryScriptLibrary_CreateNewAssetFunctions::CreateUniqueNewAssetPathName(AssetFolderPath, BaseAssetName, UniqueAssetPathAndName, UniqueAssetName, Options, Outcome);
	}

	static UStaticMesh* CreateNewStaticMeshAssetFromMesh(UDynamicMesh* FromDynamicMesh, UObject* UnusedOuter, FString AssetPathAndName, FGeometryScriptCreateNewStaticMeshAssetOptions Options, EGeometryScriptOutcomePins& Outcome)
	{
		return UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewStaticMeshAssetFromMesh(FromDynamicMesh, AssetPathAndName, Options, Outcome);
	}
};

class UFalconGeometryLibrary_MeshCollision
{
public:
	static void SetStaticMeshCollisionFromComponent(UStaticMesh* StaticMeshAsset, UPrimitiveComponent* SourceComponent, FGeometryScriptSetSimpleCollisionOptions Options)
	{
		UGeometryScriptLibrary_CollisionFunctions::SetStaticMeshCollisionFromComponent(StaticMeshAsset, SourceComponent, Options);
	}
};
