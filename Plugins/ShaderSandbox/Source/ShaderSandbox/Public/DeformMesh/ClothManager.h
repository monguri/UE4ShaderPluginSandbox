#pragma once

#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "DeformMesh/ClothGridMeshDeformer.h"
#include "ClothManager.generated.h"

// Cloth instance manager.
UCLASS(hidecategories=(Object,LOD, Physics, Collision), ClassGroup=Rendering)
class SHADERSANDBOX_API AClothManager : public AActor
{
	GENERATED_BODY()

public:
	/** Wind velocity, world coordinate, cm/s. */
	UPROPERTY(EditAnywhere, Category = Wind)
	FVector WindVelocity = FVector::ZeroVector;

	AClothManager();
	virtual ~AClothManager();

	void RegisterClothMesh(class UClothGridMeshComponent* ClothMesh);
	void UnregisterClothMesh(class UClothGridMeshComponent* ClothMesh);

	static AClothManager* GetInstance();
	const TArray<class USphereCollisionComponent*>& GetSphereCollisions() const;

	void RegisterSphereCollision(class USphereCollisionComponent* SphereCollision);
	void UnregisterSphereCollision(class USphereCollisionComponent* SphereCollision);

	// Don't make Dequeue method because this method execute all tasks and clear que when the number of task become equal to that of cloth mesh. 
	void EnqueueSimulateClothTask(const FGridClothParameters& Task);

private:
	int32 NumTask = 0;
	TArray<class UClothGridMeshComponent*> ClothMeshes;
	TArray<class USphereCollisionComponent*> SphereCollisions;
	FClothGridMeshDeformer VertexDeformer;
};

// global singleton instance
AClothManager* gClothManager = nullptr;

