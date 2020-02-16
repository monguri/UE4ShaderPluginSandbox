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

namespace FFT2D
{
	void DoFFT2D(FRHICommandListImmediate& RHICmdList, EFFTMode Mode, const FRHIShaderResourceView* SrcSRV, FRHIUnorderedAccessView* DstUAV);
}; // namespace FFT2D

