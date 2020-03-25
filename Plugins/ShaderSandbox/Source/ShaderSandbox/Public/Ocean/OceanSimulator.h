#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "RHICommandList.h"

typedef FVector2D FComplex;

/** Phillips spectrum configuration */
struct FOceanSpectrumParameters
{
	/** The size of displacement map. Must be power of 2. */
	uint32 DispMapDimension = 512;
	/** The side length (world space) of square patch. Typical value is 1000 ~ 2000. */
	float PatchLength = 2000.0f;
	/** Amplitude for transverse wave. Around 1.0 (not the world space height). */
	float AmplitudeScale = 0.35f;
	/** Wind direction. Normalization not required */
	FVector2D WindDirection = FVector2D(0.8f, 0.6f);
	/** The bigger the wind speed, the larger scale of wave crest. But the wave scale  can be no larger than PatchLength. Around 100 ~ 1000 */
	float WindSpeed = 600.0f;
	/** This value damps out the waves against the wind direction. Smaller value means  higher wind dependency. */
	float WindDependency = 0.07f;
	/** The amplitude for longitudinal wave. Higher value creates pointy crests. Must  be positive. */
	float ChoppyScale = 1.3f;

	float AccumulatedTime = 0.0f;

	/** Map Dxyz value to [0, 1] by (Dxyz + ThisAmplitude) / (2 * Amplitude). */
	float DxyzDebugAmplitude = 100.0f;
};

struct FOceanBufferViews
{
	FRHIShaderResourceView* H0SRV = nullptr;
	FRHIShaderResourceView* OmegaSRV = nullptr;
	FRHIShaderResourceView* HtSRV = nullptr;
	FRHIUnorderedAccessView* HtUAV = nullptr;
	FRHIShaderResourceView* DkxSRV = nullptr;
	FRHIUnorderedAccessView* DkxUAV = nullptr;
	FRHIShaderResourceView* DkySRV = nullptr;
	FRHIUnorderedAccessView* DkyUAV = nullptr;
	FRHIUnorderedAccessView* FFTWorkUAV = nullptr;
	FRHIShaderResourceView* DxSRV = nullptr;
	FRHIUnorderedAccessView* DxUAV = nullptr;
	FRHIShaderResourceView* DySRV = nullptr;
	FRHIUnorderedAccessView* DyUAV = nullptr;
	FRHIShaderResourceView* DzSRV = nullptr;
	FRHIUnorderedAccessView* DzUAV = nullptr;
	FRHIUnorderedAccessView* H0DebugViewUAV = nullptr;
	FRHIUnorderedAccessView* HtDebugViewUAV = nullptr;
	FRHIUnorderedAccessView* DkxDebugViewUAV = nullptr;
	FRHIUnorderedAccessView* DkyDebugViewUAV = nullptr;
	FRHIUnorderedAccessView* DxyzDebugViewUAV = nullptr;
	FRHIShaderResourceView* DisplacementMapSRV = nullptr;
	FRHIUnorderedAccessView* DisplacementMapUAV = nullptr;
	FRHIUnorderedAccessView* GradientFoldingMapUAV = nullptr;
};

void SimulateOcean(FRHICommandListImmediate& RHICmdList, const FOceanSpectrumParameters& Params, const FOceanBufferViews& Views);

struct FOceanSinWaveParameters
{
	uint32 MapWidth;
	uint32 MapHeight;
	float MeshWidth;
	float MeshHeight;
	float WaveLengthRow;
	float WaveLengthColumn;
	float Period;
	float Amplitude;
	float AccumulatedTime;
};

void TestSinWave(FRHICommandListImmediate& RHICmdList, const FOceanSinWaveParameters& Params, FUnorderedAccessViewRHIRef DisplacementMapUAV);


