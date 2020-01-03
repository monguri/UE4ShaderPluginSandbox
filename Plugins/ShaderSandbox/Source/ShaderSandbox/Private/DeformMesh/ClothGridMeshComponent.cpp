#include "DeformMesh/ClothGridMeshComponent.h"
#include "RenderingThread.h"
#include "RenderResource.h"
#include "PrimitiveViewRelevance.h"
#include "PrimitiveSceneProxy.h"
#include "VertexFactory.h"
#include "MaterialShared.h"
#include "Engine/CollisionProfile.h"
#include "Materials/Material.h"
#include "LocalVertexFactory.h"
#include "SceneManagement.h"
#include "DynamicMeshBuilder.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "ClothVertexBuffers.h"
#include "ClothGridMeshDeformer.h"
#include "RayTracingInstance.h"
#include "DeformMesh/ClothManager.h"
#include "DeformMesh/SphereCollisionComponent.h"

//TODO:FDeformGridMeshSceneProxyとソースコードの共通化ができないか

/** almost all is copy of FCustomMeshSceneProxy. */
class FClothGridMeshSceneProxy final : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	FClothGridMeshSceneProxy(UClothGridMeshComponent* Component)
		: FPrimitiveSceneProxy(Component)
		, VertexFactory(GetScene().GetFeatureLevel(), "FClothGridMeshSceneProxy")
		, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
	{
		TArray<FDynamicMeshVertex> Vertices;
		TArray<float> InvMasses;
		Vertices.Reset(Component->GetVertices().Num());
		InvMasses.Reset(Component->GetVertices().Num());
		IndexBuffer.Indices = Component->GetIndices();

		for (int32 VertIdx = 0; VertIdx < Component->GetVertices().Num(); VertIdx++)
		{
			// TODO:ColorとTangentはとりあえずFDynamicMeshVertexのデフォルト値まかせにする
			Vertices.Emplace(Component->GetVertices()[VertIdx]);
			InvMasses.Emplace(Component->GetVertices()[VertIdx].W);
		}
		VertexBuffers.InitFromClothVertexAttributes(&VertexFactory, Vertices, InvMasses, Component->GetAccelerations());

		// Enqueue initialization of render resource
		BeginInitResource(&VertexBuffers.PositionVertexBuffer);
		BeginInitResource(&VertexBuffers.DeformableMeshVertexBuffer);
		BeginInitResource(&VertexBuffers.ColorVertexBuffer);
		BeginInitResource(&VertexBuffers.PrevPositionVertexBuffer);
		BeginInitResource(&VertexBuffers.AcceralationVertexBuffer);
		BeginInitResource(&IndexBuffer);
		BeginInitResource(&VertexFactory);

		// Grab material
		Material = Component->GetMaterial(0);
		if(Material == NULL)
		{
			Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}
	}

	virtual ~FClothGridMeshSceneProxy()
	{
		VertexBuffers.PositionVertexBuffer.ReleaseResource();
		VertexBuffers.DeformableMeshVertexBuffer.ReleaseResource();
		VertexBuffers.ColorVertexBuffer.ReleaseResource();
		VertexBuffers.PrevPositionVertexBuffer.ReleaseResource();
		VertexBuffers.AcceralationVertexBuffer.ReleaseResource();
		IndexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		QUICK_SCOPE_CYCLE_COUNTER( STAT_ClothGridMeshSceneProxy_GetDynamicMeshElements );

		const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

		auto WireframeMaterialInstance = new FColoredMaterialRenderProxy(
			GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy() : NULL,
			FLinearColor(0, 0.5f, 1.f)
			);

		Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);

		FMaterialRenderProxy* MaterialProxy = NULL;
		if(bWireframe)
		{
			MaterialProxy = WireframeMaterialInstance;
		}
		else
		{
			MaterialProxy = Material->GetRenderProxy();
		}

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* View = Views[ViewIndex];
				// Draw the mesh.
				FMeshBatch& Mesh = Collector.AllocateMesh();
				FMeshBatchElement& BatchElement = Mesh.Elements[0];
				BatchElement.IndexBuffer = &IndexBuffer;
				Mesh.bWireframe = bWireframe;
				Mesh.VertexFactory = &VertexFactory;
				Mesh.MaterialRenderProxy = MaterialProxy;

				bool bHasPrecomputedVolumetricLightmap;
				FMatrix PreviousLocalToWorld;
				int32 SingleCaptureIndex;
				bool bOutputVelocity;
				GetScene().GetPrimitiveUniformShaderParameters_RenderThread(GetPrimitiveSceneInfo(), bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);

				FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
				DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, DrawsVelocity(), false);
				BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

				BatchElement.FirstIndex = 0;
				BatchElement.NumPrimitives = IndexBuffer.Indices.Num() / 3;
				BatchElement.MinVertexIndex = 0;
				BatchElement.MaxVertexIndex = VertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;
				Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
				Mesh.Type = PT_TriangleList;
				Mesh.DepthPriorityGroup = SDPG_World;
				Mesh.bCanApplyViewModeOverrides = false;
				Collector.AddMesh(ViewIndex, Mesh);
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bDynamicRelevance = true;
		Result.bRenderInMainPass = ShouldRenderInMainPass();
		Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
		Result.bRenderCustomDepth = ShouldRenderCustomDepth();
		Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		Result.bVelocityRelevance = IsMovable() && Result.bOpaqueRelevance && Result.bRenderInMainPass;
		return Result;
	}

	virtual bool CanBeOccluded() const override
	{
		return !MaterialRelevance.bDisableDepthTest;
	}

	virtual uint32 GetMemoryFootprint( void ) const override { return( sizeof( *this ) + GetAllocatedSize() ); }

	uint32 GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

	void EnqueClothGridMeshRenderCommand(UClothGridMeshComponent* Component)
	{
		AClothManager* ClothManager = AClothManager::GetInstance();
		check(ClothManager != nullptr);

		FClothGridMeshDeformCommand Command;
		Command.Params.NumRow = Component->GetNumRow();
		Command.Params.NumColumn = Component->GetNumColumn();
		Command.Params.NumVertex = Component->GetVertices().Num();
		Command.Params.GridWidth = Component->GetGridWidth();
		Command.Params.GridHeight = Component->GetGridHeight();
		Command.Params.DeltaTime = Component->GetDeltaTime();
		Command.Params.Stiffness = Component->GetStiffness();
		Command.Params.Damping = Component->GetDamping();
		Command.Params.WindVelocity = ClothManager->WindVelocity;
		Command.Params.FluidDensity = Component->GetFluidDensity();
		Command.Params.PreviousInertia = Component->GetPreviousInertia();
		Command.Params.VertexRadius = Component->GetVertexRadius();
		Command.Params.NumIteration = Component->GetNumIteration();

		VertexBuffers.SetAccelerations(Component->GetAccelerations());

		Command.PositionVertexBufferUAV = VertexBuffers.PositionVertexBuffer.GetUAV();
		Command.TangentVertexBufferUAV = VertexBuffers.DeformableMeshVertexBuffer.GetTangentsUAV();
		Command.PrevPositionVertexBufferUAV = VertexBuffers.PrevPositionVertexBuffer.GetUAV();
		Command.AccelerationVertexBufferSRV = VertexBuffers.AcceralationVertexBuffer.GetSRV();

		TArray<class USphereCollisionComponent*> SphereCollisions = ClothManager->GetSphereCollisions();

		TArray<FSphereCollisionParameters> SphereCollisionParams;
		SphereCollisionParams.Reserve(SphereCollisions.Num());

		for (const USphereCollisionComponent* SphereCollision : SphereCollisions)
		{
			const FVector& RelativeCenter = Component->GetComponentTransform().InverseTransformPosition(SphereCollision->GetComponentLocation());
			SphereCollisionParams.Emplace(RelativeCenter, SphereCollision->GetRadius());
		}
		Command.Params.SphereCollisionParams = SphereCollisionParams;

		ClothManager->EnqueueSimulateClothCommand(Command);
	}

