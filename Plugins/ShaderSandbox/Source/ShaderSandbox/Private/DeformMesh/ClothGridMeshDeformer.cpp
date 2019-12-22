#include "ClothGridMeshDeformer.h"
#include "GlobalShader.h"
#include "RHIResources.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"

class FClothSimulationIntegrationCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClothSimulationIntegrationCS);
	SHADER_USE_PARAMETER_STRUCT(FClothSimulationIntegrationCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, NumVertex)
		SHADER_PARAMETER(float, SquareDeltaTime)
		SHADER_PARAMETER(float, Damping)
		SHADER_PARAMETER(FVector, PreviousInertia)
		SHADER_PARAMETER_SRV(Buffer<float>, InAccelerationVertexBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float>, OutPrevPositionVertexBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float>, OutPositionVertexBuffer)
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
		SHADER_PARAMETER(uint32, NumRow)
		SHADER_PARAMETER(uint32, NumColumn)
		SHADER_PARAMETER(uint32, NumVertex)
		SHADER_PARAMETER(float, GridWidth)
		SHADER_PARAMETER(float, GridHeight)
		SHADER_PARAMETER(float, Stiffness)
		SHADER_PARAMETER_UAV(RWBuffer<float>, OutPositionVertexBuffer)
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
	static const uint32 MAX_SPHERE_COLLISION = 16;

	DECLARE_GLOBAL_SHADER(FClothSimulationSolveCollisionCS);
	SHADER_USE_PARAMETER_STRUCT(FClothSimulationSolveCollisionCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, NumVertex)
		SHADER_PARAMETER(uint32, NumSphereCollision)
		SHADER_PARAMETER(float, VertexRadius)
		SHADER_PARAMETER_ARRAY(FVector4, SphereCenterAndRadiusArray, [MAX_SPHERE_COLLISION])
		SHADER_PARAMETER_UAV(RWBuffer<float>, OutPositionVertexBuffer)
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

