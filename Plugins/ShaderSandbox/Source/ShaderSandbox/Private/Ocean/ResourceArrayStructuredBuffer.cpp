#include "Ocean/ResourceArrayStructuredBuffer.h"
#include "Containers/ResourceArray.h"
#include "RenderingThread.h"

void FResourceArrayStructuredBuffer::Initialize(FResourceArrayInterface& Data, uint32 ByteStride)
{
	FResourceArrayInterface* DataPtr = &Data;

	ENQUEUE_RENDER_COMMAND(InitializeResourceArrayStructuredBuffer)(
		[DataPtr, ByteStride, this](FRHICommandListImmediate& RHICmdList)
		{
			if (IsInitialized())
			{
				ReleaseResource();
			}

			InitResource();

			FRHIResourceCreateInfo ResourceCreateInfo;
			ResourceCreateInfo.ResourceArray = DataPtr;

			StructuredBuffer = RHICreateStructuredBuffer(ByteStride, DataPtr->GetResourceDataSize(), EBufferUsageFlags::BUF_Static | EBufferUsageFlags::BUF_ShaderResource | EBufferUsageFlags::BUF_UnorderedAccess, ResourceCreateInfo);
			SRV = RHICreateShaderResourceView(StructuredBuffer);
			UAV = RHICreateUnorderedAccessView(StructuredBuffer, false, false);
		});
}

void FResourceArrayStructuredBuffer::ReleaseDynamicRHI()
{
	UAV.SafeRelease();
	SRV.SafeRelease();
	StructuredBuffer.SafeRelease();
}

