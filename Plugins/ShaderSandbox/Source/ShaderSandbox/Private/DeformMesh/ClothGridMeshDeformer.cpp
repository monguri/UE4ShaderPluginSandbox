#include "ClothGridMeshDeformer.h"
#include "GlobalShader.h"
#include "RHIResources.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"

// SHADER_PARAMETER_UAVのARRAY版だけないので自分で作る
#define SHADER_PARAMETER_UAV_ARRAY(ShaderType,MemberName, ArrayDecl) \
	INTERNAL_SHADER_PARAMETER_EXPLICIT(UBMT_UAV, TShaderResourceParameterTypeInfo<FRHIUnorderedAccessView* ArrayDecl>, FRHIUnorderedAccessView*,MemberName,ArrayDecl,,EShaderPrecisionModifier::Float,TEXT(#ShaderType),false)

class FClothSimulationIntegrationCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClothSimulationIntegrationCS);
	SHADER_USE_PARAMETER_STRUCT(FClothSimulationIntegrationCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, NumClothMesh)
		SHADER_PARAMETER_ARRAY(uint32, NumVertex, [FClothGridMeshDeformer::MAX_CLOTH_MESH])
		SHADER_PARAMETER(float, SquareDeltaTime)
		SHADER_PARAMETER_ARRAY(float, Damping, [FClothGridMeshDeformer::MAX_CLOTH_MESH])
		SHADER_PARAMETER_ARRAY(FVector, PreviousInertia, [FClothGridMeshDeformer::MAX_CLOTH_MESH])
		SHADER_PARAMETER_SRV_ARRAY(Buffer<float>, InAccelerationVertexBuffer, [FClothGridMeshDeformer::MAX_CLOTH_MESH])
		SHADER_PARAMETER_UAV_ARRAY(RWBuffer<float>, OutPrevPositionVertexBuffer, [FClothGridMeshDeformer::MAX_CLOTH_MESH])
		SHADER_PARAMETER_UAV_ARRAY(RWBuffer<float>, OutPositionVertexBuffer, [FClothGridMeshDeformer::MAX_CLOTH_MESH])
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FClothSimulationIntegrationCS, "/Plugin/ShaderSandbox/Private/ClothSimulationGridMesh.usf", "Interate", SF_Compute);

class FClothSimulationSolveDistanceConstraintCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClothSimulationSolveDistanceConstraintCS);
	SHADER_USE_PARAMETER_STRUCT(FClothSimulationSolveDistanceConstraintCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, NumClothMesh)
		SHADER_PARAMETER_ARRAY(uint32, NumRow, [FClothGridMeshDeformer::MAX_CLOTH_MESH])
		SHADER_PARAMETER_ARRAY(uint32, NumColumn, [FClothGridMeshDeformer::MAX_CLOTH_MESH])
		SHADER_PARAMETER_ARRAY(uint32, NumVertex, [FClothGridMeshDeformer::MAX_CLOTH_MESH])
		SHADER_PARAMETER_ARRAY(float, GridWidth, [FClothGridMeshDeformer::MAX_CLOTH_MESH])
		SHADER_PARAMETER_ARRAY(float, GridHeight, [FClothGridMeshDeformer::MAX_CLOTH_MESH])
		SHADER_PARAMETER_ARRAY(float, Stiffness, [FClothGridMeshDeformer::MAX_CLOTH_MESH])
		SHADER_PARAMETER_UAV_ARRAY(RWBuffer<float>, OutPositionVertexBuffer, [FClothGridMeshDeformer::MAX_CLOTH_MESH])
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FClothSimulationSolveDistanceConstraintCS, "/Plugin/ShaderSandbox/Private/ClothSimulationGridMesh.usf", "SolveDistanceConstraint", SF_Compute);

class FClothSimulationSolveCollisionCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FClothSimulationSolveCollisionCS);
	SHADER_USE_PARAMETER_STRUCT(FClothSimulationSolveCollisionCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, NumClothMesh)
		SHADER_PARAMETER_ARRAY(uint32, NumVertex, [FClothGridMeshDeformer::MAX_CLOTH_MESH])
		SHADER_PARAMETER(uint32, NumSphereCollision)
		SHADER_PARAMETER_ARRAY(float, VertexRadius, [FClothGridMeshDeformer::MAX_CLOTH_MESH])
		SHADER_PARAMETER_ARRAY(FVector4, SphereCenterAndRadiusArray, [FClothGridMeshDeformer::MAX_SPHERE_COLLISION])
		SHADER_PARAMETER_UAV_ARRAY(RWBuffer<float>, OutPositionVertexBuffer, [FClothGridMeshDeformer::MAX_CLOTH_MESH])
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FClothSimulationSolveCollisionCS, "/Plugin/ShaderSandbox/Private/ClothSimulationGridMesh.usf", "SolveCollision", SF_Compute);

class FClothGridMeshTangentCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClothGridMeshTangentCS);
	SHADER_USE_PARAMETER_STRUCT(FClothGridMeshTangentCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, NumRow)
		SHADER_PARAMETER(uint32, NumColumn)
		SHADER_PARAMETER(uint32, NumVertex)
		SHADER_PARAMETER_UAV(RWBuffer<float>, InPositionVertexBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float4>, OutTangentVertexBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FClothGridMeshTangentCS, "/Plugin/ShaderSandbox/Private/GridMeshTangent.usf", "MainCS", SF_Compute);

void FClothGridMeshDeformer::EnqueueDeformTask(const FGridClothParameters& Param)
{
	DeformTaskQueue.Add(Param);
}

void FClothGridMeshDeformer::FlushDeformTaskQueue(FRHICommandListImmediate& RHICmdList)
{
	if (DeformTaskQueue.Num() == 0)
	{
		return;
	}

	FRDGBuilder GraphBuilder(RHICmdList);

	TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);

	TShaderMapRef<FClothSimulationIntegrationCS> ClothSimIntegrationCS(ShaderMap);
	TShaderMapRef<FClothSimulationSolveDistanceConstraintCS> ClothSimSolveDistanceConstraintCS(ShaderMap);
	TShaderMapRef<FClothSimulationSolveCollisionCS> ClothSimSolveCollisionCS(ShaderMap);

	// TODO:Stiffness、Dampingの効果がNumIterationやフレームレートに依存してしまっているのでどうにかせねば

	float DeltaTimePerIterate = DeformTaskQueue[0].DeltaTime / DeformTaskQueue[0].NumIteration;
	float SquareDeltaTime = DeltaTimePerIterate * DeltaTimePerIterate;
	uint32 NumClothMesh = DeformTaskQueue.Num();

	// TODO:あとでイテレーションもCS内に入れる
	for (uint32 IterCount = 0; IterCount < DeformTaskQueue[0].NumIteration; IterCount++)
	{
		FClothSimulationIntegrationCS::FParameters* ClothSimIntegrateParams = GraphBuilder.AllocParameters<FClothSimulationIntegrationCS::FParameters>();

		ClothSimIntegrateParams->NumClothMesh = NumClothMesh;
		ClothSimIntegrateParams->SquareDeltaTime = SquareDeltaTime;
		for (uint32 MeshIdx = 0; MeshIdx < NumClothMesh; MeshIdx++)
		{
			const FGridClothParameters& GridClothParams = DeformTaskQueue[MeshIdx];
			ClothSimIntegrateParams->NumVertex[MeshIdx] = GridClothParams.NumVertex;
			ClothSimIntegrateParams->Damping[MeshIdx] = GridClothParams.Damping;
			ClothSimIntegrateParams->PreviousInertia[MeshIdx] = GridClothParams.PreviousInertia;
			ClothSimIntegrateParams->InAccelerationVertexBuffer[MeshIdx] = GridClothParams.AccelerationVertexBufferSRV;
			ClothSimIntegrateParams->OutPrevPositionVertexBuffer[MeshIdx] = GridClothParams.PrevPositionVertexBufferUAV;
			ClothSimIntegrateParams->OutPositionVertexBuffer[MeshIdx] = GridClothParams.PositionVertexBufferUAV;
		}

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ClothSimulationIntegration"),
			*ClothSimIntegrationCS,
			ClothSimIntegrateParams,
			FIntVector(NumClothMesh, 1, 1)
		);

		FClothSimulationSolveDistanceConstraintCS::FParameters* ClothSimDistanceConstraintParams = GraphBuilder.AllocParameters<FClothSimulationSolveDistanceConstraintCS::FParameters>();
		ClothSimDistanceConstraintParams->NumClothMesh = NumClothMesh;
		for (uint32 MeshIdx = 0; MeshIdx < NumClothMesh; MeshIdx++)
		{
			const FGridClothParameters& GridClothParams = DeformTaskQueue[MeshIdx];
			ClothSimDistanceConstraintParams->NumRow[MeshIdx] = GridClothParams.NumRow;
			ClothSimDistanceConstraintParams->NumColumn[MeshIdx] = GridClothParams.NumColumn;
			ClothSimDistanceConstraintParams->NumVertex[MeshIdx] = GridClothParams.NumVertex;
			ClothSimDistanceConstraintParams->GridWidth[MeshIdx] = GridClothParams.GridWidth;
			ClothSimDistanceConstraintParams->GridHeight[MeshIdx] = GridClothParams.GridHeight;
			ClothSimDistanceConstraintParams->Stiffness[MeshIdx] = GridClothParams.Stiffness;
			ClothSimDistanceConstraintParams->OutPositionVertexBuffer[MeshIdx] = GridClothParams.PositionVertexBufferUAV;
		}

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ClothSimulationSolveDistanceConstraint"),
			*ClothSimSolveDistanceConstraintCS,
			ClothSimDistanceConstraintParams,
			FIntVector(NumClothMesh, 1, 1)
		);

		FClothSimulationSolveCollisionCS::FParameters* ClothSimCollisionParams = GraphBuilder.AllocParameters<FClothSimulationSolveCollisionCS::FParameters>();
		ClothSimCollisionParams->NumClothMesh = NumClothMesh;
		for (uint32 MeshIdx = 0; MeshIdx < NumClothMesh; MeshIdx++)
		{
			const FGridClothParameters& GridClothParams = DeformTaskQueue[MeshIdx];
			ClothSimCollisionParams->NumVertex[MeshIdx] = GridClothParams.NumVertex;
			ClothSimCollisionParams->VertexRadius[MeshIdx] = GridClothParams.VertexRadius;

			ClothSimCollisionParams->OutPositionVertexBuffer[MeshIdx] = GridClothParams.PositionVertexBufferUAV;
		}

		ClothSimCollisionParams->NumSphereCollision = DeformTaskQueue[0].SphereCollisionParams.Num();
		for (uint32 i = 0; i < FClothGridMeshDeformer::MAX_SPHERE_COLLISION; i++)
		{
			if (i < ClothSimCollisionParams->NumSphereCollision)
			{
				ClothSimCollisionParams->SphereCenterAndRadiusArray[i] = FVector4(DeformTaskQueue[0].SphereCollisionParams[i].RelativeCenter, DeformTaskQueue[0].SphereCollisionParams[i].Radius);
			}
			else
			{
				ClothSimCollisionParams->SphereCenterAndRadiusArray[i] = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
			}
		}

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ClothSimulationSolveCollision"),
			*ClothSimSolveCollisionCS,
			ClothSimCollisionParams,
			FIntVector(NumClothMesh, 1, 1)
		);
	}


	TShaderMapRef<FClothGridMeshTangentCS> GridMeshTangentCS(ShaderMap);

	FClothGridMeshTangentCS::FParameters* GridMeshTangentParams = GraphBuilder.AllocParameters<FClothGridMeshTangentCS::FParameters>();

	for (uint32 MeshIdx = 0; MeshIdx < NumClothMesh; MeshIdx++)
	{
		const FGridClothParameters& GridClothParams = DeformTaskQueue[MeshIdx];

		GridMeshTangentParams->NumRow = GridClothParams.NumRow;
		GridMeshTangentParams->NumColumn = GridClothParams.NumColumn;
		GridMeshTangentParams->NumVertex = GridClothParams.NumVertex;
		GridMeshTangentParams->InPositionVertexBuffer = GridClothParams.PositionVertexBufferUAV;
		GridMeshTangentParams->OutTangentVertexBuffer = GridClothParams.TangentVertexBufferUAV;

		const uint32 DispatchCount = FMath::DivideAndRoundUp(GridClothParams.NumVertex, (uint32)32);
		check(DispatchCount <= 65535);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("GridMeshTangent"),
			*GridMeshTangentCS,
			GridMeshTangentParams,
			FIntVector(DispatchCount, 1, 1)
		);
	}

	GraphBuilder.Execute();

	DeformTaskQueue.Reset();
}

