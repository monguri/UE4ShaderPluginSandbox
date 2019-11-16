#include "SinWaveGridMeshDeformer.h"
#include "GlobalShader.h"
#include "RHIResources.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"

class FSinWaveDeformCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSinWaveDeformCS);
	SHADER_USE_PARAMETER_STRUCT(FSinWaveDeformCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, NumRow)
		SHADER_PARAMETER(uint32, NumColumn)
		SHADER_PARAMETER(uint32, NumVertex)
		SHADER_PARAMETER(float, WaveNumberRow)
		SHADER_PARAMETER(float, WaveNumberColumn)
		SHADER_PARAMETER(float, Frequency)
		SHADER_PARAMETER(float, Amplitude)
		SHADER_PARAMETER(float, Time)
		SHADER_PARAMETER_UAV(RWBuffer<float>, OutPositionVertexBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FSinWaveDeformCS, "/Plugin/ShaderSandbox/Private/SinWaveDeformGridMesh.usf", "MainCS", SF_Compute);

class FGridMeshTangentCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FGridMeshTangentCS);
	SHADER_USE_PARAMETER_STRUCT(FGridMeshTangentCS, FGlobalShader);

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

IMPLEMENT_GLOBAL_SHADER(FGridMeshTangentCS, "/Plugin/ShaderSandbox/Private/GridMeshTangent.usf", "MainCS", SF_Compute);

void SinWaveDeformGridMesh(FRHICommandListImmediate& RHICmdList, uint32 NumRow, uint32 NumColumn, uint32 NumVertex, float WaveNumberRow, float WaveNumberColumn, float Frequency, float Amplitude, float DeltaTime, FRHIUnorderedAccessView* PositionVertexBufferUAV, class FRHIUnorderedAccessView* TangentVertexBufferUAV)
{
	FRDGBuilder GraphBuilder(RHICmdList);

	TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);

	const uint32 DispatchCount = FMath::DivideAndRoundUp(NumVertex, (uint32)32);
	check(DispatchCount <= 65535);

	TShaderMapRef<FSinWaveDeformCS> SinWaveDeformCS(ShaderMap);

	static float AccumulatedTime = 0.0f;
	AccumulatedTime += DeltaTime;

	// reset by big number.
	if (AccumulatedTime > 10000)
	{
		AccumulatedTime = 0.0f;
	}

	FSinWaveDeformCS::FParameters* SinWaveDeformParams = GraphBuilder.AllocParameters<FSinWaveDeformCS::FParameters>();
	SinWaveDeformParams->NumRow = NumRow;
	SinWaveDeformParams->NumColumn = NumColumn;
	SinWaveDeformParams->NumVertex = NumVertex;
	SinWaveDeformParams->WaveNumberRow = WaveNumberRow;
	SinWaveDeformParams->WaveNumberColumn = WaveNumberColumn;
	SinWaveDeformParams->Frequency = Frequency;
	SinWaveDeformParams->Amplitude = Amplitude;
	SinWaveDeformParams->Time = AccumulatedTime;
	SinWaveDeformParams->OutPositionVertexBuffer = PositionVertexBufferUAV;

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("SinWaveDeformMesh"),
		*SinWaveDeformCS,
		SinWaveDeformParams,
		FIntVector(DispatchCount, 1, 1)
	);

	TShaderMapRef<FGridMeshTangentCS> GridMeshTangentCS(ShaderMap);

	FGridMeshTangentCS::FParameters* GridMeshTangent = GraphBuilder.AllocParameters<FGridMeshTangentCS::FParameters>();
	GridMeshTangent->NumRow = NumRow;
	GridMeshTangent->NumColumn = NumColumn;
	GridMeshTangent->NumVertex = NumVertex;
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
