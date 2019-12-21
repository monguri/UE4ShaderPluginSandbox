#include "DeformMesh/ClothManager.h"
#include "DeformMesh/SphereCollisionComponent.h"

const TArray<class USphereCollisionComponent*>& UClothManageComponent::GetSphereCollisions() const
{
	return SphereCollisions;
}

void UClothManageComponent::AddSphereCollision(USphereCollisionComponent* SphereCollision)
{
	SphereCollisions.Add(SphereCollision);
}

void UClothManageComponent::RemoveSphereCollision(USphereCollisionComponent* SphereCollision)
{
	SphereCollisions.Remove(SphereCollision);
}

AClothManager::AClothManager()
{
	ClothManageComponent = CreateDefaultSubobject<UClothManageComponent>(TEXT("ClothManageComponent"));

	RootComponent = ClothManageComponent;

	gClothManager = this;
}

AClothManager::~AClothManager()
{
	ClothManageComponent = nullptr;
	gClothManager = nullptr;
}

AClothManager* AClothManager::GetInstance()
{
	return gClothManager;
}

void AClothManager::GetSphereCollisions(TArray<class USphereCollisionComponent*>& OutArray) const
{
	if (ClothManageComponent != nullptr)
	{
		OutArray = ClothManageComponent->GetSphereCollisions();
	}
}

void AClothManager::RegisterSphereCollision(USphereCollisionComponent* SphereCollision)
{
	if (ClothManageComponent != nullptr)
	{
		ClothManageComponent->AddSphereCollision(SphereCollision);
	}
}

void AClothManager::UnregisterSphereCollision(USphereCollisionComponent* SphereCollision)
{
	if (ClothManageComponent != nullptr)
	{
		ClothManageComponent->RemoveSphereCollision(SphereCollision);
	}
}

