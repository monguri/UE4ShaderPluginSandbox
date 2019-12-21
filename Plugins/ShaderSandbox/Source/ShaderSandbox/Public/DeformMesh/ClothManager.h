#pragma once

#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "ClothManager.generated.h"

// The component to manage cloth instance.
UCLASS(ClassGroup=Rendering)
class SHADERSANDBOX_API UClothManageComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	const TArray<class USphereCollisionComponent*>& GetSphereCollisions() const;
	void AddSphereCollision(class USphereCollisionComponent* SphereCollision);
	void RemoveSphereCollision(class USphereCollisionComponent* SphereCollision);

private:
	TArray<class USphereCollisionComponent*> SphereCollisions;
};

// Cloth instance manager.
UCLASS(hidecategories=(Object,LOD, Physics, Collision), ClassGroup=Rendering)
class SHADERSANDBOX_API AClothManager : public AActor
{
	GENERATED_BODY()

public:
	AClothManager();
	virtual ~AClothManager();

	static AClothManager* GetInstance();
	void GetSphereCollisions(TArray<class USphereCollisionComponent*>& OutArray) const;

	void RegisterSphereCollision(class USphereCollisionComponent* SphereCollision);
	void UnregisterSphereCollision(class USphereCollisionComponent* SphereCollision);

private:
	UPROPERTY(Category = StaticMeshActor, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UClothManageComponent* ClothManageComponent;
};

// global singleton instance
AClothManager* gClothManager = nullptr;

