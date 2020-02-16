#include "FFT2DTestActor.h"
#include "Engine/Texture2D.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "RHICommandList.h"

void AFFT2DTestActor::BeginPlay()
{
	if (SrcTexture != nullptr)
	{
		uint8 MipLevel = 0;
		_SrcSRV = RHICreateShaderResourceView(SrcTexture->TextureReference.TextureReferenceRHI->GetReferencedTexture(), MipLevel);
	}

	if (DstTexture != nullptr)
	{
		_DstUAV = RHICreateUnorderedAccessView(DstTexture->GameThread_GetRenderTargetResource()->TextureRHI);
	}

	ENQUEUE_RENDER_COMMAND(FFT2DTestCmmand)(
		[this](FRHICommandListImmediate& RHICmdList)
		{
			if (_SrcSRV.IsValid() && _DstUAV.IsValid())
			{
				FFT2D::DoFFT2D(RHICmdList, FFTMode, _SrcSRV, _DstUAV);
			}
		}
	);
}

void AFFT2DTestActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (_SrcSRV.IsValid())
	{
		_SrcSRV.SafeRelease();
	}

	if (_DstUAV.IsValid())
	{
		_DstUAV.SafeRelease();
	}
}

