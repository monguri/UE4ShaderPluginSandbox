#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "RHICommandList.h"

void SinWaveDeformMesh(FRHICommandListImmediate& RHICmdList, uint32 NumVertex, class FRHIUnorderedAccessView* PositionVertexBufferUAV);
