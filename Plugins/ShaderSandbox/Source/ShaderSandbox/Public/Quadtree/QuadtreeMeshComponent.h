#pragma once

#include "DeformMesh/DeformableGridMeshComponent.h"
#include "QuadtreeMeshComponent.generated.h"


// almost all is copy of UCustomMeshComponent
UCLASS(hidecategories=(Object,LOD, Physics, Collision), editinlinenew, meta=(BlueprintSpawnableComponent), ClassGroup=Rendering)
class SHADERSANDBOX_API UQuadtreeMeshComponent : public UDeformableGridMeshComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Components|Quadtree", BlueprintReadOnly)
	int32 NumGridRowColumn = 128;

	UPROPERTY(EditAnywhere, Category="Components|Quadtree", BlueprintReadOnly)
	float GridLength = 1.0f;

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

