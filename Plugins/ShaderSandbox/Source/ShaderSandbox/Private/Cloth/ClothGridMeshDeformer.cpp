#include "Cloth/ClothGridMeshDeformer.h"
#include "Cloth/ClothVertexBuffers.h"
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
		SHADER_PARAMETER_SRV(Buffer<float>, AccelerationMoveVertexBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float>, WorkAccelerationMoveBuffer)
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
		SHADER_PARAMETER_UAV(RWBuffer<float>, WorkAccelerationMoveVertexBuffer)
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

FClothGridMeshDeformer::~FClothGridMeshDeformer()
{
	ClothParameterStructuredBuffer.ReleaseResource();
}

void FClothGridMeshDeformer::EnqueueDeformCommand(const FClothGridMeshDeformCommand& Command)
{
	DeformCommandQueue.Add(Command);
}

void FClothGridMeshDeformer::FlushDeformCommandQueue(FRHICommandListImmediate& RHICmdList, FRHIUnorderedAccessView* WorkAccelerationMoveVertexBufferUAV, FRHIUnorderedAccessView* WorkPrevVertexBufferUAV, FRHIUnorderedAccessView* WorkVertexBufferUAV)
{
	FRDGBuilder GraphBuilder(RHICmdList);

#if ENGINE_MINOR_VERSION >= 25
	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);
#else
	TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);
#endif

	uint32 NumClothMesh = DeformCommandQueue.Num();
	// TODO:�ǂ����Ńo���f�[�V����������
	check(NumClothMesh > 0);
	check(NumClothMesh <= FClothSimulationCS::MAX_CLOTH_MESH);

	// TODO:�֐������悤
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
			ClothCopyToWorkParams->AccelerationMoveVertexBuffer = DeformCommand.VertexBuffers->AccelerationMoveVertexBuffer.GetSRV();
			ClothCopyToWorkParams->WorkAccelerationMoveBuffer = WorkAccelerationMoveVertexBufferUAV;
			ClothCopyToWorkParams->PrevPositionVertexBuffer = DeformCommand.VertexBuffers->PrevPositionVertexBuffer.GetUAV();
			ClothCopyToWorkParams->WorkPrevPositionBuffer = WorkPrevVertexBufferUAV;
			ClothCopyToWorkParams->PositionVertexBuffer = DeformCommand.VertexBuffers->PositionVertexBuffer.GetUAV();
			ClothCopyToWorkParams->WorkPositionBuffer = WorkVertexBufferUAV;

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("ClothMeshCopyToWorkBuffer"),
				ERDGPassFlags::AsyncCompute,
#if ENGINE_MINOR_VERSION >= 25
				ClothMeshCopyToWorkBufferCS,
#else
				*ClothMeshCopyToWorkBufferCS,
#endif
				ClothCopyToWorkParams,
				FIntVector(1, 1, 1)
			);
		}
	}

	{
		FClothSimulationCS::FParameters* ClothSimParams = GraphBuilder.AllocParameters<FClothSimulationCS::FParameters>();

		TArray<FGridClothParameters> ClothParams;
		ClothParams.Reserve(NumClothMesh);

		// TODO:Stiffness�ADamping�̌��ʂ�NumIteration��t���[�����[�g�Ɉˑ����Ă��܂��Ă���̂łǂ��ɂ����˂�

		// ���s���Ɍ��܂�N���X�p�����[�^�̐ݒ��StructuredBuffer�p��TArray�̍쐬
		uint32 Offset = 0;
		for (uint32 MeshIdx = 0; MeshIdx < NumClothMesh; MeshIdx++)
		{
			FClothGridMeshDeformCommand& DeformCommand = DeformCommandQueue[MeshIdx];
			FGridClothParameters& GridClothParams = DeformCommand.Params;

			GridClothParams.VertexIndexOffset = Offset;
			Offset += GridClothParams.NumVertex;

			check(GridClothParams.NumSphereCollision <= FGridClothParameters::MAX_SPHERE_COLLISION_PER_MESH);
			
			ClothParams.Add(GridClothParams);
		}

		ClothParameterStructuredBuffer.SetData(ClothParams);

		ClothSimParams->Params = ClothParameterStructuredBuffer.GetSRV();
		ClothSimParams->WorkAccelerationMoveVertexBuffer = WorkAccelerationMoveVertexBufferUAV;
		ClothSimParams->WorkPrevPositionVertexBuffer = WorkPrevVertexBufferUAV;
		ClothSimParams->WorkPositionVertexBuffer = WorkVertexBufferUAV;

		TShaderMapRef<FClothSimulationCS> ClothSimulationCS(ShaderMap);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ClothSimulation"),
			ERDGPassFlags::AsyncCompute,
#if ENGINE_MINOR_VERSION >= 25
			ClothSimulationCS,
#else
			*ClothSimulationCS,
#endif
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
			ClothCopyFromWorkParams->PrevPositionVertexBuffer = DeformCommand.VertexBuffers->PrevPositionVertexBuffer.GetUAV();
			ClothCopyFromWorkParams->WorkPrevPositionBuffer = WorkPrevVertexBufferUAV;
			ClothCopyFromWorkParams->PositionVertexBuffer = DeformCommand.VertexBuffers->PositionVertexBuffer.GetUAV();
			ClothCopyFromWorkParams->WorkPositionBuffer = WorkVertexBufferUAV;

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("ClothMeshCopyFromWorkBuffer"),
				ERDGPassFlags::AsyncCompute,
#if ENGINE_MINOR_VERSION >= 25
				ClothMeshCopyFromWorkBufferCS,
#else
				*ClothMeshCopyFromWorkBufferCS,
#endif
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
			GridMeshTangentParams->InPositionVertexBuffer = DeformCommand.VertexBuffers->PositionVertexBuffer.GetUAV();
			GridMeshTangentParams->OutTangentVertexBuffer = DeformCommand.VertexBuffers->DeformableMeshVertexBuffer.GetTangentsUAV();

			const uint32 DispatchCount = FMath::DivideAndRoundUp(GridClothParams.NumVertex, (uint32)32);
			check(DispatchCount <= 65535);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("GridMeshTangent"),
				ERDGPassFlags::AsyncCompute,
#if ENGINE_MINOR_VERSION >= 25
				GridMeshTangentCS,
#else
				*GridMeshTangentCS,
#endif
				GridMeshTangentParams,
				FIntVector(DispatchCount, 1, 1)
			);
		}
	}

	GraphBuilder.Execute();

	DeformCommandQueue.Reset();
}

