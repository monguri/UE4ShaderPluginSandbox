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
	uint32 NumRow;
	uint32 NumColumn;
	uint32 NumVertex;
	float GridWidth;
	float GridHeight;
	float DeltaTime;
	float Stiffness;
	float Damping;
	uint32 NumIteration;
	TArray<FSphereCollisionParameters> SphereCollisionParams;
};

void ClothSimulationGridMesh(FRHICommandListImmediate& RHICmdList, const FGridClothParameters& GridClothParams, class FRHIUnorderedAccessView* PositionVertexBufferUAV, class FRHIUnorderedAccessView* TangentVertexBufferUAV, class FRHIUnorderedAccessView* PrevPositionVertexBufferUAV, class FRHIShaderResourceView* AccelerationVertexBufferSRV);
