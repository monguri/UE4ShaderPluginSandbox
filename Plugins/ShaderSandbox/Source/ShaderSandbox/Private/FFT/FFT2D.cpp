#include "FFT2D.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHIStaticStates.h"
#include "RenderTargetPool.h"

// PostProcessFFTBloom.cppÇéQçlÇ…ÇµÇƒÇ¢ÇÈ

namespace FFT2D
{
class FTwoForOneRealFFTImage1D512x512 : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTwoForOneRealFFTImage1D512x512);
	SHADER_USE_PARAMETER_STRUCT(FTwoForOneRealFFTImage1D512x512, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntRect, SrcRect)
		SHADER_PARAMETER(FIntRect, DstRect)
		SHADER_PARAMETER_TEXTURE(Texture2D<float4>, SrcTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SrcSampler)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, DstTexture)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FTwoForOneRealFFTImage1D512x512, "/Plugin/ShaderSandbox/Private/FFT.usf", "TwoForOneRealFFTImage1D512x512", SF_Compute);

void DoFFT2D512x512(FRHICommandListImmediate& RHICmdList, EFFTMode Mode, const FTextureRHIRef& SrcTexture, FRHIUnorderedAccessView* DstUAV)
{
	const FIntRect& SrcRect = FIntRect(FIntPoint(0, 0), FIntPoint(512, 512));
	const uint32 FREQUENCY_PADDING = 2; //TODO:óùã¸ÇóùâÇ≈Ç´ÇƒÇ»Ç¢
	const FIntPoint& TmpBufferSize = FIntPoint(512 + FREQUENCY_PADDING, 512);
	const FIntRect& TmpRect = FIntRect(FIntPoint(0, 0), TmpBufferSize);

	FRDGBuilder GraphBuilder(RHICmdList);

	TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);

	{
		TShaderMapRef<FTwoForOneRealFFTImage1D512x512> TwoForOneCS(ShaderMap);

		FPooledRenderTargetDesc Desc = FPooledRenderTargetDesc::Create2DDesc(
			TmpBufferSize,
			EPixelFormat::PF_A32B32G32R32F,
			FClearValueBinding::None, 
			TexCreate_None,
			TexCreate_RenderTargetable | TexCreate_UAV,
			false
		);

		// TODO:Ç±ÇÍÇÕà»ëOé¿å±ÇµÇΩRDGTextureÇÃèëÇ´ï˚Ç≈èëÇØÇÈÇ©Ç‡
		TRefCountPtr<IPooledRenderTarget> TmpRenderTarget;
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, TmpRenderTarget, TEXT("FFT2D Tmp Buffer"));

		FTwoForOneRealFFTImage1D512x512::FParameters* ForwardFFTParams = GraphBuilder.AllocParameters<FTwoForOneRealFFTImage1D512x512::FParameters>();
		ForwardFFTParams->SrcTexture = SrcTexture;
		ForwardFFTParams->SrcSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
		ForwardFFTParams->DstTexture = TmpRenderTarget->GetRenderTargetItem().UAV;
		ForwardFFTParams->SrcRect = SrcRect;
		ForwardFFTParams->DstRect = TmpRect;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TwoForOneRealFFTImage1D512x512"),
			*TwoForOneCS,
			ForwardFFTParams,
			FIntVector(512, 1, 1)
		);
	}

	GraphBuilder.Execute();
}
}; // namespace FFT2D

