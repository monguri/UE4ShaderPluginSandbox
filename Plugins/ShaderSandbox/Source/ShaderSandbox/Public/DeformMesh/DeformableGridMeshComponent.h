#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/MeshComponent.h"
#include "DeformableGridMeshComponent.generated.h"


// almost all is copy of UCustomMeshComponent
UCLASS(hidecategories=(Object,LOD, Physics, Collision), editinlinenew, meta=(BlueprintSpawnableComponent), ClassGroup=Rendering)
class SHADERSANDBOX_API UDeformableGridMeshComponent : public UMeshComponent
{
	GENERATED_BODY()

public:
	/** Set the geometry to use on this triangle mesh */
	UFUNCTION(BlueprintCallable, Category="Components|DeformableGridMesh")
	void InitSetting(int32 NumRow, int32 NumColumn, float GridWidth, float GridHeight, float WaveNumberRow, float WaveNumberColumn, float Frequency, float Amplitude);

	uint32 GetNumRow() const { return _NumRow; }
	uint32 GetNumColumn() const { return _NumColumn; }
	const TArray<FVector>& GetVertices() const { return _Vertices; }
	const TArray<uint32>& GetIndices() const { return _Indices; }
	float GetWaveNumberRow() const { return _WaveNumberRow; }
	float GetWaveNumberColumn() const { return _WaveNumberColumn; }
	float GetFrequency() const { return _Frequency; }
	float GetAmplitude() const { return _Amplitude; }

	float GetDeltaTime() const { return _DeltaTime; }

public:
	UDeformableGridMeshComponent();

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
	uint32 _NumRow;
	uint32 _NumColumn;
	TArray<FVector> _Vertices;
	TArray<uint32> _Indices;
	float _WaveNumberRow;
	float _WaveNumberColumn;
	float _Frequency;
	float _Amplitude;

	float _DeltaTime;
};
