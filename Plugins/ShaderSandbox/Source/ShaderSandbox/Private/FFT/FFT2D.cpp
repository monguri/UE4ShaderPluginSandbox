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
class FTwoForOneRealFFTImage512x512 : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTwoForOneRealFFTImage512x512);
	SHADER_USE_PARAMETER_STRUCT(FTwoForOneRealFFTImage512x512, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, Forward)
		SHADER_PARAMETER(FIntPoint, SrcRectMin)
		SHADER_PARAMETER(FIntPoint, SrcRectMax)
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

IMPLEMENT_GLOBAL_SHADER(FTwoForOneRealFFTImage512x512, "/Plugin/ShaderSandbox/Private/FFT.usf", "TwoForOneRealFFTImage512x512", SF_Compute);

class FComplexFFTImage512x512 : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FComplexFFTImage512x512);
	SHADER_USE_PARAMETER_STRUCT(FComplexFFTImage512x512, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, Forward)
		SHADER_PARAMETER(FIntPoint, SrcRectMin)
		SHADER_PARAMETER(FIntPoint, SrcRectMax)
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

IMPLEMENT_GLOBAL_SHADER(FComplexFFTImage512x512, "/Plugin/ShaderSandbox/Private/FFT.usf", "ComplexFFTImage512x512", SF_Compute);

void DoFFT2D512x512(FRHICommandListImmediate& RHICmdList, EFFTMode Mode, const FTextureRHIRef& SrcTexture, FRHIUnorderedAccessView* DstUAV)
{
	const FIntRect& SrcRect = FIntRect(FIntPoint(0, 0), FIntPoint(512, 512));
	const uint32 FREQUENCY_PADDING = 2; //TODO:óùã¸ÇóùâÇ≈Ç´ÇƒÇ»Ç¢
	const FIntPoint& TmpBufferSize = FIntPoint(512 + FREQUENCY_PADDING, 512);
	const FIntRect& TmpRect = FIntRect(FIntPoint(0, 0), TmpBufferSize);
	const FIntRect& DstRect = SrcRect;

	const FIntPoint& TmpBufferSize2 = FIntPoint(512, 512);
	const FIntRect& TmpRect2 = SrcRect;

	FRDGBuilder GraphBuilder(RHICmdList);

	TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);

	{
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

		TRefCountPtr<IPooledRenderTarget> TmpRenderTarget2;
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, TmpRenderTarget2, TEXT("FFT2D Tmp Buffer2"));

		TShaderMapRef<FTwoForOneRealFFTImage512x512> TwoForOneForwardFFTCS(ShaderMap);

		FTwoForOneRealFFTImage512x512::FParameters* TwoForOneForwardFFTParams = GraphBuilder.AllocParameters<FTwoForOneRealFFTImage512x512::FParameters>();
		TwoForOneForwardFFTParams->Forward = 1;
		TwoForOneForwardFFTParams->SrcTexture = SrcTexture;
		TwoForOneForwardFFTParams->SrcSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
		TwoForOneForwardFFTParams->DstTexture = TmpRenderTarget->GetRenderTargetItem().UAV;
		//TwoForOneForwardFFTParams->DstTexture = DstUAV;
		TwoForOneForwardFFTParams->SrcRectMin = SrcRect.Min;
		TwoForOneForwardFFTParams->SrcRectMax = SrcRect.Max;
		TwoForOneForwardFFTParams->DstRect = TmpRect;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TwoForOneRealFFTImage512x512ForwardHorizontal"),
			*TwoForOneForwardFFTCS,
			TwoForOneForwardFFTParams,
			FIntVector(512, 1, 1)
		);

		TShaderMapRef<FComplexFFTImage512x512> ComplexForwardFFTCS(ShaderMap);

		FComplexFFTImage512x512::FParameters* ComplexForwardFFTParams = GraphBuilder.AllocParameters<FComplexFFTImage512x512::FParameters>();
		ComplexForwardFFTParams->Forward = 1;
		ComplexForwardFFTParams->SrcTexture = TmpRenderTarget->GetRenderTargetItem().ShaderResourceTexture;
		ComplexForwardFFTParams->SrcSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
		ComplexForwardFFTParams->DstTexture = TmpRenderTarget2->GetRenderTargetItem().UAV;
		ComplexForwardFFTParams->SrcRectMin = TmpRect.Min;
		ComplexForwardFFTParams->SrcRectMax = TmpRect.Max;
		ComplexForwardFFTParams->DstRect = TmpRect2;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ComplexFFTImage512x512ForwardVertical"),
			*ComplexForwardFFTCS,
			ComplexForwardFFTParams,
			FIntVector(512 + FREQUENCY_PADDING, 1, 1)
		);


		TShaderMapRef<FComplexFFTImage512x512> ComplexInverseFFTCS(ShaderMap);

		FComplexFFTImage512x512::FParameters* ComplexInverseFFTParams = GraphBuilder.AllocParameters<FComplexFFTImage512x512::FParameters>();
		ComplexInverseFFTParams->Forward = 0;
		ComplexInverseFFTParams->SrcTexture = TmpRenderTarget2->GetRenderTargetItem().ShaderResourceTexture;
		ComplexInverseFFTParams->SrcSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
		ComplexInverseFFTParams->DstTexture = TmpRenderTarget->GetRenderTargetItem().UAV;
		ComplexInverseFFTParams->SrcRectMin = TmpRect2.Min;
		ComplexInverseFFTParams->SrcRectMax = TmpRect2.Max;
		ComplexInverseFFTParams->DstRect = TmpRect;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ComplexFFTImage512x512InverseHorizontal"),
			*ComplexInverseFFTCS,
			ComplexInverseFFTParams,
			FIntVector(512 + FREQUENCY_PADDING, 1, 1)
		);

		TShaderMapRef<FTwoForOneRealFFTImage512x512> TwoForOneInverseFFTCS(ShaderMap);

		FTwoForOneRealFFTImage512x512::FParameters* TwoForOneInverseFFTParams = GraphBuilder.AllocParameters<FTwoForOneRealFFTImage512x512::FParameters>();
		TwoForOneInverseFFTParams->Forward = 0;
		TwoForOneInverseFFTParams->SrcTexture = TmpRenderTarget->GetRenderTargetItem().ShaderResourceTexture;
		TwoForOneInverseFFTParams->SrcSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
		TwoForOneInverseFFTParams->DstTexture = DstUAV;
		TwoForOneInverseFFTParams->SrcRectMin = TmpRect.Min;
		TwoForOneInverseFFTParams->SrcRectMax = TmpRect.Max;
		TwoForOneInverseFFTParams->DstRect = DstRect;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TwoForOneRealFFTImage512x512InverseHorizontal"),
			*TwoForOneInverseFFTCS,
			TwoForOneInverseFFTParams,
			FIntVector(512 + FREQUENCY_PADDING, 1, 1)
		);
	}

	GraphBuilder.Execute();
}
}; // namespace FFT2D

