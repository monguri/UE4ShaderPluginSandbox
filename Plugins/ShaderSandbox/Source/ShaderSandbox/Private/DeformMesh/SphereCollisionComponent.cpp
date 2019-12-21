#include "DeformMesh/SphereCollisionComponent.h"
#include "DeformMesh/ClothManager.h"

USphereCollisionComponent::~USphereCollisionComponent()
{
	AClothManager* Manager = AClothManager::GetInstance();
	if (Manager != nullptr)
	{
		Manager->UnregisterSphereCollision(this);
	}
}

void USphereCollisionComponent::SetRadius(float Radius)
{
	// UE4の/Engine/BasicShapes/SphereのStaticMeshを使う。直径が100cmであるという前提を置く
	_Radius = Radius;

	SetRelativeScale3D(FVector(2.0f * Radius / 100.0f));
}

void USphereCollisionComponent::OnRegister()
{
	Super::OnRegister();

	AClothManager* Manager = AClothManager::GetInstance();
	if (Manager == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("USphereCollisionComponent::OnRegister() There is no AClothManager. So failed to register this collision."));
	}
	else
	{
		Manager->RegisterSphereCollision(this);
	}
}
