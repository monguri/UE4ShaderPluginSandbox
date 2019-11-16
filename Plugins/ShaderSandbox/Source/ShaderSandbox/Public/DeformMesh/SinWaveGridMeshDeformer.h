#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "RHICommandList.h"

void SinWaveDeformGridMesh(FRHICommandListImmediate& RHICmdList, uint32 NumRow, uint32 NumColumn, uint32 NumVertex, float WaveNumberRow, float WaveNumberColumn, float Frequency, float Amplitude, float DeltaTime, class FRHIUnorderedAccessView* PositionVertexBufferUAV, class FRHIUnorderedAccessView* TangentVertexBufferUAV);
