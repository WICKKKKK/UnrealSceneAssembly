#include "BlockoutBaseGenerator.h"

#include "BlockoutGeometryScriptCompat.h"
#include "BlockoutLog.h"
#include "Functions/BlockoutLibrary_BasicFunctions.h"
#include "Functions/BlockoutLibrary_GeometryFunctions.h"

ABlockoutBaseGenerator::ABlockoutBaseGenerator()
{
}

void ABlockoutBaseGenerator::MeshOptimization()
{
	UDynamicMesh* Mesh = DynamicMeshComponent->GetDynamicMesh();
	if (!UBlockoutLibrary_BasicFunctions::IsDynamicMeshValid(Mesh))
	{
		return;
	}

	UFalconGeometryLibrary_MeshRepair::CompactMesh(Mesh);
	UBlockoutLibrary_GeometryFunctions::CleanDegenerateTris(Mesh, 1e-4f, bShowDebugLog, true);

	FFalconGeometryScriptSplitNormalsOptions SplitOptions;
	SplitOptions.OpeningAngleDeg = bSmoothNormal ? 40.0f : 10.0f;
	UFalconGeometryLibrary_MeshNormal::ComputeSplitNormals(Mesh, SplitOptions, FFalconGeometryScriptCalculateNormalsOptions());
}

void ABlockoutBaseGenerator::GenerateBlockoutMesh()
{
	CPPGenerateBlockoutMesh();

	FEditorScriptExecutionGuard Guard;
	OnRebuildGeneratedMesh(DynamicMeshComponent->GetDynamicMesh());
}

void ABlockoutBaseGenerator::CPPGenerateBlockoutMesh()
{
}

void ABlockoutBaseGenerator::CreateBlockoutMesh()
{
	GenerateBlockoutMesh();
	MeshOptimization();
}
