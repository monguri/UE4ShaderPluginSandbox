#include "Ocean/OceanQuadtreeMeshComponent.h"
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
#include "DeformMesh/DeformableVertexBuffers.h"
#include "Ocean/OceanSimulator.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "ResourceArrayStructuredBuffer.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollectionInstance.h"

using namespace Quadtree;
using namespace OceanSimulator;

/** almost all is copy of FCustomMeshSceneProxy. */
class FOceanQuadtreeMeshSceneProxy final : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	FOceanQuadtreeMeshSceneProxy(UOceanQuadtreeMeshComponent* Component)
		: FPrimitiveSceneProxy(Component)
		, VertexFactory(GetScene().GetFeatureLevel(), "FOceanQuadtreeMeshSceneProxy")
		, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
		, LODMIDList(Component->GetLODMIDList())
		, MPCInstance(Component->GetMPCInstance())
		, QuadMeshParams(Component->GetQuadMeshParams())
		, NumGridDivision(Component->NumGridDivision)
		, GridLength(Component->GridLength)
		, MaxLOD(Component->MaxLOD)
		, GridMaxPixelCoverage(Component->GridMaxPixelCoverage)
		, PatchLength(Component->PatchLength)
	{
		TArray<FDynamicMeshVertex> Vertices;
		Vertices.Reset(Component->GetVertices().Num());
		IndexBuffer.Indices = Component->GetIndices();

		for (int32 VertIdx = 0; VertIdx < Component->GetVertices().Num(); VertIdx++)
		{
			// TODO:TangentはとりあえずFDynamicMeshVertexのデフォルト値まかせにする。ColorはDynamicMeshVertexのデフォルト値を採用している
			Vertices.Emplace(Component->GetVertices()[VertIdx], Component->GetTexCoords()[VertIdx], FColor(255, 255, 255));
		}
		VertexBuffers.InitFromDynamicVertex(&VertexFactory, Vertices);

		// Enqueue initialization of render resource
		BeginInitResource(&VertexBuffers.PositionVertexBuffer);
		BeginInitResource(&VertexBuffers.DeformableMeshVertexBuffer);
		BeginInitResource(&VertexBuffers.ColorVertexBuffer);
		BeginInitResource(&IndexBuffer);
		BeginInitResource(&VertexFactory);

		// Grab material
		Material = Component->GetMaterial(0);
		if(Material == NULL)
		{
			Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}

		// GetDynamicMeshElements()のあと、VerifyUsedMaterial()によってマテリアルがコンポーネントにあったものかチェックされるので
		// SetUsedMaterialForVerification()で登録する手もあるが、レンダースレッド出ないとcheckにひっかかるので
		bVerifyUsedMaterials = false;


		int32 SizeX, SizeY;
		Component->GetDisplacementMap()->GetSize(SizeX, SizeY);
		check(SizeX == SizeY); // 正方形である前提
		check(FMath::IsPowerOfTwo(SizeX)); // 2の累乗のサイズである前提
		uint32 DispMapDimension = SizeX;
		
		// Phyllips Spectrumを使った初期化
		// Height map H(0)
		H0Data.Init(FComplex::ZeroVector, DispMapDimension * DispMapDimension);
		// FComplex::ZeroVectorという名前が少し格好悪いが、Zeroみたいな新しい定数を作ろうと思うとtypedef FVector2D FComplexではできずFVector2Dを包含したFComplex構造体を作らねばならないので今は妥協する

		Omega0Data.Init(0.0f, DispMapDimension * DispMapDimension);

		FOceanSpectrumParameters Params;
		Params.DispMapDimension = DispMapDimension;
		Params.PatchLength = PatchLength;
		Params.AmplitudeScale = Component->AmplitudeScale;
		Params.WindDirection = Component->WindDirection.GetSafeNormal();
		Params.WindSpeed = Component->WindSpeed;
		Params.WindDependency = Component->WindDependency;
		Params.ChoppyScale = Component->ChoppyScale;
		Params.AccumulatedTime = Component->GetAccumulatedTime() * Component->TimeScale;
		Params.DxyzDebugAmplitude = Component->DxyzDebugAmplitude;

		CreateInitialHeightMap(Params, Component->GetWorld()->GetGravityZ(), H0Data, Omega0Data);

		H0Buffer.Initialize(H0Data, sizeof(FComplex));
		Omega0Buffer.Initialize(Omega0Data, sizeof(float));

		HtZeroInitData.Init(FComplex::ZeroVector, DispMapDimension * DispMapDimension);
		HtBuffer.Initialize(HtZeroInitData, sizeof(FComplex));

		DkxZeroInitData.Init(FComplex::ZeroVector, DispMapDimension * DispMapDimension);
		DkxBuffer.Initialize(DkxZeroInitData, sizeof(FComplex));

		DkyZeroInitData.Init(FComplex::ZeroVector, DispMapDimension * DispMapDimension);
		DkyBuffer.Initialize(DkyZeroInitData, sizeof(FComplex));

		FFTWorkZeroInitData.Init(FComplex::ZeroVector, DispMapDimension * DispMapDimension);
		FFTWorkBuffer.Initialize(FFTWorkZeroInitData, sizeof(FComplex));

		DxZeroInitData.Init(0.0f, DispMapDimension * DispMapDimension);
		DxBuffer.Initialize(DxZeroInitData, sizeof(float));

		DyZeroInitData.Init(0.0f, DispMapDimension * DispMapDimension);
		DyBuffer.Initialize(DyZeroInitData, sizeof(float));

		DzZeroInitData.Init(0.0f, DispMapDimension * DispMapDimension);
		DzBuffer.Initialize(DyZeroInitData, sizeof(float));
	}

	virtual ~FOceanQuadtreeMeshSceneProxy()
	{
		VertexBuffers.PositionVertexBuffer.ReleaseResource();
		VertexBuffers.DeformableMeshVertexBuffer.ReleaseResource();
		VertexBuffers.ColorVertexBuffer.ReleaseResource();
		IndexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();
		H0Buffer.ReleaseResource();
		Omega0Buffer.ReleaseResource();
		HtBuffer.ReleaseResource();
		FFTWorkBuffer.ReleaseResource();
		DkxBuffer.ReleaseResource();
		DkyBuffer.ReleaseResource();
		DxBuffer.ReleaseResource();
		DyBuffer.ReleaseResource();
		DzBuffer.ReleaseResource();
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		QUICK_SCOPE_CYCLE_COUNTER( STAT_OceanQuadtreeMeshSceneProxy_GetDynamicMeshElements );

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

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* View = Views[ViewIndex];

				// RootNodeの辺の長さはPatchLengthを2のMaxLOD乗したサイズ
				FQuadNode RootNode;
				RootNode.Length = PatchLength * (1 << MaxLOD);
				RootNode.BottomRight = GetLocalToWorld().GetOrigin() + FVector(-RootNode.Length * 0.5f, -RootNode.Length * 0.5f, 0.0f);
				RootNode.LOD = MaxLOD;

				// Area()という関数もあるが、大きな数で割って精度を落とさないように2段階で割る
				float MaxScreenCoverage = (float)GridMaxPixelCoverage * GridMaxPixelCoverage / View->UnscaledViewRect.Width() / View->UnscaledViewRect.Height();

				TArray<FQuadNode> QuadNodeList;
				QuadNodeList.Reserve(512); //TODO:とりあえず512。ここで毎回確保は処理不可が無駄だが、この関数がconstなのでとりあえず
				TArray<FQuadNode> RenderList;
				RenderList.Reserve(512); //TODO:とりあえず512。ここで毎回確保は処理不可が無駄だが、この関数がconstなのでとりあえず

				Quadtree::BuildQuadtree(MaxLOD, NumGridDivision, MaxScreenCoverage, PatchLength, View->ViewMatrices.GetViewOrigin(), View->ViewMatrices.GetProjectionScale(), View->ViewMatrices.GetViewProjectionMatrix(), RootNode, QuadNodeList, RenderList);

				for (const FQuadNode& Node : RenderList)
				{
					if(!bWireframe)
					{
						MaterialProxy = LODMIDList[Node.LOD]->GetRenderProxy();
					}

					// 隣接するノードとのLODの差から、使用するメッシュトポロジーを用意したもののなかから選択する
					const FVector2D& RightAdjPos = FVector2D(Node.BottomRight.X - PatchLength * 0.5f, Node.BottomRight.Y + Node.Length * 0.5f);
					const FVector2D& LeftAdjPos = FVector2D(Node.BottomRight.X + Node.Length + PatchLength * 0.5f, Node.BottomRight.Y + Node.Length * 0.5f);
					const FVector2D& BottomAdjPos = FVector2D(Node.BottomRight.X + Node.Length * 0.5f, Node.BottomRight.Y - PatchLength * 0.5f);
					const FVector2D& TopAdjPos = FVector2D(Node.BottomRight.X + Node.Length * 0.5f, Node.BottomRight.Y + Node.Length + PatchLength * 0.5f);

					EAdjacentQuadNodeLODDifference RightAdjLODDiff = Quadtree::QueryAdjacentNodeType(Node, RightAdjPos, RenderList);
					EAdjacentQuadNodeLODDifference LeftAdjLODDiff = Quadtree::QueryAdjacentNodeType(Node, LeftAdjPos, RenderList);
					EAdjacentQuadNodeLODDifference BottomAdjLODDiff = Quadtree::QueryAdjacentNodeType(Node, BottomAdjPos, RenderList);
					EAdjacentQuadNodeLODDifference TopAdjLODDiff = Quadtree::QueryAdjacentNodeType(Node, TopAdjPos, RenderList);

					// 3進数によって4つの隣ノードのタイプとインデックスを対応させる
					uint32 QuadMeshParamsIndex = 27 * (uint32)RightAdjLODDiff + 9 * (uint32)LeftAdjLODDiff + 3 * (uint32)BottomAdjLODDiff + (uint32)TopAdjLODDiff;
					const FQuadMeshParameter& MeshParams = QuadMeshParams[QuadMeshParamsIndex];

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

					// メッシュサイズをQuadNodeのサイズに応じてスケールさせる
					float MeshScale = Node.Length / (NumGridDivision * GridLength);

					FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
					// FPrimitiveSceneProxy::ApplyLateUpdateTransform()を参考にした
					const FMatrix& NewLocalToWorld = GetLocalToWorld().ApplyScale(MeshScale).ConcatTranslation(FVector(Node.BottomRight.X, Node.BottomRight.Y, 0.0f));
					PreviousLocalToWorld = PreviousLocalToWorld.ApplyScale(MeshScale).ConcatTranslation(FVector(Node.BottomRight.X, Node.BottomRight.Y, 0.0f));

					// コンポーネントのCalcBounds()はRootNodeのサイズに合わせて実装されているのでスケールさせる。
					// 実際は描画するメッシュはQuadtreeを作る際の独自フラスタムカリングで選抜しているのでBoundsはすべてRootNodeサイズでも
					// 構わないが、一応正確なサイズにしておく
					float BoundScale = Node.Length / RootNode.Length;

					FBoxSphereBounds NewBounds = GetBounds();
					NewBounds = FBoxSphereBounds(NewBounds.Origin + NewLocalToWorld.TransformPosition(FVector(Node.BottomRight.X, Node.BottomRight.Y, 0.0f)), NewBounds.BoxExtent * BoundScale, NewBounds.SphereRadius * BoundScale);

					FBoxSphereBounds NewLocalBounds = GetLocalBounds();
					NewLocalBounds = FBoxSphereBounds(NewLocalBounds.Origin, NewLocalBounds.BoxExtent * BoundScale, NewLocalBounds.SphereRadius * BoundScale);

					DynamicPrimitiveUniformBuffer.Set(NewLocalToWorld, PreviousLocalToWorld, NewBounds, NewLocalBounds, true, bHasPrecomputedVolumetricLightmap, DrawsVelocity(), false);
					BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

					BatchElement.FirstIndex = MeshParams.IndexBufferOffset;
					BatchElement.NumPrimitives = MeshParams.NumIndices / 3;
					BatchElement.MinVertexIndex = 0;
					BatchElement.MaxVertexIndex = VertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;
					Mesh.ReverseCulling = (NewLocalToWorld.Determinant() < 0.0f);
					Mesh.Type = PT_TriangleList;
					Mesh.DepthPriorityGroup = SDPG_World;
					Mesh.bCanApplyViewModeOverrides = false;
					Collector.AddMesh(ViewIndex, Mesh);
				}
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

	void EnqueSimulateOceanCommand(FRHICommandListImmediate& RHICmdList, UOceanQuadtreeMeshComponent* Component) const
	{
		FTextureRenderTargetResource* TextureRenderTargetResource = Component->GetDisplacementMap()->GetRenderTargetResource();
		if (TextureRenderTargetResource == nullptr)
		{
			return;
		}

		FOceanSpectrumParameters Params;
		Params.DispMapDimension = TextureRenderTargetResource->GetSizeX(); // TODO:正方形前提でSizeYは見てない
		Params.PatchLength = PatchLength;
		Params.AmplitudeScale = Component->AmplitudeScale;
		Params.WindDirection = Component->WindDirection.GetSafeNormal();
		Params.WindSpeed = Component->WindSpeed;
		Params.WindDependency = Component->WindDependency;
		Params.ChoppyScale = Component->ChoppyScale;
		Params.AccumulatedTime = Component->GetAccumulatedTime() * Component->TimeScale;
		Params.DxyzDebugAmplitude = Component->DxyzDebugAmplitude;

		// レンダースレッド内でやればコマンドキューに別のコマンドを発行せずに即実行して無駄がないのでここでやる
		const FVector2D& PerlinUVOffset = -Params.WindDirection * Params.AccumulatedTime * Component->PerlinUVSpeed; // 風の方向と逆方向にしている
		if (MPCInstance != nullptr)
		{
			MPCInstance->SetVectorParameterValue(FName("PerlinUVOffset"), FVector(PerlinUVOffset.X, PerlinUVOffset.Y, 0.0f));
		}

		FOceanBufferViews Views;
		Views.H0SRV = H0Buffer.GetSRV();
		Views.OmegaSRV = Omega0Buffer.GetSRV();
		Views.HtSRV = HtBuffer.GetSRV();
		Views.HtUAV = HtBuffer.GetUAV();
		Views.DkxSRV = DkxBuffer.GetSRV();
		Views.DkxUAV = DkxBuffer.GetUAV();
		Views.DkySRV = DkyBuffer.GetSRV();
		Views.DkyUAV = DkyBuffer.GetUAV();
		Views.FFTWorkUAV = FFTWorkBuffer.GetUAV();
		Views.DxSRV = DxBuffer.GetSRV();
		Views.DxUAV = DxBuffer.GetUAV();
		Views.DySRV = DyBuffer.GetSRV();
		Views.DyUAV = DyBuffer.GetUAV();
		Views.DzSRV = DzBuffer.GetSRV();
		Views.DzUAV = DzBuffer.GetUAV();
		Views.H0DebugViewUAV = Component->GetH0DebugViewUAV();
		Views.HtDebugViewUAV = Component->GetHtDebugViewUAV();
		Views.DkxDebugViewUAV = Component->GetDkxDebugViewUAV();
		Views.DkyDebugViewUAV = Component->GetDkyDebugViewUAV();
		Views.DxyzDebugViewUAV = Component->GetDxyzDebugViewUAV();
		Views.DisplacementMapSRV = Component->GetDisplacementMapSRV();
		Views.DisplacementMapUAV = Component->GetDisplacementMapUAV();
		Views.GradientFoldingMapUAV = Component->GetGradientFoldingMapUAV();

		SimulateOcean(RHICmdList, Params, Views);
	}

private:
	UMaterialInterface* Material;
	FDeformableVertexBuffers VertexBuffers;
	FDynamicMeshIndexBuffer32 IndexBuffer;
	FLocalVertexFactory VertexFactory;
	FMaterialRelevance MaterialRelevance;

	TResourceArray<FComplex> H0Data;
	TResourceArray<float> Omega0Data;
	TResourceArray<FComplex> HtZeroInitData;
	TResourceArray<FComplex> DkxZeroInitData;
	TResourceArray<FComplex> DkyZeroInitData;
	TResourceArray<FComplex> FFTWorkZeroInitData;
	TResourceArray<float> DxZeroInitData;
	TResourceArray<float> DyZeroInitData;
	TResourceArray<float> DzZeroInitData;
	FResourceArrayStructuredBuffer H0Buffer;
	FResourceArrayStructuredBuffer Omega0Buffer;
	FResourceArrayStructuredBuffer HtBuffer;
	FResourceArrayStructuredBuffer DkxBuffer;
	FResourceArrayStructuredBuffer DkyBuffer;
	FResourceArrayStructuredBuffer FFTWorkBuffer;
	FResourceArrayStructuredBuffer DxBuffer;
	FResourceArrayStructuredBuffer DyBuffer;
	FResourceArrayStructuredBuffer DzBuffer;

	TArray<UMaterialInstanceDynamic*> LODMIDList; // Component側でUMaterialInstanceDynamicは保持されてるのでGCで解放はされない
	UMaterialParameterCollectionInstance* MPCInstance = nullptr; // Component側でUMaterialInstanceDynamicは保持されてるのでGCで解放はされない
	TArray<Quadtree::FQuadMeshParameter> QuadMeshParams;
	int32 NumGridDivision;
	float GridLength;
	int32 MaxLOD;
	int32 GridMaxPixelCoverage;
	int32 PatchLength;
};

