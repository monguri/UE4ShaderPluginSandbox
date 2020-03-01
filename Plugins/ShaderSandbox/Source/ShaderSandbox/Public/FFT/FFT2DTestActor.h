#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FFTTexture2D.h"
#include "FFT2DTestActor.generated.h"

UCLASS()
class SHADERSANDBOX_API AFFT2DTestActor : public AActor {
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Parameter, BlueprintReadOnly)
	EFFTMode FFTMode = EFFTMode::Forward;

	UPROPERTY(EditAnywhere, Category = Parameter, BlueprintReadOnly)
	class UTexture2D* SrcTexture = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter, BlueprintReadOnly)
	class UCanvasRenderTarget2D* DstTexture = nullptr;

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	FUnorderedAccessViewRHIRef _DstUAV;
};

