#pragma once

#include "DeformableGridMeshComponent.h"
#include "ClothGridMeshComponent.generated.h"


// almost all is copy of UCustomMeshComponent
UCLASS(hidecategories=(Object,LOD, Physics, Collision), editinlinenew, meta=(BlueprintSpawnableComponent), ClassGroup=Rendering)
class SHADERSANDBOX_API UClothGridMeshComponent : public UDeformableGridMeshComponent
{
	GENERATED_BODY()

public:
	/** Set the geometry and vertex paintings to use on this triangle mesh as cloth. */
	UFUNCTION(BlueprintCallable, Category="Components|ClothGridMesh")
	void InitClothSettings(int32 NumRow, int32 NumColumn, float GridWidth, float GridHeight, float Stiffness, float Damping, float LinearDrag, float FluidDensity, float LiftCoefficient, float DragCoefficient, float VertexRadius, int32 NumIteration);

	/** Ignore veclocity discontiuity just next frame. */
	UFUNCTION(BlueprintCallable, Category="Components|ClothGridMesh")
	void IgnoreVelocityDiscontinuityNextFrame();

	virtual ~UClothGridMeshComponent();

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.

	const TArray<FVector>& GetAccelerationMoves() const { return _AccelerationMoves; }

protected:
	//~ Begin UActorComponent Interface
	virtual void SendRenderDynamicData_Concurrent() override;
	//~ End UActorComponent Interface

private:
	bool _IgnoreVelocityDiscontinuityNextFrame = false;

	TArray<FVector> _AccelerationMoves;
	float _LogStiffness;
	float _LogDamping;
	float _LinearLogDrag;
	float _FluidDensity;
	float _LiftLogCoefficient;
	float _DragLogCoefficient;
	FVector _PreviousInertia;
	float _VertexRadius;
	int32 _NumIteration;

	// variables to cache previous frame world location to calculate linear velocities.
	FVector _PrevLocation;

	// variables to cache linear velocities every frame to calculate accelerations and previous frame inertia.
	FVector _CurLinearVelocity;
	FVector _PrevLinearVelocity;

	void MakeDeformCommand(struct FClothGridMeshDeformCommand& Command);
};