//////////////////////////////////////////////////////////////////////////

UOceanQuadtreeMeshComponent::UOceanQuadtreeMeshComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

UOceanQuadtreeMeshComponent::~UOceanQuadtreeMeshComponent()
{
	if (_DisplacementMapSRV.IsValid())
	{
		_DisplacementMapSRV.SafeRelease();
	}

	if (_DisplacementMapUAV.IsValid())
	{
		_DisplacementMapUAV.SafeRelease();
	}

	if (_GradientFoldingMapUAV.IsValid())
	{
		_GradientFoldingMapUAV.SafeRelease();
	}

	if (_H0DebugViewUAV.IsValid())
	{
		_H0DebugViewUAV.SafeRelease();
	}

	if (_HtDebugViewUAV.IsValid())
	{
		_HtDebugViewUAV.SafeRelease();
	}

	if (_DkxDebugViewUAV.IsValid())
	{
		_DkxDebugViewUAV.SafeRelease();
	}

	if (_DkyDebugViewUAV.IsValid())
	{
		_DkyDebugViewUAV.SafeRelease();
	}

	if (_DxyzDebugViewUAV.IsValid())
	{
		_DxyzDebugViewUAV.SafeRelease();
	}
}

FPrimitiveSceneProxy* UOceanQuadtreeMeshComponent::CreateSceneProxy()
{
	FPrimitiveSceneProxy* Proxy = NULL;
	if(_Vertices.Num() > 0 && _Indices.Num() > 0 && DisplacementMap != nullptr && GradientFoldingMap != nullptr)
	{
		Proxy = new FOceanQuadtreeMeshSceneProxy(this);
	}
	return Proxy;
}

