#include "Cloth/ClothParameterStructuredBuffer.h"
#include "Cloth/ClothGridMeshParameters.h"

void FClothParameterStructuredBuffer::SetData(const TArray<struct FGridClothParameters>& Data)
{
	uint32 Size = Data.Num() * sizeof(FGridClothParameters);

	if (!IsInitialized())
	{
		InitResource();

		FRHIResourceCreateInfo CreateInfo;

		ComponentsData = RHICreateStructuredBuffer(sizeof(FGridClothParameters), Size, EBufferUsageFlags::BUF_Dynamic | EBufferUsageFlags::BUF_ShaderResource, CreateInfo);
		ComponentsDataSRV = RHICreateShaderResourceView(ComponentsData);
	}

	//TODO: Reallocateはしてないので、クロスインスタンスの数が増えたりはしないという前提。増えるようなら、前のようにBUF_Volatileにして毎フレームnewした方がいい
	uint8* Buffer = (uint8*)RHILockStructuredBuffer(ComponentsData, 0, Size, EResourceLockMode::RLM_WriteOnly);
	FMemory::Memcpy(Buffer, Data.GetData(), Size);
	RHIUnlockStructuredBuffer(ComponentsData);
}

void FClothParameterStructuredBuffer::ReleaseDynamicRHI()
{
	ComponentsDataSRV.SafeRelease();
	ComponentsData.SafeRelease();
}

