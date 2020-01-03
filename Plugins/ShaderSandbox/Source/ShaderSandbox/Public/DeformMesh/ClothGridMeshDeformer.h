#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "RHICommandList.h"
#include "ClothParameterStructuredBuffer.h"

struct FClothGridMeshDeformCommand
{
	FGridClothParameters Params;

	class FRHIUnorderedAccessView* PositionVertexBufferUAV;
	class FRHIUnorderedAccessView* TangentVertexBufferUAV;
	class FRHIUnorderedAccessView* PrevPositionVertexBufferUAV;
	class FRHIShaderResourceView* AccelerationVertexBufferSRV;
};

struct FClothGridMeshDeformer
{
	void EnqueueDeformCommand(const FClothGridMeshDeformCommand& Command);
	void FlushDeformCommandQueue(FRHICommandListImmediate& RHICmdList, FRHIUnorderedAccessView* WorkAccelerationVertexBufferUAV, FRHIUnorderedAccessView* WorkPrevVertexBufferUAV, FRHIUnorderedAccessView* WorkVertexBufferUAV);

	TArray<FClothGridMeshDeformCommand> DeformCommandQueue;
};