private:

	UMaterialInterface* Material;
	FClothVertexBuffers VertexBuffers;
	FDynamicMeshIndexBuffer32 IndexBuffer;
	FLocalVertexFactory VertexFactory;

	FMaterialRelevance MaterialRelevance;
};

//////////////////////////////////////////////////////////////////////////
const FVector UClothGridMeshComponent::GRAVITY = FVector(0.0f, 0.0f, -980.0f);

UClothGridMeshComponent::~UClothGridMeshComponent()
{
	AClothManager* Manager = AClothManager::GetInstance();
	if (Manager != nullptr)
	{
		Manager->UnregisterClothMesh(this);
	}
}

void UClothGridMeshComponent::InitClothSettings(int32 NumRow, int32 NumColumn, float GridWidth, float GridHeight, float Stiffness, float Damping, float FluidDensity, float VertexRadius, int32 NumIteration)
{
	_NumRow = NumRow;
	_NumColumn = NumColumn;
	_GridWidth = GridWidth;
	_GridHeight = GridHeight;
	_Vertices.Reset((NumRow + 1) * (NumColumn + 1));
	_Indices.Reset(NumRow * NumColumn * 2 * 3); // ひとつのグリッドには3つのTriangle、6つの頂点インデックス指定がある
	_Accelerations.Reset((NumRow + 1) * (NumColumn + 1));
	_Stiffness = Stiffness;
	_Damping = Damping;
	_FluidDensity = FluidDensity;
	_VertexRadius = VertexRadius;
	_NumIteration = NumIteration;

	_PrevLocation = GetComponentLocation();
	_CurLinearVelocity = FVector::ZeroVector;
	_PrevLinearVelocity = FVector::ZeroVector;

	//TODO:とりあえずy=0の一行目のみInvMass=0に
	for (int32 x = 0; x < NumColumn + 1; x++)
	{
		_Vertices.Emplace(x * GridWidth, 0.0f, 0.0f, 0.0f);
	}

	//TODO:とりあえずy>0の二行目以降はInvMass=1に
	for (int32 y = 1; y < NumRow + 1; y++)
	{
		for (int32 x = 0; x < NumColumn + 1; x++)
		{
			_Vertices.Emplace(x * GridWidth, y * GridHeight, 0.0f, 1.0f);
		}
	}

	//TODO:とりあえず加速度はZ方向の重力を一律に入れるのみ
	for (int32 y = 0; y < NumRow + 1; y++)
	{
		for (int32 x = 0; x < NumColumn + 1; x++)
		{
			_Accelerations.Emplace(UClothGridMeshComponent::GRAVITY);
		}
	}

	for (int32 Row = 0; Row < NumRow; Row++)
	{
		for (int32 Column = 0; Column < NumColumn; Column++)
		{
			_Indices.Emplace(Row * (NumColumn + 1) + Column);
			_Indices.Emplace((Row + 1) * (NumColumn + 1) + Column);
			_Indices.Emplace((Row + 1) * (NumColumn + 1) + Column + 1);

			_Indices.Emplace(Row * (NumColumn + 1) + Column);
			_Indices.Emplace((Row + 1) * (NumColumn + 1) + Column + 1);
			_Indices.Emplace(Row * (NumColumn + 1) + Column + 1);
		}
	}

	MarkRenderStateDirty();
	UpdateBounds();

	AClothManager* Manager = AClothManager::GetInstance();
	if (Manager == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("UClothGridMeshComponent::OnRegister() There is no AClothManager. So failed to register this cloth mesh."));
	}
	else
	{
		Manager->RegisterClothMesh(this);
	}
}

