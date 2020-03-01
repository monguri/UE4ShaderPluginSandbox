#include "FFTTexture2DTestActor.h"
#include "Engine/Texture2D.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "RHICommandList.h"

void AFFTTexture2DTestActor::BeginPlay()
{
	if (SrcTexture == nullptr || DstTexture == nullptr)
	{
		return;
	}

	_DstUAV = RHICreateUnorderedAccessView(DstTexture->GameThread_GetRenderTargetResource()->TextureRHI);
	int32 Width, Height;
	DstTexture->GetSize(Width, Height);
	if (Width != 512 || Height != 512) // TODO:マジックナンバー
	{
		return;
	}

	ENQUEUE_RENDER_COMMAND(FFTTexture2DTestCmmand)(
		[this](FRHICommandListImmediate& RHICmdList)
		{
			if (_DstUAV.IsValid())
			{
				FFTTexture2D::DoFFTTexture2D512x512(RHICmdList, FFTMode, SrcTexture->Resource->TextureRHI, _DstUAV);
			}
		}
	);
}

void AFFTTexture2DTestActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (_DstUAV.IsValid())
	{
		_DstUAV.SafeRelease();
	}
}

