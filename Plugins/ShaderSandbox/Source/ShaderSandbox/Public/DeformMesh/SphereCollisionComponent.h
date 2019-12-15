#pragma once
#include "Components/StaticMeshComponent.h"
#include "SphereCollisionComponent.generated.h"


// クロス用の球コリジョンを表現するためのコンポーネント。使用の際は、UE4の/Engine/BasicShapes/SphereをStaticMeshに設定すること。
UCLASS(hidecategories=(Object,LOD, Physics, Collision), editinlinenew, meta=(BlueprintSpawnableComponent), ClassGroup=Rendering)
class SHADERSANDBOX_API USphereCollisionComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	/** Set the geometry and vertex paintings to use on this triangle mesh as cloth. */
	UFUNCTION(BlueprintCallable, Category = "Components|ClothGridMesh")
	void SetRadius(float Radius);

	float GetRadius() const { return _Radius; }

private:
	float _Radius = 100.0f;
};
