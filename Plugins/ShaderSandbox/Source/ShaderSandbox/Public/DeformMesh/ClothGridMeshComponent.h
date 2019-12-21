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
	void InitClothSettings(int32 NumRow, int32 NumColumn, float GridWidth, float GridHeight, float Stiffness, float Damping, float VertexRadius, int32 NumIteration);

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.

	const TArray<FVector>& GetAccelerations() const { return _Accelerations; }
	float GetStiffness() const { return _Stiffness; }
	float GetDamping() const { return _Damping; }
	float GetVertexRadius() const { return _VertexRadius; }
	int32 GetNumIteration() const { return _NumIteration; }

protected:
	virtual void SendRenderDynamicData_Concurrent() override;

private:
	TArray<FVector> _Accelerations;
	float _Stiffness;
	float _Damping;
	float _VertexRadius;
	int32 _NumIteration;
};
