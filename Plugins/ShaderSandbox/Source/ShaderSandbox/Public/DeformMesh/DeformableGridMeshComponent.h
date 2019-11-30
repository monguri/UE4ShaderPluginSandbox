#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/MeshComponent.h"
#include "DeformableGridMeshComponent.generated.h"


// almost all is copy of UCustomMeshComponent
UCLASS(hidecategories=(Object,LOD, Physics, Collision), editinlinenew, meta=(BlueprintSpawnableComponent), ClassGroup=Rendering)
class SHADERSANDBOX_API UDeformableGridMeshComponent : public UMeshComponent
{
	GENERATED_BODY()

public:
	/** Set the geometry to use on this triangle mesh */
	UFUNCTION(BlueprintCallable, Category="Components|DeformableGridMesh")
	void InitGridMeshSetting(int32 NumRow, int32 NumColumn, float GridWidth, float GridHeight);

	uint32 GetNumRow() const { return _NumRow; }
	uint32 GetNumColumn() const { return _NumColumn; }
	const TArray<FVector>& GetVertices() const { return _Vertices; }
	const TArray<uint32>& GetIndices() const { return _Indices; }
	float GetGridWidth() const { return _GridWidth; }
	float GetGridHeight() const { return _GridHeight; }

	float GetAccumulatedTime() const { return _AccumulatedTime; }

public:
	UDeformableGridMeshComponent();

	//~ Begin UMeshComponent Interface.
	virtual int32 GetNumMaterials() const override;
	//~ End UMeshComponent Interface.

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ Begin USceneComponent Interface.

protected:
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	uint32 _NumRow = 10;
	uint32 _NumColumn = 10;
	TArray<FVector> _Vertices;
	TArray<uint32> _Indices;
	float _GridWidth = 10.0f;
	float _GridHeight = 10.0f;

	float _AccumulatedTime = 0.0f;
};

