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
		SHADER_PARAMETER(uint32, NumVertex)
		SHADER_PARAMETER_UAV(RWBuffer<float>, OutPositionVertexBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FSinWaveDeformCS, "/Plugin/ShaderSandbox/Private/SinWaveDeformGridMesh.usf", "MainCS", SF_Compute);

static FIntVector ComputeDispatchCount(uint32 ItemCount, uint32 GroupSize)
{
	const uint32 BatchCount = FMath::DivideAndRoundUp(ItemCount, GroupSize);
	const uint32 DispatchCountX = FMath::FloorToInt(FMath::Sqrt(BatchCount));
	const uint32 DispatchCountY = DispatchCountX + FMath::DivideAndRoundUp(BatchCount - DispatchCountX * DispatchCountX, DispatchCountX);

	check(DispatchCountX <= 65535);
	check(DispatchCountY <= 65535);
	check(BatchCount <= DispatchCountX * DispatchCountY);
	return FIntVector(DispatchCountX, DispatchCountY, 1);
}

void SinWaveDeformGridMesh(FRHICommandListImmediate& RHICmdList, uint32 NumVertex, FRHIUnorderedAccessView* PositionVertexBufferUAV)
{
	FRDGBuilder GraphBuilder(RHICmdList);
	FSinWaveDeformCS::FParameters* Parameters = GraphBuilder.AllocParameters<FSinWaveDeformCS::FParameters>();
	Parameters->NumVertex = NumVertex;
	Parameters->OutPositionVertexBuffer = PositionVertexBufferUAV;

	TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);

	const uint32 DispatchCount = FMath::DivideAndRoundUp(NumVertex, (uint32)32);
	check(DispatchCount <= 65535);

	TShaderMapRef<FSinWaveDeformCS> ComputeShader(ShaderMap);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("SinWaveDeformMesh"),
		*ComputeShader,
		Parameters,
		FIntVector(DispatchCount, 1, 1)
	);

	GraphBuilder.Execute();
}
