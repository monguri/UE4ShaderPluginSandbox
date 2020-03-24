#pragma once

#include "DeformMesh/DeformableGridMeshComponent.h"
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

	UPROPERTY(EditAnywhere, Category="Components|OceanQuadtree", BlueprintReadOnly, Meta = (UIMin = "0.0", UIMax = "1000.0", ClampMin = "0.0", ClampMax = "1000.0"))
	float WindSpeed = 600.0f;

	UPROPERTY(EditAnywhere, Category="Components|OceanQuadtree", BlueprintReadOnly, Meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
	float WindDependency = 0.07f;

	UPROPERTY(EditAnywhere, Category="Components|OceanQuadtree", BlueprintReadOnly, Meta = (UIMin = "0.0", UIMax = "10.0", ClampMin = "0.0", ClampMax = "10.0"))
	float ChoppyScale = 1.3f;

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
	float GetPatchLength() const { return _PatchLength; }
	float GetTimeScale() const { return _TimeScale; }
	float GetAmplitudeScale() const { return _AmplitudeScale; }
	FVector2D GetWindDirection() const { return _WindDirection; }
	float GetWindSpeed() const { return _WindSpeed; }
	float GetWindDependency() const { return _WindDependency; }
	float GetChoppyScale() const { return _ChoppyScale; }

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

protected:
	virtual void OnRegister() override;
	virtual void SendRenderDynamicData_Concurrent() override;

private:
	// explanation of parametes are in OceanSimulator.h

	float _PatchLength = 2000.0f;
	float _TimeScale = 0.8f;
	float _AmplitudeScale = 0.35f;
	FVector2D _WindDirection = FVector2D(0.8f, 0.6f);
	float _WindSpeed = 600.0f;
	float _WindDependency = 0.07f;
	float _ChoppyScale = 1.3f;

	FShaderResourceViewRHIRef _DisplacementMapSRV;
	FUnorderedAccessViewRHIRef _DisplacementMapUAV;
	FUnorderedAccessViewRHIRef _GradientFoldingMapUAV;
	FUnorderedAccessViewRHIRef _H0DebugViewUAV;
	FUnorderedAccessViewRHIRef _HtDebugViewUAV;
	FUnorderedAccessViewRHIRef _DkxDebugViewUAV;
	FUnorderedAccessViewRHIRef _DkyDebugViewUAV;
	FUnorderedAccessViewRHIRef _DxyzDebugViewUAV;
};
