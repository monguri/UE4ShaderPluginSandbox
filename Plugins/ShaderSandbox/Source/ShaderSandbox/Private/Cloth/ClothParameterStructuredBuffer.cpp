#include "Cloth/ClothParameterStructuredBuffer.h"
#include "Cloth/ClothGridMeshParameters.h"

FClothParameterStructuredBuffer::~FClothParameterStructuredBuffer()
{
	ComponentsDataSRV.SafeRelease();
	ComponentsData.SafeRelease();
}

void FClothParameterStructuredBuffer::SetData(const TArray<struct FGridClothParameters>& Data)
{
	if (ComponentsData.IsValid())
	{
		// 既に初期化済みなら値の設定
		check(ComponentsDataSRV.IsValid());

		//TODO: Reallocateはしてないので、クロスインスタンスの数が増えたりはしないという前提。増えるようなら、前のようにBUF_Volatileにして毎フレームnewした方がいい
		uint32 Size = Data.Num() * sizeof(FGridClothParameters);
		uint8* Buffer = (uint8*)RHILockStructuredBuffer(ComponentsData, 0, Size, EResourceLockMode::RLM_WriteOnly);
		FMemory::Memcpy(Buffer, Data.GetData(), Size);
		RHIUnlockStructuredBuffer(ComponentsData);
	}
	else
	{
		// まだ初期化してないならInitResource()からInitDynamicRHI()の呼び出し
		DataCache = Data;
		InitResource();
	}
}

void FClothParameterStructuredBuffer::InitDynamicRHI()
{
	FRHIResourceCreateInfo CreateInfo;
	uint32 Size = DataCache.Num() * sizeof(FGridClothParameters);

	ComponentsData = RHICreateStructuredBuffer(sizeof(FGridClothParameters), Size, EBufferUsageFlags::BUF_Dynamic | EBufferUsageFlags::BUF_ShaderResource, CreateInfo);
	ComponentsDataSRV = RHICreateShaderResourceView(ComponentsData);

	uint8* Buffer = (uint8*)RHILockStructuredBuffer(ComponentsData, 0, Size, EResourceLockMode::RLM_WriteOnly);
	FMemory::Memcpy(Buffer, DataCache.GetData(), Size);
	RHIUnlockStructuredBuffer(ComponentsData);
}

void FClothParameterStructuredBuffer::ReleaseDynamicRHI()
{
	ComponentsDataSRV.SafeRelease();
	ComponentsData.SafeRelease();
}

