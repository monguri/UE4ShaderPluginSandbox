#include "FFT2D.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"

namespace FFT2D
{
class FForwardFFT2D512x512CS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FForwardFFT2D512x512CS);
	SHADER_USE_PARAMETER_STRUCT(FForwardFFT2D512x512CS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(Buffer<float4>, SrcTexture2D)
		SHADER_PARAMETER_UAV(RWBuffer<float4>, DstTexture2D)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FForwardFFT2D512x512CS, "/Plugin/ShaderSandbox/Private/FFT.usf", "ForwardFFT2D512x512CS", SF_Compute);

void DoFFT2D512x512(FRHICommandListImmediate& RHICmdList, EFFTMode Mode, FRHIShaderResourceView* SrcSRV, FRHIUnorderedAccessView* DstUAV)
{
	FRDGBuilder GraphBuilder(RHICmdList);

	TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);

	TShaderMapRef<FForwardFFT2D512x512CS> ForwardFFTCS(ShaderMap);

	FForwardFFT2D512x512CS::FParameters* ForwardFFTParams = GraphBuilder.AllocParameters<FForwardFFT2D512x512CS::FParameters>();
	ForwardFFTParams->SrcTexture2D = SrcSRV;
	ForwardFFTParams->DstTexture2D = DstUAV;

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("ForwardFFT2D512x512CS"),
		*ForwardFFTCS,
		ForwardFFTParams,
		FIntVector(512, 1, 1)
	);

	GraphBuilder.Execute();
}
}; // namespace FFT2D

