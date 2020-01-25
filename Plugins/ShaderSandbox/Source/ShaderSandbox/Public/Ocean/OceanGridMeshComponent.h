#pragma once

#include "DeformMesh/DeformableGridMeshComponent.h"
#include "OceanGridMeshComponent.generated.h"


// almost all is copy of UCustomMeshComponent
UCLASS(hidecategories=(Object,LOD, Physics, Collision), editinlinenew, meta=(BlueprintSpawnableComponent), ClassGroup=Rendering)
class SHADERSANDBOX_API UOceanGridMeshComponent : public UDeformableGridMeshComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Components|OceanGridMesh", BlueprintReadOnly)
	class UCanvasRenderTarget2D* DisplacementMap = nullptr;

	/** Set sin wave settings. */
	UFUNCTION(BlueprintCallable, Category="Components|OceanGridMesh")
	void SetSinWaveSettings(float WaveLengthRow, float WaveLengthColumn, float Period, float Amplitude);

	float GetWaveLengthRow() const { return _WaveLengthRow; }
	float GetWaveLengthColumn() const { return _WaveLengthColumn; }
	float GetPeriod() const { return _Period; }
	float GetAmplitude() const { return _Amplitude; }
	float GetAccumulatedTime() const { return _AccumulatedTime; }

	/** Set sin wave settings. */
	UFUNCTION(BlueprintCallable, Category="Components|OceanGridMesh")
	void SetOceanSpectrumSettings(float TimeScale, float AmplitudeScale, FVector2D WindDirection, float WindSpeed, float WindDependency, float ChoppyScale);

	float GetTimeScale() const { return _TimeScale; }
	float GetAmplitudeScale() const { return _AmplitudeScale; }
	FVector2D GetWindDirection() const { return _WindDirection; }
	float GetWindSpeed() const { return _WindSpeed; }
	float GetWindDependency() const { return _WindDependency; }
	float GetChoppyScale() const { return _ChoppyScale; }

	UCanvasRenderTarget2D* GetDisplacementMap() const { return DisplacementMap; }
	FUnorderedAccessViewRHIRef GetDisplacementMapUAV() const { return _DisplacementMapUAV; }

public:
	UOceanGridMeshComponent();
	~UOceanGridMeshComponent();

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.

protected:
	virtual void SendRenderDynamicData_Concurrent() override;

private:
	// sin wave test parameters

	float _WaveLengthRow = 10.0f;
	float _WaveLengthColumn = 10.0f;
	float _Period = 1.0f;
	float _Amplitude = 10.0f;

	// explanation of parametes are in OceanSimulator.h

	float _TimeScale = 0.8f;
	float _AmplitudeScale = 0.35f;
	FVector2D _WindDirection = FVector2D(0.8f, 0.6f);
	float _WindSpeed = 600.0f;
	float _WindDependency = 0.07f;
	float _ChoppyScale = 1.3f;

	FUnorderedAccessViewRHIRef _DisplacementMapUAV;
};

