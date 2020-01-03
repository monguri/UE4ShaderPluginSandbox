#pragma once

#include "RenderResource.h"

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

	TArray<FSphereCollisionParameters> SphereCollisionParams;
};

struct FClothParameterStructuredBuffer : public FRenderResource
{
};

