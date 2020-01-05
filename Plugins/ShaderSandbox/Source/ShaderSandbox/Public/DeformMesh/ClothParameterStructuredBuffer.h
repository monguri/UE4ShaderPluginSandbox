#pragma once

#include "RenderResource.h"

struct FGridClothParameters
{
	static const uint32 MAX_SPHERE_COLLISION_PER_MESH = 16;
	static const float BASE_FREQUENCY;

	uint32 NumIteration = 0;
	uint32 NumRow = 0;
	uint32 NumColumn = 0;
	uint32 VertexIndexOffset = 0;
	uint32 NumVertex = 0;
	float GridWidth = 0.0f;
	float GridHeight = 0.0f;
	float SqrIterDeltaTime = 0.0f;
	float Stiffness = 0.0f;
	float Damping = 0.0f;
	FVector PreviousInertiaMove = FVector::ZeroVector;
	FVector WindVelocity = FVector::ZeroVector;
	float FluidDensity = 0.0f;
	float IterDeltaTime = 0.0f;
	float VertexRadius = 0.0f;
	uint32 NumSphereCollision = 0;
	// xyz : RelativeCenter, w : Radius;
	FVector4 SphereCollisionParams[MAX_SPHERE_COLLISION_PER_MESH];
};

struct FClothParameterStructuredBuffer : public FRenderResource
{
public:
	FClothParameterStructuredBuffer(const TArray<FGridClothParameters>& ComponentsData);
	virtual ~FClothParameterStructuredBuffer();

	virtual void InitDynamicRHI() override;
	virtual void ReleaseDynamicRHI() override;

	int32 GetComponentsDataCount() const { return OriginalComponentsData.Num(); };
	FRHIShaderResourceView* GetSRV() const { return ComponentsDataSRV; }

private:
	FStructuredBufferRHIRef ComponentsData;
	FShaderResourceViewRHIRef ComponentsDataSRV;
	TArray<FGridClothParameters> OriginalComponentsData;
};

