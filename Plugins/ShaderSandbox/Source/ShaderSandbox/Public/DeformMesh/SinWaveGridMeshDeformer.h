#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "RHICommandList.h"

void SinWaveDeformGridMesh(FRHICommandListImmediate& RHICmdList, uint32 NumRow, uint32 NumColumn, uint32 NumVertex, class FRHIUnorderedAccessView* PositionVertexBufferUAV, class FRHIUnorderedAccessView* TangentVertexBufferUAV);
