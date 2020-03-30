#include "FFTTexture2D.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHIStaticStates.h"
#include "RenderTargetPool.h"

namespace FFTTexture2D
{
class FHalfCompressedComplexFFTImage512x512 : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FHalfCompressedComplexFFTImage512x512);
	SHADER_USE_PARAMETER_STRUCT(FHalfCompressedComplexFFTImage512x512, FGlobalShader);

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

IMPLEMENT_GLOBAL_SHADER(FHalfCompressedComplexFFTImage512x512, "/Plugin/ShaderSandbox/Private/FFTTexture2D.usf", "HalfCompressedComplexFFTImage512x512", SF_Compute);

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

IMPLEMENT_GLOBAL_SHADER(FComplexFFTImage512x512, "/Plugin/ShaderSandbox/Private/FFTTexture2D.usf", "ComplexFFTImage512x512", SF_Compute);

void DoFFTTexture2D512x512(FRHICommandListImmediate& RHICmdList, EFFTMode Mode, const FTextureRHIRef& SrcTexture, FRHIUnorderedAccessView* DstUAV)
{
	const FIntRect& SrcRect = FIntRect(FIntPoint(0, 0), FIntPoint(512, 512));
	const uint32 FREQUENCY_PADDING = 2;
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
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, TmpRenderTarget, TEXT("FFTTexture2D Tmp Buffer"));

		TRefCountPtr<IPooledRenderTarget> TmpRenderTarget2;
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, TmpRenderTarget2, TEXT("FFTTexture2D Tmp Buffer2"));

		TShaderMapRef<FHalfCompressedComplexFFTImage512x512> HalfCompressedForwardFFTCS(ShaderMap);

		FHalfCompressedComplexFFTImage512x512::FParameters* HalfCompressedForwardFFTParams = GraphBuilder.AllocParameters<FHalfCompressedComplexFFTImage512x512::FParameters>();
		HalfCompressedForwardFFTParams->Forward = 1;
		HalfCompressedForwardFFTParams->SrcTexture = SrcTexture;
		HalfCompressedForwardFFTParams->SrcSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
		HalfCompressedForwardFFTParams->DstTexture = TmpRenderTarget->GetRenderTargetItem().UAV;
		//HalfCompressedForwardFFTParams->DstTexture = DstUAV;
		HalfCompressedForwardFFTParams->SrcRectMin = SrcRect.Min;
		HalfCompressedForwardFFTParams->SrcRectMax = SrcRect.Max;
		HalfCompressedForwardFFTParams->DstRect = TmpRect;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("HalfCompressedComplexFFTImage512x512ForwardHorizontal"),
			*HalfCompressedForwardFFTCS,
			HalfCompressedForwardFFTParams,
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

		TShaderMapRef<FHalfCompressedComplexFFTImage512x512> HalfCompressedInverseFFTCS(ShaderMap);

		FHalfCompressedComplexFFTImage512x512::FParameters* HalfCompressedInverseFFTParams = GraphBuilder.AllocParameters<FHalfCompressedComplexFFTImage512x512::FParameters>();
		HalfCompressedInverseFFTParams->Forward = 0;
		HalfCompressedInverseFFTParams->SrcTexture = TmpRenderTarget->GetRenderTargetItem().ShaderResourceTexture;
		HalfCompressedInverseFFTParams->SrcSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
		HalfCompressedInverseFFTParams->DstTexture = DstUAV;
		HalfCompressedInverseFFTParams->SrcRectMin = TmpRect.Min;
		HalfCompressedInverseFFTParams->SrcRectMax = TmpRect.Max;
		HalfCompressedInverseFFTParams->DstRect = DstRect;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("HalfCompressedComplexFFTImage512x512InverseHorizontal"),
			*HalfCompressedInverseFFTCS,
			HalfCompressedInverseFFTParams,
			FIntVector(512 + FREQUENCY_PADDING, 1, 1)
		);
	}

	GraphBuilder.Execute();
}
}; // namespace FFTTexture2D

