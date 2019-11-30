#include "DeformMesh/DeformableGridMeshComponent.h"
#include "Engine/CollisionProfile.h"

UDeformableGridMeshComponent::UDeformableGridMeshComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UDeformableGridMeshComponent::InitGridMeshSetting(int32 NumRow, int32 NumColumn, float GridWidth, float GridHeight)
{
	_NumRow = NumRow;
	_NumColumn = NumColumn;
	_GridWidth = GridWidth;
	_GridHeight = GridHeight;
	_Vertices.Reset((NumRow + 1) * (NumColumn + 1));
	_Indices.Reset(NumRow * NumColumn * 2 * 3); // ひとつのグリッドには3つのTriangle、6つの頂点インデックス指定がある

	for (int32 y = 0; y < NumRow + 1; y++)
	{
		for (int32 x = 0; x < NumColumn + 1; x++)
		{
			_Vertices.Emplace(x * GridWidth, y * GridHeight, 0.0f);
		}
	}

	for (int32 Row = 0; Row < NumRow; Row++)
	{
		for (int32 Column = 0; Column < NumColumn; Column++)
		{
			_Indices.Emplace(Row * (NumColumn + 1) + Column);
			_Indices.Emplace((Row + 1) * (NumColumn + 1) + Column);
			_Indices.Emplace((Row + 1) * (NumColumn + 1) + Column + 1);

			_Indices.Emplace(Row * (NumColumn + 1) + Column);
			_Indices.Emplace((Row + 1) * (NumColumn + 1) + Column + 1);
			_Indices.Emplace(Row * (NumColumn + 1) + Column + 1);
		}
	}

	MarkRenderStateDirty();
	UpdateBounds();
}

int32 UDeformableGridMeshComponent::GetNumMaterials() const
{
	return 1;
}

FBoxSphereBounds UDeformableGridMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox BoundingBox(ForceInit);

	// Bounds are tighter if the box is generated from pre-transformed vertices.
	for (const FVector& Vertex : _Vertices)
	{
		BoundingBox += LocalToWorld.TransformPosition(Vertex);
	}

	FBoxSphereBounds NewBounds;
	NewBounds.BoxExtent = BoundingBox.GetExtent();
	NewBounds.Origin = BoundingBox.GetCenter();
	NewBounds.SphereRadius = NewBounds.BoxExtent.Size();

	return NewBounds;
}

void UDeformableGridMeshComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	_AccumulatedTime += DeltaTime;

	// reset by a big number
	if (_AccumulatedTime > 10000)
	{
		_AccumulatedTime = 0.0f;
	}

	MarkRenderDynamicDataDirty();
}
