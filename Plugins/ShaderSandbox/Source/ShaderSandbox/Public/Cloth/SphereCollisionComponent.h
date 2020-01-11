#pragma once
#include "Components/StaticMeshComponent.h"
#include "SphereCollisionComponent.generated.h"


// The component to collide clothes as sphere collider. To use, set UE4's /Engine/BasicShapes/Sphere to static mesh.
UCLASS(hidecategories=(Object,LOD, Physics, Collision), editinlinenew, meta=(BlueprintSpawnableComponent), ClassGroup=Rendering)
class SHADERSANDBOX_API USphereCollisionComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	virtual ~USphereCollisionComponent();

	/** Set the geometry and vertex paintings to use on this triangle mesh as cloth. */
	UFUNCTION(BlueprintCallable, Category = "Components|ClothGridMesh")
	void SetRadius(float Radius);
	float GetRadius() const { return _Radius; }

protected:
	//~ Begin UActorComponent Interface
	virtual void OnRegister() override;
	//~ End UActorComponent Interface

private:
	float _Radius = 100.0f;
};
