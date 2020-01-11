#pragma once

// StructuredBuffer用の構造体は128bit（16Byte）単位でないとデバイスロストやおかしな挙動になるので注意すること
struct FGridClothParameters
{
	// TODO:この構造体は、この定数を含めていろんなところで使うのでこのヘッダから出すべき
	// あるいはこのヘッダとcppをまるごとClothVertexBuffers.h/cppに入れるか
	static const FVector GRAVITY;
	static const uint32 MAX_SPHERE_COLLISION_PER_MESH = 16;
	static const float BASE_FREQUENCY;

	uint32 NumIteration = 0;
	uint32 NumRow = 0;
	uint32 NumColumn = 0;
	uint32 VertexIndexOffset = 0;
	uint32 NumVertex = 0;
	float GridWidth = 0.0f;
	float GridHeight = 0.0f;
	float Stiffness = 0.0f;
	float Damping = 0.0f;
	FVector PreviousInertia = FVector::ZeroVector;
	FVector WindVelocity = FVector::ZeroVector;
	float FluidDensity = 0.0f;
	float LiftCoefficient = 0.0f;
	float DragCoefficient = 0.0f;
	float IterDeltaTime = 0.0f;
	float VertexRadius = 0.0f;
	FVector AlignmentDummy = FVector::ZeroVector;
	uint32 NumSphereCollision = 0;
	// xyz : RelativeCenter, w : Radius;
	FVector4 SphereCollisionParams[MAX_SPHERE_COLLISION_PER_MESH];
};