void UOceanQuadtreeMeshComponent::OnRegister()
{
	Super::OnRegister();

	// グリッドメッシュ型のVertexBufferやTexCoordsBufferを用意するのはUDeformableGridMeshComponent::Ini:tGridMeshSetting()と同じだが、
	// 接するQuadNodeのLODの差を考慮して数パターンのインデックス配列を用意せねばならないので独自の実装をする
	_NumRow = NumGridDivision;
	_NumColumn = NumGridDivision;
	_GridWidth = GridLength;
	_GridHeight = GridLength;
	_Vertices.Reset((NumGridDivision + 1) * (NumGridDivision + 1));
	_TexCoords.Reset((NumGridDivision + 1) * (NumGridDivision + 1));

	// ここでは正方形の中心を原点にする平行移動やLODに応じたスケールはしない。実際にメッシュを描画に渡すときに平行移動とスケールを行う。

	for (int32 y = 0; y < NumGridDivision + 1; y++)
	{
		for (int32 x = 0; x < NumGridDivision + 1; x++)
		{
			_Vertices.Emplace(x * GridLength, y * GridLength, 0.0f, 0.0f);
		}
	}

	for (int32 y = 0; y < NumGridDivision + 1; y++)
	{
		for (int32 x = 0; x < NumGridDivision + 1; x++)
		{
			_TexCoords.Emplace((float)x / NumGridDivision, (float)y / NumGridDivision);
		}
	}

	// QuadNodeの境界部分の連続的な変化のため、偶数のグリッド分割でないと適切なジオメトリにできない
	if (NumGridDivision % 2 == 1)
	{
		UE_LOG(LogTemp, Error, TEXT("NumGridDivision must be an even number."));
		return;
	}

	Quadtree::CreateQuadMeshes(NumGridDivision, _Indices, QuadMeshParams);

	if (_DisplacementMapSRV.IsValid())
	{
		_DisplacementMapSRV.SafeRelease();
	}

	if (DisplacementMap != nullptr)
	{
		_DisplacementMapSRV = RHICreateShaderResourceView(DisplacementMap->GameThread_GetRenderTargetResource()->TextureRHI, 0);
	}

	if (_DisplacementMapUAV.IsValid())
	{
		_DisplacementMapUAV.SafeRelease();
	}

	if (DisplacementMap != nullptr)
	{
		_DisplacementMapUAV = RHICreateUnorderedAccessView(DisplacementMap->GameThread_GetRenderTargetResource()->TextureRHI);
	}

	if (_GradientFoldingMapUAV.IsValid())
	{
		_GradientFoldingMapUAV.SafeRelease();
	}

	if (GradientFoldingMap != nullptr)
	{
		_GradientFoldingMapUAV = RHICreateUnorderedAccessView(GradientFoldingMap->GameThread_GetRenderTargetResource()->TextureRHI);
	}

	if (_H0DebugViewUAV.IsValid())
	{
		_H0DebugViewUAV.SafeRelease();
	}

	if (H0DebugView != nullptr)
	{
		_H0DebugViewUAV = RHICreateUnorderedAccessView(H0DebugView->GameThread_GetRenderTargetResource()->TextureRHI);
	}

	if (_HtDebugViewUAV.IsValid())
	{
		_HtDebugViewUAV.SafeRelease();
	}

	if (HtDebugView != nullptr)
	{
		_HtDebugViewUAV = RHICreateUnorderedAccessView(HtDebugView->GameThread_GetRenderTargetResource()->TextureRHI);
	}

	if (_DkxDebugViewUAV.IsValid())
	{
		_DkxDebugViewUAV.SafeRelease();
	}

	if (DkxDebugView != nullptr)
	{
		_DkxDebugViewUAV = RHICreateUnorderedAccessView(DkxDebugView->GameThread_GetRenderTargetResource()->TextureRHI);
	}

	if (_DkyDebugViewUAV.IsValid())
	{
		_DkyDebugViewUAV.SafeRelease();
	}

	if (DkyDebugView != nullptr)
	{
		_DkyDebugViewUAV = RHICreateUnorderedAccessView(DkyDebugView->GameThread_GetRenderTargetResource()->TextureRHI);
	}

	if (DxyzDebugView != nullptr)
	{
		_DxyzDebugViewUAV = RHICreateUnorderedAccessView(DxyzDebugView->GameThread_GetRenderTargetResource()->TextureRHI);
	}

	_MPCInstance = nullptr;
	if (OceanMPC != nullptr)
	{
		_MPCInstance = GetWorld()->GetParameterCollectionInstance(OceanMPC);
		_MPCInstance->SetVectorParameterValue(FName("PerlinDisplacement"), PerlinDisplacement);
		_MPCInstance->SetVectorParameterValue(FName("PerlinGradient"), PerlinGradient);
		// UVスケールが整数の逆数にならないと、ループ構造でperinノイズを使っている以上、境界部分でずれが起きる。よってUPROPERTYでは逆数をFIntVectorで扱っている。
		// PerlinUVScaleはLODのUVスケールで決められたパッチの中でさらにスケールさせるものである。
		_MPCInstance->SetVectorParameterValue(FName("PerlinUVScale"), FVector(1.0f / PerlinUVInvScale.X, 1.0f / PerlinUVInvScale.Y, 1.0f / PerlinUVInvScale.Z));
	}

	UMaterialInterface* Material = GetMaterial(0);
	if(Material == NULL)
	{
		Material = UMaterial::GetDefaultMaterial(MD_Surface);
	}

	// QuadNodeの数は、MaxLOD-2が最小レベルなのですべて最小のQuadNodeで敷き詰めると
	// 2^(MaxLOD-2)*2^(MaxLOD-2)
	// 通常はそこまでいかない。カメラから遠くなるにつれて2倍になっていけば
	// 2*6=12になるだろう。それは原点にカメラがあるときで、かつカメラの高さが0に近いときなので、
	// せいぜその4倍程度の数と見積もってでいいはず

	float InvMaxLOD = 1.0f / MaxLOD;
	//LODMIDList.SetNumZeroed(48);
	//for (int32 i = 0; i < 48; i++)
	_LODMIDList.SetNumZeroed(MaxLOD + 1);
	for (int32 LOD = 0; LOD < MaxLOD + 1; LOD++)
	{
		_LODMIDList[LOD] = UMaterialInstanceDynamic::Create(Material, this);
		_LODMIDList[LOD]->SetScalarParameterValue(FName("UVScale"), (float)(1 << LOD));
		_LODMIDList[LOD]->SetVectorParameterValue(FName("LODColor"), FLinearColor((MaxLOD - LOD) * InvMaxLOD, 0.0f, LOD * InvMaxLOD));
	}

	MarkRenderStateDirty();
	UpdateBounds();
}

