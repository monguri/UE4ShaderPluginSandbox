#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "RHICommandList.h"

struct FSphereCollisionParameters
{
	FVector RelativeCenter;
	float Radius;

	FSphereCollisionParameters(const FVector& RelativeCenterVec, float RadiusVal) : RelativeCenter(RelativeCenterVec), Radius(RadiusVal) {}
};

struct FGridClothParameters
{
	uint32 NumRow = 0;
	uint32 NumColumn = 0;
	uint32 NumVertex = 0;
	float GridWidth = 0.0f;
	float GridHeight = 0.0f;
	float DeltaTime = 0.0f;
	float Stiffness = 0.0f;
	float Damping = 0.0f;
	FVector WindVelocity;
	float FluidDensity = 0.0f;
	FVector PreviousInertia = FVector::ZeroVector;
	float VertexRadius = 0.0f;
	uint32 NumIteration = 0;

	class FRHIUnorderedAccessView* PositionVertexBufferUAV;
	class FRHIUnorderedAccessView* TangentVertexBufferUAV;
	class FRHIUnorderedAccessView* PrevPositionVertexBufferUAV;
	class FRHIShaderResourceView* AccelerationVertexBufferSRV;
	TArray<FSphereCollisionParameters> SphereCollisionParams;
};

struct FClothGridMeshDeformer
{
	void EnqueueDeformTask(const FGridClothParameters& Param);
	void FlushDeformTaskQueue(FRHICommandListImmediate& RHICmdList, FRHIUnorderedAccessView* WorkAccelerationVertexBufferUAV, FRHIUnorderedAccessView* WorkPrevVertexBufferUAV, FRHIUnorderedAccessView* WorkVertexBufferUAV);

	TArray<FGridClothParameters> DeformTaskQueue;
};

