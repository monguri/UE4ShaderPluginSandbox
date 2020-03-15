#pragma once

#include "DeformMesh/DeformableGridMeshComponent.h"
#include "QuadtreeMeshComponent.generated.h"


// almost all is copy of UCustomMeshComponent
UCLASS(hidecategories=(Object,LOD, Physics, Collision), editinlinenew, meta=(BlueprintSpawnableComponent), ClassGroup=Rendering)
class SHADERSANDBOX_API UQuadtreeMeshComponent : public UDeformableGridMeshComponent
{
	GENERATED_BODY()

public:
	UQuadtreeMeshComponent();

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.
};

