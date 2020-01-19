#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "RHICommandList.h"

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

void SimulateOcean(FRHICommandListImmediate& RHICmdList, const FOceanSinWaveParameters& Params, FUnorderedAccessViewRHIRef DisplacementMapUAV);

