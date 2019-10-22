#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RenderTextureRendering.h"
#include "RenderTextureActor.generated.h"

UCLASS()
class SHADERSANDBOX_API ARenderTextureActor : public AActor {
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category=Rendering, BlueprintReadOnly)
	class UTextureRenderTarget2D* RenderTarget = nullptr;

	UPROPERTY(EditAnywhere, Category=Rendering, BlueprintReadOnly)
	class UCanvasRenderTarget2D* CanvasTarget = nullptr;

public:
	ARenderTextureActor();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void Tick(float DeltaTime) override;

private:
	FRenderTextureRendering Renderer;
};
