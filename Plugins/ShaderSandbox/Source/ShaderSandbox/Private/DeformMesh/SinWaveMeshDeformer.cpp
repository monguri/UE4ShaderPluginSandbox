#include "SinWaveMeshDeformer.h"
#include "GlobalShader.h"
#include "RHIResources.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"

class FSinWaveDeformCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSinWaveDeformCS);
	SHADER_USE_PARAMETER_STRUCT(FSinWaveDeformCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_UAV(RWBuffer, OutPositionVertexBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FSinWaveDeformCS, "/Plugin/ShaderSandbox/Private/SinWaveDeformMesh.usf", "MainCS", SF_Compute);

void SinWaveDeformMesh(FRHICommandListImmediate& RHICmdList, uint32 NumVertex, FRHIUnorderedAccessView* PositionVertexBufferUAV)
{
	FRDGBuilder GraphBuilder(RHICmdList);
	FSinWaveDeformCS::FParameters* Parameters = GraphBuilder.AllocParameters<FSinWaveDeformCS::FParameters>();
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
