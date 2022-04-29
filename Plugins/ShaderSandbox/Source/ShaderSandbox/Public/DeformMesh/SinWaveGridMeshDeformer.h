#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "RHICommandList.h"

struct FGridSinWaveParameters
{
	uint32 NumRow;
	uint32 NumColumn;
	uint32 NumVertex;
	float GridWidth;
	float GridHeight;
	float WaveLengthRow;
	float WaveLengthColumn;
	float Period;
	float Amplitude;
	float AccumulatedTime;
	bool bAsync;
};

void SinWaveDeformGridMesh(FRHICommandListImmediate& RHICmdList, const FGridSinWaveParameters& GridSinWaveParams, class FRHIUnorderedAccessView* PositionVertexBufferUAV, class FRHIUnorderedAccessView* TangentVertexBufferUAV
#if 0
#if RHI_RAYTRACING
	, const FRayTracingGeometry& RayTracingGeomt
#endif
#endif
);
