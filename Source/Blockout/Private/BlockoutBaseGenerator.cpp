#include "BlockoutBaseGenerator.h"

#include "BlockoutLog.h"
#include "Functions/BlockoutLibrary_BasicFunctions.h"
#include "Functions/BlockoutLibrary_GeometryFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshQueryFunctions.h"
#include "GeometryScript/MeshRepairFunctions.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

ABlockoutBaseGenerator::ABlockoutBaseGenerator()
{
}

void ABlockoutBaseGenerator::MeshOptimization()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("%s::MeshOptimization"), *GetName()));

	UDynamicMesh* Mesh = DynamicMeshComponent->GetDynamicMesh();
	if (!UBlockoutLibrary_BasicFunctions::IsDynamicMeshValid(Mesh))
	{
		return;
	}

	if (bShowDebugLog)
	{
		UE_LOG(LogBlockout, Warning, TEXT("------------------------MeshOptimization------------------------"));
	}

	UGeometryScriptLibrary_MeshRepairFunctions::CompactMesh(Mesh);
	UBlockoutLibrary_GeometryFunctions::CleanDegenerateTris(Mesh, 1e-4f, bShowDebugLog, true);

	FGeometryScriptSplitNormalsOptions SplitOptions;
	SplitOptions.OpeningAngleDeg = bSmoothNormal ? 40.0f : 10.0f;
	UGeometryScriptLibrary_MeshNormalsFunctions::ComputeSplitNormals(Mesh, SplitOptions, FGeometryScriptCalculateNormalsOptions());
}

void ABlockoutBaseGenerator::GenerateBlockoutMesh()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("%s::GenerateBlockoutMesh"), *GetName()));

	CPPGenerateBlockoutMesh();

	FEditorScriptExecutionGuard Guard;
	OnRebuildGeneratedMesh(DynamicMeshComponent->GetDynamicMesh());

	if (bShowDebugLog)
	{
		UDynamicMesh* Mesh = DynamicMeshComponent->GetDynamicMesh();
		UE_LOG(LogBlockout, Warning, TEXT("----------------------AfterCreateBlockoutMesh-----------------------"));
		UE_LOG(LogBlockout, Warning, TEXT("AfterCreate VertexCount: %d"), UGeometryScriptLibrary_MeshQueryFunctions::GetVertexCount(Mesh));
		UE_LOG(LogBlockout, Warning, TEXT("AfterCreate NumVertexIDs: %d"), UGeometryScriptLibrary_MeshQueryFunctions::GetNumVertexIDs(Mesh));
		UE_LOG(LogBlockout, Warning, TEXT("AfterCreate IsDenseMesh: %s"), UGeometryScriptLibrary_MeshQueryFunctions::GetIsDenseMesh(Mesh) ? TEXT("true") : TEXT("false"));
	}
}

void ABlockoutBaseGenerator::CPPGenerateBlockoutMesh()
{
}

void ABlockoutBaseGenerator::CreateBlockoutMesh()
{
	GenerateBlockoutMesh();
	MeshOptimization();
}
