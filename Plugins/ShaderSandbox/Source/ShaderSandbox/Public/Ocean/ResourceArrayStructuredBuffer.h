#pragma once

#include "RenderResource.h"

struct FResourceArrayStructuredBuffer : public FRenderResource
{
public:
	// Caution: ENQUEUE_RENDER_COMMAND() use contents of Data. So don't use transient data.
	void Initialize(class FResourceArrayInterface& Data, uint32 ByteStride);
	virtual void ReleaseDynamicRHI() override;

	FRHIShaderResourceView* GetSRV() const { return SRV; }
	FRHIUnorderedAccessView* GetUAV() const { return UAV; }

private:
	FStructuredBufferRHIRef StructuredBuffer;
	FShaderResourceViewRHIRef SRV;
	FUnorderedAccessViewRHIRef UAV;
};

