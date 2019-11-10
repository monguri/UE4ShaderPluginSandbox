#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/MeshComponent.h"
#include "DeformMeshComponent.generated.h"


// almost all is copy of UCustomMeshComponent
UCLASS(hidecategories=(Object,LOD, Physics, Collision), editinlinenew, meta=(BlueprintSpawnableComponent), ClassGroup=Rendering)
class SHADERSANDBOX_API UDeformMeshComponent : public UMeshComponent
{
	GENERATED_BODY()

public:
	/** Set the geometry to use on this triangle mesh */
	UFUNCTION(BlueprintCallable, Category="Components|DeformMesh")
	void SetMeshVerticesIndices(const TArray<FVector>& Vertices, const TArray<int32>& Indices); // BPä÷êîÇæÇ∆TArray<uint32>Ç…ÇÕëŒâûÇµÇƒÇ¢Ç»Ç¢ÇÃÇ≈Ç‚ÇﬁÇ»Ç≠int32Çégóp

	const TArray<FVector>& GetVertices() const { return _Vertices; }

	const TArray<uint32>& GetIndices() const { return _Indices; }

public:
	UDeformMeshComponent();

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin UMeshComponent Interface.
	virtual int32 GetNumMaterials() const override;
	//~ End UMeshComponent Interface.

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ Begin USceneComponent Interface.

protected:
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void SendRenderDynamicData_Concurrent() override;

private:
	TArray<FVector> _Vertices;
	TArray<uint32> _Indices;
};
