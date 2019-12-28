#include "DeformMesh/ClothManager.h"
#include "DeformMesh/SphereCollisionComponent.h"

AClothManager::AClothManager()
{
	gClothManager = this;
}

AClothManager::~AClothManager()
{
	gClothManager = nullptr;
}

AClothManager* AClothManager::GetInstance()
{
	return gClothManager;
}

void AClothManager::RegisterClothMesh(UClothGridMeshComponent* ClothMesh)
{
	ClothMeshes.Add(ClothMesh);
}

void AClothManager::UnregisterClothMesh(UClothGridMeshComponent* ClothMesh)
{
	ClothMeshes.Remove(ClothMesh);
}

const TArray<USphereCollisionComponent*>& AClothManager::GetSphereCollisions() const
{
	return SphereCollisions;
}

void AClothManager::RegisterSphereCollision(USphereCollisionComponent* SphereCollision)
{
	SphereCollisions.Add(SphereCollision);
}

void AClothManager::UnregisterSphereCollision(USphereCollisionComponent* SphereCollision)
{
	SphereCollisions.Remove(SphereCollision);
}

void AClothManager::EnqueueSimulateClothTask(const FGridClothParameters& Task)
{
	AClothManager* Self = this;

	NumTask++;

	ENQUEUE_RENDER_COMMAND(EnqueueClothGridMeshDeformTask)(
		[Self, Task](FRHICommandListImmediate& RHICmdList)
		{
			Self->VertexDeformer.EnqueueDeformTask(Task);
		}
	);

	if ((int32)NumTask >= ClothMeshes.Num())
	{
		ENQUEUE_RENDER_COMMAND(FlushClothGridMeshDeformTask)(
			[Self, Task](FRHICommandListImmediate& RHICmdList)
			{
				Self->VertexDeformer.FlushDeformTaskQueue(RHICmdList);
			}
		);

		NumTask = 0;
	}
}

