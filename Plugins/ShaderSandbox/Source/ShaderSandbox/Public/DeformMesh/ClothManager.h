#pragma once

#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "ClothManager.generated.h"

// Cloth instance manager.
UCLASS(hidecategories=(Object,LOD, Physics, Collision), ClassGroup=Rendering)
class SHADERSANDBOX_API AClothManager : public AActor
{
	GENERATED_BODY()

public:
	AClothManager();
	virtual ~AClothManager();

	void RegisterClothMesh(class UClothGridMeshComponent* ClothMesh);
	void UnregisterClothMesh(class UClothGridMeshComponent* ClothMesh);

	static AClothManager* GetInstance();
	const TArray<class USphereCollisionComponent*>& GetSphereCollisions() const;

	void RegisterSphereCollision(class USphereCollisionComponent* SphereCollision);
	void UnregisterSphereCollision(class USphereCollisionComponent* SphereCollision);

private:
	TArray<class UClothGridMeshComponent*> ClothMeshes;
	TArray<class USphereCollisionComponent*> SphereCollisions;
};

// global singleton instance
AClothManager* gClothManager = nullptr;

