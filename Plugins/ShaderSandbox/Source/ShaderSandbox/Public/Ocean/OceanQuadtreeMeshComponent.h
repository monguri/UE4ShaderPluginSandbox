#pragma once

#include "DeformMesh/DeformableGridMeshComponent.h"
#include "Quadtree/Quadtree.h"
#include "OceanQuadtreeMeshComponent.generated.h"


// almost all is copy of UCustomMeshComponent
UCLASS(hidecategories=(Object,LOD, Physics, Collision), editinlinenew, meta=(BlueprintSpawnableComponent), ClassGroup=Rendering)
class SHADERSANDBOX_API UOceanQuadtreeMeshComponent : public UDeformableGridMeshComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Components|OceanQuadtree", BlueprintReadOnly, Meta = (UIMin = "1", UIMax = "1000", ClampMin = "1", ClampMax = "1000"))
	int32 NumGridDivision = 128;

	UPROPERTY(EditAnywhere, Category="Components|OceanQuadtree", BlueprintReadOnly, Meta = (UIMin = "1.0", UIMax = "100.0", ClampMin = "1.0", ClampMax = "100.0"))
	float GridLength = 1.0f;

	UPROPERTY(EditAnywhere, Category="Components|OceanQuadtree", BlueprintReadOnly, Meta = (UIMin = "1", UIMax = "10", ClampMin = "1", ClampMax = "10"))
	int32 MaxLOD = 8;

	UPROPERTY(EditAnywhere, Category="Components|OceanQuadtree", BlueprintReadOnly, Meta = (UIMin = "1", UIMax = "256", ClampMin = "1", ClampMax = "256"))
	int32 GridMaxPixelCoverage = 64;

	UPROPERTY(EditAnywhere, Category="Components|OceanQuadtree", BlueprintReadOnly, Meta = (UIMin = "10", UIMax = "10000", ClampMin = "10", ClampMax = "10000"))
	int32 PatchLength = 2000;

	UPROPERTY(EditAnywhere, Category="Components|OceanQuadtree", BlueprintReadOnly, Meta = (UIMin = "0.0", UIMax = "10.0", ClampMin = "0.0", ClampMax = "10.0"))
	float TimeScale = 0.8f;

	UPROPERTY(EditAnywhere, Category="Components|OceanQuadtree", BlueprintReadOnly, Meta = (UIMin = "0.0", UIMax = "10.0", ClampMin = "0.0", ClampMax = "10.0"))
	float AmplitudeScale = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Components|OceanQuadtree", BlueprintReadOnly)
	FVector2D WindDirection = FVector2D(0.8f, 0.6f);

	UPROPERTY(EditAnywhere, Category="Components|OceanQuadtree", BlueprintReadOnly, Meta = (UIMin = "0.0", UIMax = "10000.0", ClampMin = "0.0", ClampMax = "10000.0"))
	float WindSpeed = 600.0f;

	UPROPERTY(EditAnywhere, Category="Components|OceanQuadtree", BlueprintReadOnly, Meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
	float WindDependency = 0.85f;

	UPROPERTY(EditAnywhere, Category="Components|OceanQuadtree", BlueprintReadOnly, Meta = (UIMin = "0.0", UIMax = "10.0", ClampMin = "0.0", ClampMax = "10.0"))
	float ChoppyScale = 1.3f;

	UPROPERTY(EditAnywhere, Category="Components|OceanQuadtree", BlueprintReadOnly, Meta = (UIMin = "0.0", UIMax = "500000.0", ClampMin = "0.0", ClampMax = "500000.0"))
	float PerlinLerpBeginDistance = 800.0f;

	UPROPERTY(EditAnywhere, Category="Components|OceanQuadtree", BlueprintReadOnly, Meta = (UIMin = "0.0", UIMax = "500000.0", ClampMin = "0.0", ClampMax = "500000.0"))
	float PerlinLerpEndDistance = 20000.0f;

	UPROPERTY(EditAnywhere, Category="Components|OceanQuadtree", BlueprintReadOnly, Meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
	float PerlinUVSpeed = 0.06f;

	UPROPERTY(EditAnywhere, Category="Components|OceanQuadtree", BlueprintReadOnly)
	FVector PerlinDisplacement = FVector(35.0f, 42.0f, 57.0f);

	UPROPERTY(EditAnywhere, Category="Components|OceanQuadtree", BlueprintReadOnly)
	FVector PerlinGradient = FVector(1.4f, 1.6f, 2.2f);

	UPROPERTY(EditAnywhere, Category="Components|OceanQuadtree", BlueprintReadOnly)
	FVector PerlinUVScale = FVector(1.12f, 0.59f, 0.23f);

	UPROPERTY(EditAnywhere, Category = "Components|OceanQuadtree", BlueprintReadOnly)
	class UMaterialParameterCollection* OceanMPC = nullptr;

	UPROPERTY(EditAnywhere, Category="Components|OceanQuadtree", BlueprintReadOnly)
	class UCanvasRenderTarget2D* DisplacementMap = nullptr;

	UPROPERTY(EditAnywhere, Category="Components|OceanQuadtree", BlueprintReadOnly)
	class UCanvasRenderTarget2D* GradientFoldingMap = nullptr;

	UPROPERTY(EditAnywhere, Category="Components|OceanQuadtree", BlueprintReadOnly)
	class UCanvasRenderTarget2D* H0DebugView = nullptr;

	UPROPERTY(EditAnywhere, Category="Components|OceanQuadtree", BlueprintReadOnly)
	class UCanvasRenderTarget2D* HtDebugView = nullptr;

	UPROPERTY(EditAnywhere, Category="Components|OceanQuadtree", BlueprintReadOnly)
	class UCanvasRenderTarget2D* DkxDebugView = nullptr;

	UPROPERTY(EditAnywhere, Category="Components|OceanQuadtree", BlueprintReadOnly)
	class UCanvasRenderTarget2D* DkyDebugView = nullptr;

	UPROPERTY(EditAnywhere, Category="Components|OceanQuadtree", BlueprintReadOnly)
	class UCanvasRenderTarget2D* DxyzDebugView = nullptr;

	UPROPERTY(EditAnywhere, Category="Components|OceanQuadtree", BlueprintReadOnly)
	float DxyzDebugAmplitude = 100.0f;

	float GetAccumulatedTime() const { return _AccumulatedTime; }

	UCanvasRenderTarget2D* GetDisplacementMap() const { return DisplacementMap; }
	FShaderResourceViewRHIRef GetDisplacementMapSRV() const { return _DisplacementMapSRV; }
	FUnorderedAccessViewRHIRef GetDisplacementMapUAV() const { return _DisplacementMapUAV; }
	FUnorderedAccessViewRHIRef GetGradientFoldingMapUAV() const { return _GradientFoldingMapUAV; }
	FUnorderedAccessViewRHIRef GetH0DebugViewUAV() const { return _H0DebugViewUAV; }
	FUnorderedAccessViewRHIRef GetHtDebugViewUAV() const { return _HtDebugViewUAV; }
	FUnorderedAccessViewRHIRef GetDkxDebugViewUAV() const { return _DkxDebugViewUAV; }
	FUnorderedAccessViewRHIRef GetDkyDebugViewUAV() const { return _DkyDebugViewUAV; }
	FUnorderedAccessViewRHIRef GetDxyzDebugViewUAV() const { return _DxyzDebugViewUAV; }

public:
	UOceanQuadtreeMeshComponent();
	~UOceanQuadtreeMeshComponent();

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End USceneComponent Interface.

	const TArray<class UMaterialInstanceDynamic*>& GetLODMIDList() const;
	class UMaterialParameterCollectionInstance* GetMPCInstance() const;
	const TArray<Quadtree::FQuadMeshParameter>& GetQuadMeshParams() const;

protected:
	//~ Begin UActorComponent Interface.
	virtual void OnRegister() override;
	virtual void SendRenderDynamicData_Concurrent() override;
	//~ End UActorComponent Interface.

private:
	FShaderResourceViewRHIRef _DisplacementMapSRV;
	FUnorderedAccessViewRHIRef _DisplacementMapUAV;
	FUnorderedAccessViewRHIRef _GradientFoldingMapUAV;
	FUnorderedAccessViewRHIRef _H0DebugViewUAV;
	FUnorderedAccessViewRHIRef _HtDebugViewUAV;
	FUnorderedAccessViewRHIRef _DkxDebugViewUAV;
	FUnorderedAccessViewRHIRef _DkyDebugViewUAV;
	FUnorderedAccessViewRHIRef _DxyzDebugViewUAV;

	UPROPERTY(Transient)
	TArray<class UMaterialInstanceDynamic*> _LODMIDList;
	UPROPERTY(Transient)
	class UMaterialParameterCollectionInstance* _MPCInstance = nullptr;

	TArray<Quadtree::FQuadMeshParameter> QuadMeshParams;
};

