#pragma once

#include "RenderResource.h"

struct FSphereCollisionParameters
{
	FVector RelativeCenter;
	float Radius;
};

struct FGridClothParameters
{
	static const uint32 MAX_SPHERE_COLLISION_PER_MESH = 4;

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
	uint32 NumSphereCollision = 0;

	FSphereCollisionParameters SphereCollisionParams[MAX_SPHERE_COLLISION_PER_MESH];
};

struct FClothParameterStructuredBuffer : public FRenderResource
{
public:
	FClothParameterStructuredBuffer(const TArray<FGridClothParameters>& ComponentsData);
	virtual ~FClothParameterStructuredBuffer();

	virtual void InitDynamicRHI() override;
	virtual void ReleaseDynamicRHI() override;

	int32 GetComponentsDataCount() const { return OriginalComponentsData.Num(); };

private:
	FStructuredBufferRHIRef ComponentsData;
	FShaderResourceViewRHIRef ComponentsDataSRV;
	TArray<FGridClothParameters> OriginalComponentsData;
};

