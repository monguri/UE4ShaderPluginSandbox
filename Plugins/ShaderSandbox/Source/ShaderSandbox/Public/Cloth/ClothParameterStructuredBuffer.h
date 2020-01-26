#pragma once

#include "RenderResource.h"

struct FClothParameterStructuredBuffer : public FRenderResource
{
public:
	void SetData(const TArray<struct FGridClothParameters>& Data);
	virtual void ReleaseDynamicRHI() override;

	FRHIShaderResourceView* GetSRV() const { return ComponentsDataSRV; }

private:
	FStructuredBufferRHIRef ComponentsData;
	FShaderResourceViewRHIRef ComponentsDataSRV;
	TArray<struct FGridClothParameters> DataCache;
};

