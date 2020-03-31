#include "FFTTexture2D.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHIStaticStates.h"
#include "RenderTargetPool.h"

namespace FFTTexture2D
{
class FHalfPackFFTTexture2DHorizontal : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FHalfPackFFTTexture2DHorizontal);
	SHADER_USE_PARAMETER_STRUCT(FHalfPackFFTTexture2DHorizontal, FGlobalShader);

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

IMPLEMENT_GLOBAL_SHADER(FHalfPackFFTTexture2DHorizontal, "/Plugin/ShaderSandbox/Private/FFTTexture2D.usf", "HalfPackFFTTexture2DHorizontal", SF_Compute);

class FFFTTexture2DVertical : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FFFTTexture2DVertical);
	SHADER_USE_PARAMETER_STRUCT(FFFTTexture2DVertical, FGlobalShader);

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

IMPLEMENT_GLOBAL_SHADER(FFFTTexture2DVertical, "/Plugin/ShaderSandbox/Private/FFTTexture2D.usf", "FFTTexture2DVertical", SF_Compute);

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

		TShaderMapRef<FHalfPackFFTTexture2DHorizontal> HalfPackForwardFFTCS(ShaderMap);

		FHalfPackFFTTexture2DHorizontal::FParameters* HalfPackForwardFFTParams = GraphBuilder.AllocParameters<FHalfPackFFTTexture2DHorizontal::FParameters>();
		HalfPackForwardFFTParams->Forward = 1;
		HalfPackForwardFFTParams->SrcTexture = SrcTexture;
		HalfPackForwardFFTParams->SrcSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
		HalfPackForwardFFTParams->DstTexture = TmpRenderTarget->GetRenderTargetItem().UAV;
		//HalfPackForwardFFTParams->DstTexture = DstUAV;
		HalfPackForwardFFTParams->SrcRectMin = SrcRect.Min;
		HalfPackForwardFFTParams->SrcRectMax = SrcRect.Max;
		HalfPackForwardFFTParams->DstRect = TmpRect;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("HalfPackForwardFFTTexture2DHorizontal"),
			*HalfPackForwardFFTCS,
			HalfPackForwardFFTParams,
			FIntVector(512, 1, 1)
		);

		TShaderMapRef<FFFTTexture2DVertical> ForwardFFTCS(ShaderMap);

		FFFTTexture2DVertical::FParameters* ForwardFFTParams = GraphBuilder.AllocParameters<FFFTTexture2DVertical::FParameters>();
		ForwardFFTParams->Forward = 1;
		ForwardFFTParams->SrcTexture = TmpRenderTarget->GetRenderTargetItem().ShaderResourceTexture;
		ForwardFFTParams->SrcSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
		ForwardFFTParams->DstTexture = TmpRenderTarget2->GetRenderTargetItem().UAV;
		ForwardFFTParams->SrcRectMin = TmpRect.Min;
		ForwardFFTParams->SrcRectMax = TmpRect.Max;
		ForwardFFTParams->DstRect = TmpRect2;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ForwardFFTTexture2DVertical"),
			*ForwardFFTCS,
			ForwardFFTParams,
			FIntVector(512 + FREQUENCY_PADDING, 1, 1)
		);


		TShaderMapRef<FFFTTexture2DVertical> InverseFFTCS(ShaderMap);

		FFFTTexture2DVertical::FParameters* InverseFFTParams = GraphBuilder.AllocParameters<FFFTTexture2DVertical::FParameters>();
		InverseFFTParams->Forward = 0;
		InverseFFTParams->SrcTexture = TmpRenderTarget2->GetRenderTargetItem().ShaderResourceTexture;
		InverseFFTParams->SrcSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
		InverseFFTParams->DstTexture = TmpRenderTarget->GetRenderTargetItem().UAV;
		InverseFFTParams->SrcRectMin = TmpRect2.Min;
		InverseFFTParams->SrcRectMax = TmpRect2.Max;
		InverseFFTParams->DstRect = TmpRect;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("InverseFFTTexture2DVertical"),
			*InverseFFTCS,
			InverseFFTParams,
			FIntVector(512 + FREQUENCY_PADDING, 1, 1)
		);

		TShaderMapRef<FHalfPackFFTTexture2DHorizontal> HalfPackInverseFFTCS(ShaderMap);

		FHalfPackFFTTexture2DHorizontal::FParameters* HalfPackInverseFFTParams = GraphBuilder.AllocParameters<FHalfPackFFTTexture2DHorizontal::FParameters>();
		HalfPackInverseFFTParams->Forward = 0;
		HalfPackInverseFFTParams->SrcTexture = TmpRenderTarget->GetRenderTargetItem().ShaderResourceTexture;
		HalfPackInverseFFTParams->SrcSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
		HalfPackInverseFFTParams->DstTexture = DstUAV;
		HalfPackInverseFFTParams->SrcRectMin = TmpRect.Min;
		HalfPackInverseFFTParams->SrcRectMax = TmpRect.Max;
		HalfPackInverseFFTParams->DstRect = DstRect;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("HalfPackInverseFFTTexture2DHorizontal"),
			*HalfPackInverseFFTCS,
			HalfPackInverseFFTParams,
			FIntVector(512 + FREQUENCY_PADDING, 1, 1)
		);
	}

	GraphBuilder.Execute();
}
}; // namespace FFTTexture2D

