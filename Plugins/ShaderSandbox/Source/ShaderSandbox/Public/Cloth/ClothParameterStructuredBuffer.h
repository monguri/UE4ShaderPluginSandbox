#pragma once

#include "RenderResource.h"

struct FClothParameterStructuredBuffer : public FRenderResource
{
public:
	FClothParameterStructuredBuffer(const TArray<struct FGridClothParameters>& ComponentsData);
	virtual ~FClothParameterStructuredBuffer();

	virtual void InitDynamicRHI() override;
	virtual void ReleaseDynamicRHI() override;

	int32 GetComponentsDataCount() const { return OriginalComponentsData.Num(); };
	FRHIShaderResourceView* GetSRV() const { return ComponentsDataSRV; }

private:
	FStructuredBufferRHIRef ComponentsData;
	FShaderResourceViewRHIRef ComponentsDataSRV;
	TArray<struct FGridClothParameters> OriginalComponentsData;
};

