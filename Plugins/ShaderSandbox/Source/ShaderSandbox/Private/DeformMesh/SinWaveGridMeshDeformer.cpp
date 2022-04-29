#include "DeformMesh/SinWaveGridMeshDeformer.h"
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
		SHADER_PARAMETER(float, GridWidth)
		SHADER_PARAMETER(float, GridHeight)
		SHADER_PARAMETER(float, WaveLengthRow)
		SHADER_PARAMETER(float, WaveLengthColumn)
		SHADER_PARAMETER(float, Period)
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

class FSinWaveGridMeshTangentCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSinWaveGridMeshTangentCS);
	SHADER_USE_PARAMETER_STRUCT(FSinWaveGridMeshTangentCS, FGlobalShader);

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

IMPLEMENT_GLOBAL_SHADER(FSinWaveGridMeshTangentCS, "/Plugin/ShaderSandbox/Private/GridMeshTangent.usf", "MainCS", SF_Compute);

void SinWaveDeformGridMesh(FRHICommandListImmediate& RHICmdList, const FGridSinWaveParameters& GridSinWaveParams, FRHIUnorderedAccessView* PositionVertexBufferUAV, class FRHIUnorderedAccessView* TangentVertexBufferUAV
#if 0
#if RHI_RAYTRACING
	, const FRayTracingGeometry& RayTracingGeometry
#endif
#endif
)
{
	FRDGBuilder GraphBuilder(RHICmdList);

#if ENGINE_MINOR_VERSION >= 25
	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);
#else
	TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);
#endif

	const uint32 DispatchCount = FMath::DivideAndRoundUp(GridSinWaveParams.NumVertex, (uint32)32);
	check(DispatchCount <= 65535);

	TShaderMapRef<FSinWaveDeformCS> SinWaveDeformCS(ShaderMap);

	FSinWaveDeformCS::FParameters* SinWaveDeformParams = GraphBuilder.AllocParameters<FSinWaveDeformCS::FParameters>();
	SinWaveDeformParams->NumRow = GridSinWaveParams.NumRow;
	SinWaveDeformParams->NumColumn = GridSinWaveParams.NumColumn;
	SinWaveDeformParams->NumVertex = GridSinWaveParams.NumVertex;
	SinWaveDeformParams->GridWidth = GridSinWaveParams.GridWidth;
	SinWaveDeformParams->GridHeight = GridSinWaveParams.GridHeight;
	SinWaveDeformParams->WaveLengthRow = GridSinWaveParams.WaveLengthRow;
	SinWaveDeformParams->WaveLengthColumn = GridSinWaveParams.WaveLengthColumn;
	SinWaveDeformParams->Period = GridSinWaveParams.Period;
	SinWaveDeformParams->Amplitude = GridSinWaveParams.Amplitude;
	SinWaveDeformParams->Time = GridSinWaveParams.AccumulatedTime;
	SinWaveDeformParams->OutPositionVertexBuffer = PositionVertexBufferUAV;

	ERDGPassFlags PassFlags = GridSinWaveParams.bAsync ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute;

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("SinWaveDeformMesh"),
		PassFlags,
#if ENGINE_MINOR_VERSION >= 25
		SinWaveDeformCS,
#else
		*SinWaveDeformCS,
#endif
		SinWaveDeformParams,
		FIntVector(DispatchCount, 1, 1)
	);

	TShaderMapRef<FSinWaveGridMeshTangentCS> GridMeshTangentCS(ShaderMap);

	FSinWaveGridMeshTangentCS::FParameters* GridMeshTangent = GraphBuilder.AllocParameters<FSinWaveGridMeshTangentCS::FParameters>();
	GridMeshTangent->NumRow = GridSinWaveParams.NumRow;
	GridMeshTangent->NumColumn = GridSinWaveParams.NumColumn;
	GridMeshTangent->NumVertex = GridSinWaveParams.NumVertex;
	GridMeshTangent->InPositionVertexBuffer = PositionVertexBufferUAV;
	GridMeshTangent->OutTangentVertexBuffer = TangentVertexBufferUAV;

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("GridMeshTangent"),
		PassFlags,
#if ENGINE_MINOR_VERSION >= 25
		GridMeshTangentCS,
#else
		*GridMeshTangentCS,
#endif
		GridMeshTangent,
		FIntVector(DispatchCount, 1, 1)
	);

	GraphBuilder.Execute();

#if 0
#if RHI_RAYTRACING
	TArray<FAccelerationStructureUpdateParams> Updates;
	FAccelerationStructureUpdateParams Params;
	Params.Geometry = RayTracingGeometry.RayTracingGeometryRHI;
	Params.VertexBuffer = RayTracingGeometry.>Initializer.PositionVertexBuffer;
	Updates.Add(Params);

	RHICmdList.UpdateAccelerationStructures(Updates);
#endif
#endif
}