void UClothGridMeshComponent::IgnoreVelocityDiscontinuityNextFrame()
{
	_IgnoreVelocityDiscontinuityNextFrame = true;
}

FPrimitiveSceneProxy* UClothGridMeshComponent::CreateSceneProxy()
{
	FPrimitiveSceneProxy* Proxy = NULL;
	if(_Vertices.Num() > 0 && _Indices.Num() > 0)
	{
		Proxy = new FClothGridMeshSceneProxy(this);
	}
	return Proxy;
}

void UClothGridMeshComponent::SendRenderDynamicData_Concurrent()
{
	//SCOPE_CYCLE_COUNTER(STAT_ClothGridMeshCompUpdate);
	Super::SendRenderDynamicData_Concurrent();

	AClothManager* ClothManager = AClothManager::GetInstance();

	if (SceneProxy != nullptr && ClothManager != nullptr) // ClothManagerは早期retrunのために取得しているだけ
	{
		// 現在位置から加速度などのパラメータを更新する
		UpdateParamsFromCurrentLocation();

		((FClothGridMeshSceneProxy*)SceneProxy)->EnqueClothGridMeshRenderCommand(this);
	}
}

void UClothGridMeshComponent::UpdateParamsFromCurrentLocation()
{
	_PrevLinearVelocity = _CurLinearVelocity;
	const FVector& CurLocation = GetComponentLocation();

	if (!_IgnoreVelocityDiscontinuityNextFrame)
	{
		_CurLinearVelocity = (CurLocation - _PrevLocation) / GetDeltaTime();
	}

	_PrevLocation = CurLocation;

	_IgnoreVelocityDiscontinuityNextFrame = false;

	float IterDeltaTime = GetDeltaTime() / _NumIteration;

	// このアルファ値の計算はNvClothのものを使っている。なぜこの式なのかは不明。
	float LinearAlpha = 0.5f * (_NumIteration + 1) / _NumIteration;

	const FVector& CurInertia = IterDeltaTime * (_PrevLinearVelocity - _CurLinearVelocity) * LinearAlpha;
	_PreviousInertia = IterDeltaTime * (_PrevLinearVelocity - _CurLinearVelocity) * (1.0f - LinearAlpha);

	for (uint32 i = 0; i < (_NumRow + 1) * (_NumColumn + 1); i++)
	{
		_Accelerations[i] = CurInertia + UClothGridMeshComponent::GRAVITY;
	}
}
