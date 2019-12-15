#include "DeformMesh/SphereCollisionComponent.h"

void USphereCollisionComponent::SetRadius(float Radius)
{
	// UE4の/Engine/BasicShapes/SphereのStaticMeshを使う。直径が100cmであるという前提を置く
	_Radius = Radius;

	SetRelativeScale3D(FVector(2.0f * Radius / 100.0f));
}
