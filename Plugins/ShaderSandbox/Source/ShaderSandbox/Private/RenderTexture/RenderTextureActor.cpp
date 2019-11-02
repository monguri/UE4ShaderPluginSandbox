#include "RenderTexture/RenderTextureActor.h"
#include "RHI.h"
#include "Engine/World.h"
#include "SceneInterface.h"

ARenderTextureActor::ARenderTextureActor()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ARenderTextureActor::BeginPlay()
{
	AActor::BeginPlay();

	ERHIFeatureLevel::Type FeatureLevel = GetWorld()->Scene->GetFeatureLevel();
	UE_LOG(LogTemp, Warning, TEXT("Shader Feature Level: %d"), FeatureLevel);
	UE_LOG(LogTemp, Warning, TEXT("Max Shader Feature Level: %d"), GMaxRHIFeatureLevel);

	Renderer.Initialize(FeatureLevel, RenderTarget, CanvasTarget);
}

void ARenderTextureActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Renderer.Finalize();
}

void ARenderTextureActor::Tick(float DeltaTime)
{
	if (RenderTarget != nullptr)
	{
		Renderer.DrawRenderTextureVSPS();
	}
	if (CanvasTarget != nullptr)
	{
		Renderer.DrawRenderTextureCS();
	}
}
