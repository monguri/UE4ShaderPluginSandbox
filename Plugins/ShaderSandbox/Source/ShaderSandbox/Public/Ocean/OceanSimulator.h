#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "RHICommandList.h"

/** Phillips spectrum configuration */
struct FOceanSpectrumParameters
{
	/** The size of displacement map. Must be power of 2. */
	uint32 DispMapDimension = 512;
	/** The side length (world space) of square patch. Typical value is 1000 ~ 2000. */
	float PatchLength = 1000.0f;
	/** Adjust the time interval for simulation (controls the simulation speed) */
	float TimeScale = 0.8f;
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
};

void SimulateOcean(FRHICommandListImmediate& RHICmdList, const FOceanSpectrumParameters& Params, FRHIShaderResourceView* H0SRV, FRHIShaderResourceView* OmegaSRV, FRHIUnorderedAccessView* HtUAV, FRHIUnorderedAccessView* DisplacementMapUAV, FRHIUnorderedAccessView* H0DebugViewUAV);

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


