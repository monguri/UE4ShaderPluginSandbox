#include "DeformMesh/ClothGridMeshDeformer.h"
#include "GlobalShader.h"
#include "RHIResources.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"

class FClothMeshCopyToWorkBufferCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClothMeshCopyToWorkBufferCS);
	SHADER_USE_PARAMETER_STRUCT(FClothMeshCopyToWorkBufferCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, VertexIndexOffset)
		SHADER_PARAMETER(uint32, NumVertex)
		SHADER_PARAMETER_SRV(Buffer<float>, AccelerationVertexBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float>, WorkAccelerationBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float>, PrevPositionVertexBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float>, WorkPrevPositionBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float>, PositionVertexBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float>, WorkPositionBuffer)
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
		SHADER_PARAMETER(uint32, VertexIndexOffset)
		SHADER_PARAMETER(uint32, NumVertex)
		SHADER_PARAMETER_UAV(RWBuffer<float>, PrevPositionVertexBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float>, WorkPrevPositionBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float>, PositionVertexBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float>, WorkPositionBuffer)
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
	static const uint32 MAX_CLOTH_MESH = 16;

	DECLARE_GLOBAL_SHADER(FClothSimulationCS);
	SHADER_USE_PARAMETER_STRUCT(FClothSimulationCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(StructuredBuffer<FGridClothParameters>, Params)
		SHADER_PARAMETER_UAV(RWBuffer<float>, WorkAccelerationVertexBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float>, WorkPrevPositionVertexBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float>, WorkPositionVertexBuffer)
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

void FClothGridMeshDeformer::EnqueueDeformCommand(const FClothGridMeshDeformCommand& Command)
{
	DeformCommandQueue.Add(Command);
}

void FClothGridMeshDeformer::FlushDeformCommandQueue(FRHICommandListImmediate& RHICmdList, FRHIUnorderedAccessView* WorkAccelerationVertexBufferUAV, FRHIUnorderedAccessView* WorkPrevVertexBufferUAV, FRHIUnorderedAccessView* WorkVertexBufferUAV)
{
	FRDGBuilder GraphBuilder(RHICmdList);

	TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);

	uint32 NumClothMesh = DeformCommandQueue.Num();
	// TODO:どこかでバリデーションしたい
	check(NumClothMesh > 0);
	check(NumClothMesh <= FClothSimulationCS::MAX_CLOTH_MESH);

	// TODO:関数化しよう
	{
		uint32 Offset = 0;
		for (uint32 MeshIdx = 0; MeshIdx < NumClothMesh; MeshIdx++)
		{
			TShaderMapRef<FClothMeshCopyToWorkBufferCS> ClothMeshCopyToWorkBufferCS(ShaderMap);
			FClothMeshCopyToWorkBufferCS::FParameters* ClothCopyToWorkParams = GraphBuilder.AllocParameters<FClothMeshCopyToWorkBufferCS::FParameters>();

			const FClothGridMeshDeformCommand& DeformCommand = DeformCommandQueue[MeshIdx];
			ClothCopyToWorkParams->VertexIndexOffset = Offset;
			Offset += DeformCommand.Params.NumVertex;
			ClothCopyToWorkParams->NumVertex = DeformCommand.Params.NumVertex;
			ClothCopyToWorkParams->AccelerationVertexBuffer = DeformCommand.AccelerationVertexBufferSRV;
			ClothCopyToWorkParams->WorkAccelerationBuffer = WorkAccelerationVertexBufferUAV;
			ClothCopyToWorkParams->PrevPositionVertexBuffer = DeformCommand.PrevPositionVertexBufferUAV;
			ClothCopyToWorkParams->WorkPrevPositionBuffer = WorkPrevVertexBufferUAV;
			ClothCopyToWorkParams->PositionVertexBuffer = DeformCommand.PositionVertexBufferUAV;
			ClothCopyToWorkParams->WorkPositionBuffer = WorkVertexBufferUAV;

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("ClothMeshCopyToWorkBuffer"),
				*ClothMeshCopyToWorkBufferCS,
				ClothCopyToWorkParams,
				FIntVector(1, 1, 1)
			);
		}
	}

	{
		FClothSimulationCS::FParameters* ClothSimParams = GraphBuilder.AllocParameters<FClothSimulationCS::FParameters>();

		TArray<FGridClothParameters> ClothParams;
		ClothParams.Reserve(NumClothMesh);

		// TODO:Stiffness、Dampingの効果がNumIterationやフレームレートに依存してしまっているのでどうにかせねば

		// 実行時に決まるクロスパラメータの設定とStructuredBuffer用のTArrayの作成
		uint32 Offset = 0;
		for (uint32 MeshIdx = 0; MeshIdx < NumClothMesh; MeshIdx++)
		{
			FClothGridMeshDeformCommand& DeformCommand = DeformCommandQueue[MeshIdx];
			FGridClothParameters& GridClothParams = DeformCommand.Params;

			GridClothParams.VertexIndexOffset = Offset;
			Offset += GridClothParams.NumVertex;

			// Wind系の入力パラメータを使用に際し調整
			const FVector& WindVelocity = GridClothParams.WindVelocity * FMath::FRandRange(0.0f, 2.0f); //毎フレーム、風力にランダムなゆらぎをつける
			GridClothParams.WindVelocity = WindVelocity;

			GridClothParams.FluidDensity = GridClothParams.FluidDensity / (100.0f * 100.0f * 100.0f); // シェーダの計算がMKS単位系基準なのでそれに入れるFluidDensityはすごく小さくせねばならずユーザが入力しにくいので、MKS単位系で入れさせておいてここでスケールする

			check(GridClothParams.NumSphereCollision <= FGridClothParameters::MAX_SPHERE_COLLISION_PER_MESH);
			
			ClothParams.Add(GridClothParams);
		}

		// TODO:今は毎フレーム生成でBUF_Volatileにしてるけど、BUF_Staticにして、毎フレーム書き換えあるいはreallocにしたい
		FClothParameterStructuredBuffer* ClothParameterStructuredBuffer = new FClothParameterStructuredBuffer(ClothParams);
		ClothParameterStructuredBuffer->InitResource();

		ClothSimParams->Params = ClothParameterStructuredBuffer->GetSRV();
		ClothSimParams->WorkAccelerationVertexBuffer = WorkAccelerationVertexBufferUAV;
		ClothSimParams->WorkPrevPositionVertexBuffer = WorkPrevVertexBufferUAV;
		ClothSimParams->WorkPositionVertexBuffer = WorkVertexBufferUAV;

		TShaderMapRef<FClothSimulationCS> ClothSimulationCS(ShaderMap);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ClothSimulation"),
			*ClothSimulationCS,
			ClothSimParams,
			FIntVector(NumClothMesh, 1, 1)
		);
	}

	{
		uint32 Offset = 0;
		for (uint32 MeshIdx = 0; MeshIdx < NumClothMesh; MeshIdx++)
		{
			TShaderMapRef<FClothMeshCopyFromWorkBufferCS> ClothMeshCopyFromWorkBufferCS(ShaderMap);

			FClothMeshCopyFromWorkBufferCS::FParameters* ClothCopyFromWorkParams = GraphBuilder.AllocParameters<FClothMeshCopyFromWorkBufferCS::FParameters>();
			const FClothGridMeshDeformCommand& DeformCommand = DeformCommandQueue[MeshIdx];

			ClothCopyFromWorkParams->VertexIndexOffset = Offset;
			Offset += DeformCommand.Params.NumVertex;
			ClothCopyFromWorkParams->NumVertex = DeformCommand.Params.NumVertex;
			ClothCopyFromWorkParams->PrevPositionVertexBuffer = DeformCommand.PrevPositionVertexBufferUAV;
			ClothCopyFromWorkParams->WorkPrevPositionBuffer = WorkPrevVertexBufferUAV;
			ClothCopyFromWorkParams->PositionVertexBuffer = DeformCommand.PositionVertexBufferUAV;
			ClothCopyFromWorkParams->WorkPositionBuffer = WorkVertexBufferUAV;

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("ClothMeshCopyFromWorkBuffer"),
				*ClothMeshCopyFromWorkBufferCS,
				ClothCopyFromWorkParams,
				FIntVector(1, 1, 1)
			);
		}
	}

	{
		for (const FClothGridMeshDeformCommand& DeformCommand : DeformCommandQueue)
		{
			TShaderMapRef<FClothGridMeshTangentCS> GridMeshTangentCS(ShaderMap);

			FClothGridMeshTangentCS::FParameters* GridMeshTangentParams = GraphBuilder.AllocParameters<FClothGridMeshTangentCS::FParameters>();

			const FGridClothParameters& GridClothParams = DeformCommand.Params;
			GridMeshTangentParams->NumRow = GridClothParams.NumRow;
			GridMeshTangentParams->NumColumn = GridClothParams.NumColumn;
			GridMeshTangentParams->NumVertex = GridClothParams.NumVertex;
			GridMeshTangentParams->InPositionVertexBuffer = DeformCommand.PositionVertexBufferUAV;
			GridMeshTangentParams->OutTangentVertexBuffer = DeformCommand.TangentVertexBufferUAV;

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
	}

	GraphBuilder.Execute();

	DeformCommandQueue.Reset();
}

