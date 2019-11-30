#pragma once

#include "DeformableGridMeshComponent.h"
#include "ClothGridMeshComponent.generated.h"


// almost all is copy of UCustomMeshComponent
UCLASS(hidecategories=(Object,LOD, Physics, Collision), editinlinenew, meta=(BlueprintSpawnableComponent), ClassGroup=Rendering)
class SHADERSANDBOX_API UClothGridMeshComponent : public UDeformableGridMeshComponent
{
	GENERATED_BODY()

public:
	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.

protected:
	virtual void SendRenderDynamicData_Concurrent() override;
};
