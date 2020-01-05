#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "RHICommandList.h"
#include "ClothParameterStructuredBuffer.h"

struct FClothGridMeshDeformCommand
{
	FGridClothParameters Params;
	struct FClothVertexBuffers* VertexBuffers = nullptr;
};

struct FClothGridMeshDeformer
{
	void EnqueueDeformCommand(const FClothGridMeshDeformCommand& Command);
	void FlushDeformCommandQueue(FRHICommandListImmediate& RHICmdList, FRHIUnorderedAccessView* WorkAccelerationMoveVertexBufferUAV, FRHIUnorderedAccessView* WorkPrevVertexBufferUAV, FRHIUnorderedAccessView* WorkVertexBufferUAV);

	TArray<FClothGridMeshDeformCommand> DeformCommandQueue;
};

