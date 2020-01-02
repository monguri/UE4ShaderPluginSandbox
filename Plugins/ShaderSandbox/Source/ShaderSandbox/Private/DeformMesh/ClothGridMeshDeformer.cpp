#include "ClothGridMeshDeformer.h"
#include "GlobalShader.h"
#include "RHIResources.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"

class FClothMeshCopyToWorkBufferCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClothMeshCopyToWorkBufferCS);
	SHADER_USE_PARAMETER_STRUCT(FClothMeshCopyToWorkBufferCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, Offset)
		SHADER_PARAMETER(uint32, NumVertex)
		SHADER_PARAMETER_UAV(RWBuffer<float>, PositionVertexBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float>, WorkBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FClothMeshCopyToWorkBufferCS, "/Plugin/ShaderSandbox/Private/ClothMeshCopy.usf", "CopyToWorkBuffer", SF_Compute);

class FClothMeshCopyFromWorkBufferCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClothMeshCopyFromWorkBufferCS);
	SHADER_USE_PARAMETER_STRUCT(FClothMeshCopyFromWorkBufferCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, Offset)
		SHADER_PARAMETER(uint32, NumVertex)
		SHADER_PARAMETER_UAV(RWBuffer<float>, PositionVertexBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float>, WorkBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FClothMeshCopyFromWorkBufferCS, "/Plugin/ShaderSandbox/Private/ClothMeshCopy.usf", "CopyFromWorkBuffer", SF_Compute);

class FClothSimulationCS : public FGlobalShader
{
public:
	static const uint32 MAX_SPHERE_COLLISION = 16;

	DECLARE_GLOBAL_SHADER(FClothSimulationCS);
	SHADER_USE_PARAMETER_STRUCT(FClothSimulationCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, NumRow)
		SHADER_PARAMETER(uint32, NumColumn)
		SHADER_PARAMETER(uint32, NumVertex)
		SHADER_PARAMETER(float, GridWidth)
		SHADER_PARAMETER(float, GridHeight)
		SHADER_PARAMETER(float, SquareDeltaTime)
		SHADER_PARAMETER(float, Stiffness)
		SHADER_PARAMETER(float, Damping)
		SHADER_PARAMETER(FVector, PreviousInertia)
		SHADER_PARAMETER(FVector, WindVelocity)
		SHADER_PARAMETER(float, FluidDensity)
		SHADER_PARAMETER(float, DeltaTime)
		SHADER_PARAMETER(float, VertexRadius)
		SHADER_PARAMETER(uint32, NumSphereCollision)
		SHADER_PARAMETER_ARRAY(FVector4, SphereCenterAndRadiusArray, [MAX_SPHERE_COLLISION])
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

IMPLEMENT_GLOBAL_SHADER(FClothSimulationCS, "/Plugin/ShaderSandbox/Private/ClothSimulationGridMesh.usf", "Main", SF_Compute);

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

void FClothGridMeshDeformer::FlushDeformTaskQueue(FRHICommandListImmediate& RHICmdList, FRHIUnorderedAccessView* WorkVertexBufferUAV)
{
	FRDGBuilder GraphBuilder(RHICmdList);

	TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);

	TShaderMapRef<FClothSimulationCS> ClothSimulationCS(ShaderMap);

	// TODO:Stiffness、Dampingの効果がNumIterationやフレームレートに依存してしまっているのでどうにかせねば

	for (const FGridClothParameters& GridClothParams : DeformTaskQueue)
	{
		float DeltaTimePerIterate = GridClothParams.DeltaTime / GridClothParams.NumIteration;
		float SquareDeltaTime = DeltaTimePerIterate * DeltaTimePerIterate;
		const FVector& WindVelocity = GridClothParams.WindVelocity * FMath::FRandRange(0.0f, 2.0f); //毎フレーム、風力にランダムなゆらぎをつける

		for (uint32 IterCount = 0; IterCount < GridClothParams.NumIteration; IterCount++)
		{
			FClothSimulationCS::FParameters* ClothSimParams = GraphBuilder.AllocParameters<FClothSimulationCS::FParameters>();
			ClothSimParams->NumRow = GridClothParams.NumRow;
			ClothSimParams->NumColumn = GridClothParams.NumColumn;
			ClothSimParams->NumVertex = GridClothParams.NumVertex;
			ClothSimParams->GridWidth = GridClothParams.GridWidth;
			ClothSimParams->GridHeight = GridClothParams.GridHeight;
			ClothSimParams->SquareDeltaTime = SquareDeltaTime;
			ClothSimParams->Stiffness = GridClothParams.Stiffness;
			ClothSimParams->Damping = GridClothParams.Damping;
			ClothSimParams->PreviousInertia = GridClothParams.PreviousInertia;
			ClothSimParams->WindVelocity = WindVelocity;
			ClothSimParams->FluidDensity = GridClothParams.FluidDensity / (100.0f * 100.0f * 100.0f); // シェーダの計算がMKS単位系基準なのでそれに入れるFluidDensityはすごく小さくせねばならずユーザが入力しにくいので、MKS単位系で入れさせておいてここでスケールする
			ClothSimParams->DeltaTime = DeltaTimePerIterate;
			ClothSimParams->VertexRadius = GridClothParams.VertexRadius;
			ClothSimParams->NumSphereCollision = GridClothParams.SphereCollisionParams.Num();
			for (uint32 i = 0; i < FClothSimulationCS::MAX_SPHERE_COLLISION; i++)
			{
				if (i < ClothSimParams->NumSphereCollision)
				{
					ClothSimParams->SphereCenterAndRadiusArray[i] = FVector4(GridClothParams.SphereCollisionParams[i].RelativeCenter, GridClothParams.SphereCollisionParams[i].Radius);
				}
				else
				{
					ClothSimParams->SphereCenterAndRadiusArray[i] = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
				}
			}
			ClothSimParams->InAccelerationVertexBuffer = GridClothParams.AccelerationVertexBufferSRV;
			ClothSimParams->OutPrevPositionVertexBuffer = GridClothParams.PrevPositionVertexBufferUAV;
			ClothSimParams->OutPositionVertexBuffer = GridClothParams.PositionVertexBufferUAV;

			//TODO: とりあえず今はこの関数呼び出しがメッシュ一個なので1 Dispatch
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("ClothSimulation"),
				*ClothSimulationCS,
				ClothSimParams,
				FIntVector(1, 1, 1)
			);
		}
	}

	for (const FGridClothParameters& GridClothParams : DeformTaskQueue)
	{
		TShaderMapRef<FClothGridMeshTangentCS> GridMeshTangentCS(ShaderMap);

		FClothGridMeshTangentCS::FParameters* GridMeshTangentParams = GraphBuilder.AllocParameters<FClothGridMeshTangentCS::FParameters>();
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

