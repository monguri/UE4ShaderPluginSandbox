#pragma once

#include "DeformMesh/DeformableGridMeshComponent.h"
#include "QuadtreeMeshComponent.generated.h"


// almost all is copy of UCustomMeshComponent
UCLASS(hidecategories=(Object,LOD, Physics, Collision), editinlinenew, meta=(BlueprintSpawnableComponent), ClassGroup=Rendering)
class SHADERSANDBOX_API UQuadtreeMeshComponent : public UDeformableGridMeshComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Components|Quadtree", BlueprintReadOnly, Meta = (UIMin = "1", UIMax = "1000", ClampMin = "1", ClampMax = "1000"))
	int32 NumGridRowColumn = 128;

	UPROPERTY(EditAnywhere, Category="Components|Quadtree", BlueprintReadOnly, Meta = (UIMin = "1.0", UIMax = "100.0", ClampMin = "1.0", ClampMax = "100.0"))
	float GridLength = 1.0f;

	UPROPERTY(EditAnywhere, Category="Components|Quadtree", BlueprintReadOnly, Meta = (UIMin = "1", UIMax = "10", ClampMin = "1", ClampMax = "10"))
	int32 MaxLOD = 8;

public:
	UQuadtreeMeshComponent();

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.

	const TArray<class UMaterialInstanceDynamic*>& GetMIDPool() const;

protected:
	virtual void OnRegister() override;

private:
	UPROPERTY(Transient)
	TArray<class UMaterialInstanceDynamic*> MIDPool;
};

