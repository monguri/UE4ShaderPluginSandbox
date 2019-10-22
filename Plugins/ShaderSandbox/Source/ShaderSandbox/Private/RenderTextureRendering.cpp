#include "RenderTextureRendering.h"
#include "GlobalShader.h"
#include "RHICommandList.h"
#include "RenderingThread.h"
#include "TextureResource.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "PipelineStateCache.h"
#include "RHIStaticStates.h"

class FRenderTextureVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRenderTextureVS);

public:
	FRenderTextureVS() = default;

	FRenderTextureVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM4);
	}

#if 0 // 今は不要。
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}
#endif
};

class FRenderTexturePS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRenderTexturePS);

public:
	FRenderTexturePS() = default;

	FRenderTexturePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM4);
	}

#if 0 // 今は不要。
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}
#endif
};

IMPLEMENT_GLOBAL_SHADER(FRenderTextureVS, "/Plugin/ShaderSandbox/Private/RenderTexture.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FRenderTexturePS, "/Plugin/ShaderSandbox/Private/RenderTexture.usf", "MainPS", SF_Pixel);

class FRenderTextureCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRenderTextureCS);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	FRenderTextureCS() = default;
	FRenderTextureCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		OutTexture.Bind(Initializer.ParameterMap, TEXT("OutTexture"));
		OutTextureSampler.Bind(Initializer.ParameterMap, TEXT("OutTextureSampler"));
	}

#if 0
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), 8);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), 8);
	}
#endif

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdated = FGlobalShader::Serialize(Ar);
		Ar << OutTexture;
		Ar << OutTextureSampler;
		return bShaderHasOutdated;
	}

	void BindOutputUAV(FRHICommandList& RHICmdList, FRHIUnorderedAccessView* OutputUAV)
	{
		FRHIComputeShader* ComputeShaderRHI = GetComputeShader();
		if (OutTexture.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutTexture.GetBaseIndex(), OutputUAV);
		}
	}

	void UnbindOutputUAV(FRHICommandList& RHICmdList)
	{
		FRHIComputeShader* ComputeShaderRHI = GetComputeShader();
		if (OutTexture.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutTexture.GetBaseIndex(), nullptr);
		}
	}

	FShaderResourceParameter OutTexture;
	FShaderResourceParameter OutTextureSampler;
};

IMPLEMENT_GLOBAL_SHADER(FRenderTextureCS, "/Plugin/ShaderSandbox/Private/RenderTexture.usf", "MainCS", SF_Compute);

namespace
{
void DrawRenderTextureVSPS_RenderThread(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, FTextureRenderTargetResource& OutTextureRenderTargetResource)
{
	check(IsInRenderingThread());

	FRHITexture2D* RenderTargetTexture = OutTextureRenderTargetResource.GetRenderTargetTexture();

	RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, RenderTargetTexture);

	FRHIRenderPassInfo RenderPathInfo(RenderTargetTexture, ERenderTargetActions::DontLoad_Store, OutTextureRenderTargetResource.TextureRHI);

	RHICmdList.BeginRenderPass(RenderPathInfo, TEXT("DrawRenderTexture"));
	{
		RHICmdList.SetViewport(0, 0, 0.0f, OutTextureRenderTargetResource.GetSizeX(), OutTextureRenderTargetResource.GetSizeY(), 1.0f);

		TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);
		TShaderMapRef<FRenderTextureVS> VertexShader(GlobalShaderMap);
		TShaderMapRef<FRenderTexturePS> PixelShader(GlobalShaderMap);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, ECompareFunction::CF_Always>::GetRHI();
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.PrimitiveType = EPrimitiveType::PT_TriangleList;
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

		int32 BaseVertexIndex = 0;
		int32 NumPrimitives = 2;
		int32 NumInstances = 1;
		RHICmdList.DrawPrimitive(BaseVertexIndex, NumPrimitives, NumInstances);
	}
	RHICmdList.EndRenderPass();
}

void DrawRenderTextureCS_RenderThread(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, uint32 SizeX, uint32 SizeY, FUnorderedAccessViewRHIRef OutUAVRef)
{
	check(IsInRenderingThread());

	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, OutUAVRef);

	//RHICmdList.BeginRenderPass(RenderPathInfo, TEXT("DrawRenderTexture"));
	{
		TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);
		TShaderMapRef<FRenderTextureCS> RenderTextureCS(GlobalShaderMap);

		RHICmdList.SetComputeShader(RenderTextureCS->GetComputeShader());
		RenderTextureCS->BindOutputUAV(RHICmdList, OutUAVRef);
		RHICmdList.DispatchComputeShader(FMath::DivideAndRoundUp(SizeX, (uint32)8), FMath::DivideAndRoundUp(SizeY, (uint32)8), 1);
		RenderTextureCS->UnbindOutputUAV(RHICmdList);
	}
	//RHICmdList.EndRenderPass();

	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, OutUAVRef);
}
} // namespace

void FRenderTextureRendering::Initialize(ERHIFeatureLevel::Type FeatureLevel, UTextureRenderTarget2D* OutputRenderTarget, class UCanvasRenderTarget2D* OutputCanvasTarget)
{
	_FeatureLevel = FeatureLevel;
	_OutputRenderTarget = OutputRenderTarget;
	_OutputCanvasTarget = OutputCanvasTarget;

	if (FeatureLevel >= ERHIFeatureLevel::Type::SM5)
	{
		_OutputCanvasUAV = RHICreateUnorderedAccessView(_OutputCanvasTarget->GameThread_GetRenderTargetResource()->TextureRHI);
	}
}

void FRenderTextureRendering::Finalize()
{
	if (_OutputCanvasUAV.IsValid())
	{
		_OutputCanvasUAV.SafeRelease();
	}
}

void FRenderTextureRendering::DrawRenderTextureVSPS()
{
	if (_OutputRenderTarget == nullptr)
	{
		return;
	}

	FTextureRenderTargetResource* TextureRenderTargetResource = _OutputRenderTarget->GameThread_GetRenderTargetResource();
	if (TextureRenderTargetResource == nullptr)
	{
		return;
	}

	ENQUEUE_RENDER_COMMAND(RenderTextureCommand)(
		[this, TextureRenderTargetResource](FRHICommandListImmediate& RHICmdList)
		{
			DrawRenderTextureVSPS_RenderThread(RHICmdList, _FeatureLevel, *TextureRenderTargetResource);
		}
	);
}

void FRenderTextureRendering::DrawRenderTextureCS()
{
	if (_OutputCanvasTarget == nullptr)
	{
		return;
	}

	FTextureRenderTargetResource* TextureRenderTargetResource = _OutputCanvasTarget->GameThread_GetRenderTargetResource();
	if (TextureRenderTargetResource == nullptr)
	{
		return;
	}

	ENQUEUE_RENDER_COMMAND(RenderCanvasCommand)(
		[this, TextureRenderTargetResource](FRHICommandListImmediate& RHICmdList)
		{
			DrawRenderTextureCS_RenderThread(RHICmdList, _FeatureLevel, TextureRenderTargetResource->GetSizeX(), TextureRenderTargetResource->GetSizeY(), _OutputCanvasUAV);
		}
	);
}
