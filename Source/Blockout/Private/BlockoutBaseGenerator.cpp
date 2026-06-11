#include "BlockoutBaseGenerator.h"

#include "BlockoutLog.h"
#include "Functions/BlockoutLibrary_BasicFunctions.h"
#include "Functions/BlockoutLibrary_GeometryFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshRepairFunctions.h"

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

	UGeometryScriptLibrary_MeshRepairFunctions::CompactMesh(Mesh);
	UBlockoutLibrary_GeometryFunctions::CleanDegenerateTris(Mesh, 1e-4f, bShowDebugLog, true);

	FGeometryScriptSplitNormalsOptions SplitOptions;
	SplitOptions.OpeningAngleDeg = bSmoothNormal ? 40.0f : 10.0f;
	UGeometryScriptLibrary_MeshNormalsFunctions::ComputeSplitNormals(Mesh, SplitOptions, FGeometryScriptCalculateNormalsOptions());
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
