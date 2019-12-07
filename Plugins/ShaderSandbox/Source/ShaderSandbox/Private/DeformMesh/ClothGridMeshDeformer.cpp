#include "ClothGridMeshDeformer.h"
#include "GlobalShader.h"
#include "RHIResources.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"

class FClothSimulationCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClothSimulationCS);
	SHADER_USE_PARAMETER_STRUCT(FClothSimulationCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, NumRow)
		SHADER_PARAMETER(uint32, NumColumn)
		SHADER_PARAMETER(uint32, NumVertex)
		SHADER_PARAMETER(float, GridWidth)
		SHADER_PARAMETER(float, GridHeight)
		SHADER_PARAMETER(float, SquareDeltaTime)
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

IMPLEMENT_GLOBAL_SHADER(FClothSimulationCS, "/Plugin/ShaderSandbox/Private/ClothSimulationGridMesh.usf", "MainCS", SF_Compute);

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

	const uint32 DispatchCount = FMath::DivideAndRoundUp(GridClothParams.NumVertex, (uint32)32);
	check(DispatchCount <= 65535);

	TShaderMapRef<FClothSimulationCS> ClothSimulationCS(ShaderMap);

	FClothSimulationCS::FParameters* ClothSimulationParams = GraphBuilder.AllocParameters<FClothSimulationCS::FParameters>();
	ClothSimulationParams->NumRow = GridClothParams.NumRow;
	ClothSimulationParams->NumColumn = GridClothParams.NumColumn;
	ClothSimulationParams->NumVertex = GridClothParams.NumVertex;
	ClothSimulationParams->GridWidth = GridClothParams.GridWidth;
	ClothSimulationParams->GridHeight = GridClothParams.GridHeight;
	ClothSimulationParams->SquareDeltaTime = GridClothParams.DeltaTime * GridClothParams.DeltaTime;
	ClothSimulationParams->InAccelerationVertexBuffer = AccelerationVertexBufferSRV;
	ClothSimulationParams->OutPrevPositionVertexBuffer = PrevPositionVertexBufferUAV;
	ClothSimulationParams->OutPositionVertexBuffer = PositionVertexBufferUAV;

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("ClothSimulationMesh"),
		*ClothSimulationCS,
		ClothSimulationParams,
		FIntVector(DispatchCount, 1, 1)
	);

	TShaderMapRef<FClothGridMeshTangentCS> GridMeshTangentCS(ShaderMap);

	FClothGridMeshTangentCS::FParameters* GridMeshTangent = GraphBuilder.AllocParameters<FClothGridMeshTangentCS::FParameters>();
	GridMeshTangent->NumRow = GridClothParams.NumRow;
	GridMeshTangent->NumColumn = GridClothParams.NumColumn;
	GridMeshTangent->NumVertex = GridClothParams.NumVertex;
	GridMeshTangent->InPositionVertexBuffer = PositionVertexBufferUAV;
	GridMeshTangent->OutTangentVertexBuffer = TangentVertexBufferUAV;

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("GridMeshTangent"),
		*GridMeshTangentCS,
		GridMeshTangent,
		FIntVector(DispatchCount, 1, 1)
	);

	GraphBuilder.Execute();
}

