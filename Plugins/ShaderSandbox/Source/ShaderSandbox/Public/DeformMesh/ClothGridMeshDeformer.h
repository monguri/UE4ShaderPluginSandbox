#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "RHICommandList.h"

struct FGridClothParameters
{
	uint32 NumRow;
	uint32 NumColumn;
	uint32 NumVertex;
	float GridWidth;
	float GridHeight;
	float AccumulatedTime;
};

void ClothSimulationGridMesh(FRHICommandListImmediate& RHICmdList, const FGridClothParameters& GridClothParams, class FRHIUnorderedAccessView* PositionVertexBufferUAV, class FRHIUnorderedAccessView* TangentVertexBufferUAV, class FRHIUnorderedAccessView* PrevPositionVertexBufferUAV, class FRHIShaderResourceView* AccelerationVertexBufferSRV);
