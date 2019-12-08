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
		SHADER_PARAMETER_UAV(RWBuffer<float>, OutPositionVertexBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FClothSimulationSolveDistanceConstraintCS, "/Plugin/ShaderSandbox/Private/ClothSimulationGridMesh.usf", "SolveDistanceConstraint", SF_Compute);

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


	TShaderMapRef<FClothSimulationIntegrationCS> ClothSimIntegrationCS(ShaderMap);

	FClothSimulationIntegrationCS::FParameters* ClothSimIntegrateParams = GraphBuilder.AllocParameters<FClothSimulationIntegrationCS::FParameters>();
	ClothSimIntegrateParams->NumVertex = GridClothParams.NumVertex;
	ClothSimIntegrateParams->SquareDeltaTime = GridClothParams.DeltaTime * GridClothParams.DeltaTime;
	ClothSimIntegrateParams->InAccelerationVertexBuffer = AccelerationVertexBufferSRV;
	ClothSimIntegrateParams->OutPrevPositionVertexBuffer = PrevPositionVertexBufferUAV;
	ClothSimIntegrateParams->OutPositionVertexBuffer = PositionVertexBufferUAV;

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("ClothSimulationIntegration"),
		*ClothSimIntegrationCS,
		ClothSimIntegrateParams,
		FIntVector(DispatchCount, 1, 1)
	);


	TShaderMapRef<FClothSimulationSolveDistanceConstraintCS> ClothSimSolveDistanceConstraintCS(ShaderMap);

	FClothSimulationSolveDistanceConstraintCS::FParameters* ClothSimDistanceConstraintParams = GraphBuilder.AllocParameters<FClothSimulationSolveDistanceConstraintCS::FParameters>();
	ClothSimDistanceConstraintParams->NumRow = GridClothParams.NumRow;
	ClothSimDistanceConstraintParams->NumColumn = GridClothParams.NumColumn;
	ClothSimDistanceConstraintParams->NumVertex = GridClothParams.NumVertex;
	ClothSimDistanceConstraintParams->GridWidth = GridClothParams.GridWidth;
	ClothSimDistanceConstraintParams->GridHeight = GridClothParams.GridHeight;
	ClothSimDistanceConstraintParams->OutPositionVertexBuffer = PositionVertexBufferUAV;

	//TODO:本来は距離コンストレイント解決はループが必要だが、重心を考慮せずに必ずRowIndexが小さい方に引き付けるという前提を置いてループを一回にする
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("ClothSimulationSolveDistanceConstraint"),
		*ClothSimSolveDistanceConstraintCS,
		ClothSimDistanceConstraintParams,
		FIntVector(GridClothParams.NumVertex, 1, 1) // TODO:とりあえず、ここはマルチスレッドをしない
	);

	TShaderMapRef<FClothGridMeshTangentCS> GridMeshTangentCS(ShaderMap);

	FClothGridMeshTangentCS::FParameters* GridMeshTangentParams = GraphBuilder.AllocParameters<FClothGridMeshTangentCS::FParameters>();
	GridMeshTangentParams->NumRow = GridClothParams.NumRow;
	GridMeshTangentParams->NumColumn = GridClothParams.NumColumn;
	GridMeshTangentParams->NumVertex = GridClothParams.NumVertex;
	GridMeshTangentParams->InPositionVertexBuffer = PositionVertexBufferUAV;
	GridMeshTangentParams->OutTangentVertexBuffer = TangentVertexBufferUAV;

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("GridMeshTangent"),
		*GridMeshTangentCS,
		GridMeshTangentParams,
		FIntVector(DispatchCount, 1, 1)
	);

	GraphBuilder.Execute();
}

