#pragma once

#include "CoreMinimal.h"
#include "RHIResources.h"
#include "RHICommandList.h"

UENUM()
enum class EFFTMode : uint8
{
	Forward = 0,
	Inverse,
	ForwardAndInverse,
	InverseAndForward,
};

namespace FFTTexture2D
{
	void DoFFTTexture2D512x512(FRHICommandListImmediate& RHICmdList, EFFTMode Mode, const FTextureRHIRef& SrcTexture, FRHIUnorderedAccessView* DstUAV);
}; // namespace FFTTexture2D

