#include "DeformMesh/ClothParameterStructuredBuffer.h"

const FVector FGridClothParameters::GRAVITY = FVector(0.0f, 0.0f, -980.0f);
const float FGridClothParameters::BASE_FREQUENCY = 60.0f;

FClothParameterStructuredBuffer::FClothParameterStructuredBuffer(const TArray<FGridClothParameters>& ComponentsData)
: OriginalComponentsData(ComponentsData)
{
}

FClothParameterStructuredBuffer::~FClothParameterStructuredBuffer()
{
	ComponentsDataSRV.SafeRelease();
	ComponentsData.SafeRelease();
}

void FClothParameterStructuredBuffer::InitDynamicRHI()
{
	FRHIResourceCreateInfo CreateInfo;
	uint32 Size = OriginalComponentsData.Num() * sizeof(FGridClothParameters);

	ComponentsData = RHICreateStructuredBuffer(sizeof(FGridClothParameters), Size, EBufferUsageFlags::BUF_Volatile | EBufferUsageFlags::BUF_ShaderResource, CreateInfo);
	ComponentsDataSRV = RHICreateShaderResourceView(ComponentsData);

	uint8* Buffer = (uint8*)RHILockStructuredBuffer(ComponentsData, 0, Size, EResourceLockMode::RLM_WriteOnly);
	FMemory::Memcpy(Buffer, OriginalComponentsData.GetData(), Size);
	RHIUnlockStructuredBuffer(ComponentsData);
}

void FClothParameterStructuredBuffer::ReleaseDynamicRHI()
{
	ComponentsDataSRV.SafeRelease();
	ComponentsData.SafeRelease();
}
