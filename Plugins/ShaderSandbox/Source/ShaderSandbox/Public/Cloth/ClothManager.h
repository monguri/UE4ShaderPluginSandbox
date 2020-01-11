#pragma once

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "Cloth/ClothGridMeshDeformer.h"
#include "DeformMesh/DeformableVertexBuffers.h"
#include "ClothManager.generated.h"

UCLASS(hidecategories=(Object,LOD, Physics, Collision), ClassGroup=Rendering)
class SHADERSANDBOX_API UClothManagerComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	void RegisterClothMesh(class UClothGridMeshComponent* ClothMesh);
	void UnregisterClothMesh(class UClothGridMeshComponent* ClothMesh);
	void EnqueueSimulateClothCommand(const FClothGridMeshDeformCommand& Command);

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.
};

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
	void EnqueueSimulateClothCommand(const FClothGridMeshDeformCommand& Command);

private:
	TArray<class USphereCollisionComponent*> SphereCollisions;
};

// global singleton instance
AClothManager* gClothManager = nullptr;