void ClothSimulationGridMesh(FRHICommandListImmediate& RHICmdList, const FGridClothParameters& GridClothParams, class FRHIUnorderedAccessView* PositionVertexBufferUAV, class FRHIUnorderedAccessView* TangentVertexBufferUAV, class FRHIUnorderedAccessView* PrevPositionVertexBufferUAV, class FRHIShaderResourceView* AccelerationVertexBufferSRV)
{
	FRDGBuilder GraphBuilder(RHICmdList);

	TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);

	TShaderMapRef<FClothSimulationIntegrationCS> ClothSimIntegrationCS(ShaderMap);
	TShaderMapRef<FClothSimulationSolveDistanceConstraintCS> ClothSimSolveDistanceConstraintCS(ShaderMap);
	TShaderMapRef<FClothSimulationSolveCollisionCS> ClothSimSolveCollisionCS(ShaderMap);

	float DeltaTimePerIterate = GridClothParams.DeltaTime / GridClothParams.NumIteration;
	float SquareDeltaTime = DeltaTimePerIterate * DeltaTimePerIterate;

	// TODO:Stiffness、Dampingの効果がNumIterationやフレームレートに依存してしまっているのでどうにかせねば

	for (uint32 IterCount = 0; IterCount < GridClothParams.NumIteration; IterCount++)
	{
		FClothSimulationIntegrationCS::FParameters* ClothSimIntegrateParams = GraphBuilder.AllocParameters<FClothSimulationIntegrationCS::FParameters>();
		ClothSimIntegrateParams->NumVertex = GridClothParams.NumVertex;
		ClothSimIntegrateParams->SquareDeltaTime = SquareDeltaTime;
		ClothSimIntegrateParams->Damping = GridClothParams.Damping;
		ClothSimIntegrateParams->PreviousInertia = GridClothParams.PreviousInertia;
		ClothSimIntegrateParams->InAccelerationVertexBuffer = AccelerationVertexBufferSRV;
		ClothSimIntegrateParams->OutPrevPositionVertexBuffer = PrevPositionVertexBufferUAV;
		ClothSimIntegrateParams->OutPositionVertexBuffer = PositionVertexBufferUAV;

		//TODO: とりあえず今はこの関数呼び出しがメッシュ一個なので1 Dispatch
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ClothSimulationIntegration"),
			*ClothSimIntegrationCS,
			ClothSimIntegrateParams,
			FIntVector(1, 1, 1)
		);

		FClothSimulationSolveDistanceConstraintCS::FParameters* ClothSimDistanceConstraintParams = GraphBuilder.AllocParameters<FClothSimulationSolveDistanceConstraintCS::FParameters>();
		ClothSimDistanceConstraintParams->NumRow = GridClothParams.NumRow;
		ClothSimDistanceConstraintParams->NumColumn = GridClothParams.NumColumn;
		ClothSimDistanceConstraintParams->NumVertex = GridClothParams.NumVertex;
		ClothSimDistanceConstraintParams->GridWidth = GridClothParams.GridWidth;
		ClothSimDistanceConstraintParams->GridHeight = GridClothParams.GridHeight;
		ClothSimDistanceConstraintParams->Stiffness = GridClothParams.Stiffness;
		ClothSimDistanceConstraintParams->OutPositionVertexBuffer = PositionVertexBufferUAV;

		//TODO: とりあえず今はこの関数呼び出しがメッシュ一個なので1 Dispatch
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ClothSimulationSolveDistanceConstraint"),
			*ClothSimSolveDistanceConstraintCS,
			ClothSimDistanceConstraintParams,
			FIntVector(1, 1, 1)
		);

		FClothSimulationSolveCollisionCS::FParameters* ClothSimCollisionParams = GraphBuilder.AllocParameters<FClothSimulationSolveCollisionCS::FParameters>();
		ClothSimCollisionParams->NumVertex = GridClothParams.NumVertex;
		ClothSimCollisionParams->VertexRadius = GridClothParams.VertexRadius;
		ClothSimCollisionParams->NumSphereCollision = GridClothParams.SphereCollisionParams.Num();
		for (uint32 i = 0; i < FClothSimulationSolveCollisionCS::MAX_SPHERE_COLLISION; i++)
		{
			if (i < ClothSimCollisionParams->NumSphereCollision)
			{
				ClothSimCollisionParams->SphereCenterAndRadiusArray[i] = FVector4(GridClothParams.SphereCollisionParams[i].RelativeCenter, GridClothParams.SphereCollisionParams[i].Radius);
			}
			else
			{
				ClothSimCollisionParams->SphereCenterAndRadiusArray[i] = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
			}
		}
		ClothSimCollisionParams->OutPositionVertexBuffer = PositionVertexBufferUAV;

		//TODO: とりあえず今はこの関数呼び出しがメッシュ一個なので1 Dispatch
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ClothSimulationSolveCollision"),
			*ClothSimSolveCollisionCS,
			ClothSimCollisionParams,
			FIntVector(1, 1, 1)
		);
	}


	TShaderMapRef<FClothGridMeshTangentCS> GridMeshTangentCS(ShaderMap);

	FClothGridMeshTangentCS::FParameters* GridMeshTangentParams = GraphBuilder.AllocParameters<FClothGridMeshTangentCS::FParameters>();
	GridMeshTangentParams->NumRow = GridClothParams.NumRow;
	GridMeshTangentParams->NumColumn = GridClothParams.NumColumn;
	GridMeshTangentParams->NumVertex = GridClothParams.NumVertex;
	GridMeshTangentParams->InPositionVertexBuffer = PositionVertexBufferUAV;
	GridMeshTangentParams->OutTangentVertexBuffer = TangentVertexBufferUAV;

	const uint32 DispatchCount = FMath::DivideAndRoundUp(GridClothParams.NumVertex, (uint32)32);
	check(DispatchCount <= 65535);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("GridMeshTangent"),
		*GridMeshTangentCS,
		GridMeshTangentParams,
		FIntVector(DispatchCount, 1, 1)
	);


	GraphBuilder.Execute();
}