FBoxSphereBounds UOceanQuadtreeMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	// QuadtreeのRootNodeのサイズにしておく。アクタのBPエディタのビューポート表示やフォーカス操作などでこのBoundが使われるのでなるべく正確にする
	// また、QuadNodeの各メッシュのBoundを計算する基準サイズとしても使う。
	float HalfRootNodeLength = PatchLength * (1 << (MaxLOD - 1));
	const FVector& Min = LocalToWorld.TransformPosition(FVector(-HalfRootNodeLength, -HalfRootNodeLength, 0.0f));
	const FVector& Max = LocalToWorld.TransformPosition(FVector(HalfRootNodeLength, HalfRootNodeLength, 0.0f));
	FBox Box(Min, Max);

	const FBoxSphereBounds& Ret = FBoxSphereBounds(Box);
	return Ret;
}

void UOceanQuadtreeMeshComponent::SendRenderDynamicData_Concurrent()
{
	//SCOPE_CYCLE_COUNTER(STAT_OceanQuadtreeMeshCompUpdate);
	Super::SendRenderDynamicData_Concurrent();

	if (SceneProxy != nullptr
		&& DisplacementMap != nullptr
		&& _DisplacementMapSRV.IsValid()
		&& _DisplacementMapUAV.IsValid()
		&& GradientFoldingMap != nullptr
		&& _GradientFoldingMapUAV.IsValid()
		&& _H0DebugViewUAV.IsValid()
		&& _HtDebugViewUAV.IsValid()
		&& _DkxDebugViewUAV.IsValid()
		&& _DkyDebugViewUAV.IsValid()
		&& _DxyzDebugViewUAV.IsValid()
	)
	{
		ENQUEUE_RENDER_COMMAND(OceanDeformGridMeshCommand)(
			[this](FRHICommandListImmediate& RHICmdList)
			{
				if (SceneProxy != nullptr
					&& DisplacementMap != nullptr
					&& _DisplacementMapSRV.IsValid()
					&& _DisplacementMapUAV.IsValid()
					&& GradientFoldingMap != nullptr
					&& _GradientFoldingMapUAV.IsValid()
					&& _H0DebugViewUAV.IsValid()
					&& _HtDebugViewUAV.IsValid()
					&& _DkxDebugViewUAV.IsValid()
					&& _DkyDebugViewUAV.IsValid()
					&& _DxyzDebugViewUAV.IsValid()
					)
				{
					((const FOceanQuadtreeMeshSceneProxy*)SceneProxy)->EnqueSimulateOceanCommand(RHICmdList, this);
				}
			}
		);
	}
}

const TArray<class UMaterialInstanceDynamic*>& UOceanQuadtreeMeshComponent::GetLODMIDList() const
{
	return _LODMIDList;
}

UMaterialParameterCollectionInstance* UOceanQuadtreeMeshComponent::GetMPCInstance() const
{
	return _MPCInstance;
}

const TArray<Quadtree::FQuadMeshParameter>& UOceanQuadtreeMeshComponent::GetQuadMeshParams() const
{
	return QuadMeshParams;
}

