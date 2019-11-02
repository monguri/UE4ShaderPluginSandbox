#pragma once

#include "CoreMinimal.h"

class FRenderTextureRendering {

public:
	void Initialize(ERHIFeatureLevel::Type FeatureLevel, class UTextureRenderTarget2D* OutputRenderTarget, class UCanvasRenderTarget2D* OutputCanvasTarget);
	void Finalize();
	void DrawRenderTextureVSPS();
	void DrawRenderTextureCS();

private:
	ERHIFeatureLevel::Type _FeatureLevel = ERHIFeatureLevel::Type::SM5;
	class UTextureRenderTarget2D* _OutputRenderTarget = nullptr;
	class UCanvasRenderTarget2D* _OutputCanvasTarget = nullptr;
	FUnorderedAccessViewRHIRef _OutputCanvasUAV;
};
