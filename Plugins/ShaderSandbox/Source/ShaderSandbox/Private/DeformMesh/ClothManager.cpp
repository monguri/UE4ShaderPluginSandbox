#include "DeformMesh/ClothManager.h"
#include "PrimitiveSceneProxy.h"
#include "DeformMesh/ClothGridMeshComponent.h"
#include "DeformMesh/SphereCollisionComponent.h"

static inline void InitOrUpdateResourceMacroClothManager(FRenderResource* Resource)
{
	if (!Resource->IsInitialized())
	{
		Resource->InitResource();
	}
	else
	{
		Resource->UpdateRHI();
	}
}

class FClothManagerSceneProxy final : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	FClothManagerSceneProxy(UClothManagerComponent* Component)
		: FPrimitiveSceneProxy(Component)
	{
	}

	virtual ~FClothManagerSceneProxy()
	{
		WorkPositionVertexBuffer.ReleaseResource();
	}

	virtual uint32 GetMemoryFootprint( void ) const override { return( sizeof( *this ) + GetAllocatedSize() ); }

	uint32 GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

	void RegisterClothMesh(UClothGridMeshComponent* ClothMesh)
	{
		FMergeData MergeData;
		MergeData.NumVertex = ClothMesh->GetVertices().Num();

		uint32 Offset = 0;
		for (const TPair<UClothGridMeshComponent*, FMergeData>& ClothMeshData : ClothMeshDataMap)
		{
			Offset += ClothMeshData.Value.NumVertex;
		}

		MergeData.Offset = Offset;

		check(ClothMeshDataMap.Find(ClothMesh) == nullptr);
		ClothMeshDataMap.Add(ClothMesh, MergeData);

		// TODO:クロスの回数だけInitしてUpdateRHIしてるのはもったいない
		// Initは再入可能。中でReallocしている
		WorkPositionVertexBuffer.Init(MergeData.Offset + MergeData.NumVertex);

		InitOrUpdateResourceMacroClothManager(&WorkPositionVertexBuffer);
	}

	void UnregisterClothMesh(UClothGridMeshComponent* ClothMesh)
	{
		ClothMeshDataMap.Remove(ClothMesh);
		// TODO:Removeしても、今のところ連結したバーテックスバッファは縮めない。
	}

	// TODO:1メッシュにまとめる前の処理を維持するため仮に作った
	void EnqueueSimulateClothTask(FRHICommandListImmediate& RHICmdList, const FGridClothParameters& Task)
	{
		NumTask++;
		VertexDeformer.EnqueueDeformTask(Task);

		if ((int32)NumTask >= ClothMeshDataMap.Num())
		{
			VertexDeformer.FlushDeformTaskQueue(RHICmdList, WorkPositionVertexBuffer.GetUAV());
			NumTask = 0;
		}
	}

private:
	// Data for 
	struct FMergeData
	{
		int32 Offset;
		int32 NumVertex;
	};

	int32 NumTask = 0;
	TMap<class UClothGridMeshComponent*, FMergeData> ClothMeshDataMap;
	FClothGridMeshDeformer VertexDeformer;
	// vertex buffer to simulate all clothes by one dispatch by merging all vertex buffers.
	FDeformablePositionVertexBuffer WorkPositionVertexBuffer;
};

void UClothManagerComponent::RegisterClothMesh(UClothGridMeshComponent* ClothMesh)
{
	if (SceneProxy != nullptr)
	{
		FClothManagerSceneProxy* ClothManagerSceneProxy = (FClothManagerSceneProxy*)SceneProxy;
		ENQUEUE_RENDER_COMMAND(RegisterClothMesh)(
			[ClothManagerSceneProxy, ClothMesh](FRHICommandListImmediate& RHICmdList)
			{
				ClothManagerSceneProxy->RegisterClothMesh(ClothMesh);
			});
	}
}

void UClothManagerComponent::UnregisterClothMesh(UClothGridMeshComponent* ClothMesh)
{
	if (SceneProxy != nullptr)
	{
		FClothManagerSceneProxy* ClothManagerSceneProxy = (FClothManagerSceneProxy*)SceneProxy;
		ENQUEUE_RENDER_COMMAND(UnregisterClothMesh)(
			[ClothManagerSceneProxy, ClothMesh](FRHICommandListImmediate& RHICmdList)
			{
				ClothManagerSceneProxy->UnregisterClothMesh(ClothMesh);
			});
	}
}

// TODO:1メッシュにまとめる前の処理を維持するため仮に作った
void UClothManagerComponent::EnqueueSimulateClothTask(const FGridClothParameters& Task)
{
	if (SceneProxy != nullptr)
	{
		FClothManagerSceneProxy* ClothManagerSceneProxy = (FClothManagerSceneProxy*)SceneProxy;
		ENQUEUE_RENDER_COMMAND(UnregisterClothMesh)(
			[ClothManagerSceneProxy, Task](FRHICommandListImmediate& RHICmdList)
			{
				ClothManagerSceneProxy->EnqueueSimulateClothTask(RHICmdList, Task);
			});
	}
}

FPrimitiveSceneProxy* UClothManagerComponent::CreateSceneProxy()
{
	return new FClothManagerSceneProxy(this);
}

AClothManager::AClothManager()
{
	gClothManager = this;
	RootComponent = CreateDefaultSubobject<UClothManagerComponent>(TEXT("ClothManagerComponent"));
}

AClothManager::~AClothManager()
{
	gClothManager = nullptr;
}

AClothManager* AClothManager::GetInstance()
{
	return gClothManager;
}

void AClothManager::RegisterClothMesh(UClothGridMeshComponent* ClothMesh)
{
	Cast<UClothManagerComponent>(RootComponent)->RegisterClothMesh(ClothMesh);
}

void AClothManager::UnregisterClothMesh(UClothGridMeshComponent* ClothMesh)
{
	Cast<UClothManagerComponent>(RootComponent)->UnregisterClothMesh(ClothMesh);
}

const TArray<USphereCollisionComponent*>& AClothManager::GetSphereCollisions() const
{
	return SphereCollisions;
}

void AClothManager::RegisterSphereCollision(USphereCollisionComponent* SphereCollision)
{
	SphereCollisions.Add(SphereCollision);
}

void AClothManager::UnregisterSphereCollision(USphereCollisionComponent* SphereCollision)
{
	SphereCollisions.Remove(SphereCollision);
}

// TODO:1メッシュにまとめる前の処理を維持するため仮に作った
void AClothManager::EnqueueSimulateClothTask(const FGridClothParameters& Task)
{
	Cast<UClothManagerComponent>(RootComponent)->EnqueueSimulateClothTask(Task);
}

