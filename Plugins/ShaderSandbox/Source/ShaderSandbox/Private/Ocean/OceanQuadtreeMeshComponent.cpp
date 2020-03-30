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

	// QuadNodeの境界部分の連続的な変化のため、偶数のグリッド分割でないと適切なジオメトリにできない
	if (NumGridDivision % 2 == 1)
	{
		UE_LOG(LogTemp, Error, TEXT("NumGridDivision must be an even number."));
		return;
	}

	CreateQuadMeshes();

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

void UOceanQuadtreeMeshComponent::CreateQuadMeshes()
{
	// グリッドメッシュ型のVertexBufferやTexCoordsBufferを用意するのはUDeformableGridMeshComponent::Ini:tGridMeshSetting()と同じだが、
	// 接するQuadNodeのLODの差を考慮して数パターンのインデックス配列を用意せねばならないので独自の実装をする
	_NumRow = NumGridDivision;
	_NumColumn = NumGridDivision;
	_GridWidth = GridLength;
	_GridHeight = GridLength;
	_Vertices.Reset((NumGridDivision + 1) * (NumGridDivision + 1));
	_TexCoords.Reset((NumGridDivision + 1) * (NumGridDivision + 1));
	// グリッドをすべて2個のトライアングルで分割した場合のメッシュを81パターン分で確保しておく。正確には境界部分をより大きなトライアングルで
	// 分割するパターンを含むのでもっと少ないが、式が複雑になるので大きめに確保しておく
	_Indices.Reset(81 * 2 * 3 * NumGridDivision * NumGridDivision);

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

	check((uint32)EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST == 0);
	check((uint32)EAdjacentQuadNodeLODDifference::MAX == 3);

	// 右隣がLODが自分以下なのと、一段階上なのと、二段階以上なのの3パターン。左隣、下隣、上隣でも3パターンでそれらの組み合わせで3*3*3*3パターン。
	QuadMeshParams.Reset(81);

	uint32 IndexOffset = 0;
	for (uint32 RightType = (uint32)EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST; RightType < (uint32)EAdjacentQuadNodeLODDifference::MAX; RightType++)
	{
		for (uint32 LeftType = (uint32)EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST; LeftType < (uint32)EAdjacentQuadNodeLODDifference::MAX; LeftType++)
		{
			for (uint32 BottomType = (uint32)EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST; BottomType < (uint32)EAdjacentQuadNodeLODDifference::MAX; BottomType++)
			{
				for (uint32 TopType = (uint32)EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST; TopType < (uint32)EAdjacentQuadNodeLODDifference::MAX; TopType++)
				{
					uint32 NumInnerMeshIndices = CreateInnerMesh();
					uint32 NumBoundaryMeshIndices = CreateBoundaryMesh((EAdjacentQuadNodeLODDifference)RightType, (EAdjacentQuadNodeLODDifference)LeftType, (EAdjacentQuadNodeLODDifference)BottomType, (EAdjacentQuadNodeLODDifference)TopType);
					// TArrayのインデックスは、RightType * 3^3 + LeftType * 3^2 + BottomType * 3^1 + TopType * 3^0となる
					QuadMeshParams.Emplace(IndexOffset, NumInnerMeshIndices + NumBoundaryMeshIndices);
					IndexOffset += NumInnerMeshIndices + NumBoundaryMeshIndices;
				}
			}
		}
	}
}

uint32 UOceanQuadtreeMeshComponent::CreateInnerMesh()
{
	check(NumGridDivision % 2 == 0);

	// 内側の部分はどのメッシュパターンでも同じだが、すべて同じものを作成する。ドローコールでインデックスバッファの不連続アクセスはできないので
	for (int32 Row = 1; Row < NumGridDivision - 1; Row++)
	{
		for (int32 Column = 1; Column < NumGridDivision - 1; Column++)
		{
			// 4隅のグリッドをトライアングル2つに分割する対角線は、メッシュ全体の対角線の方向になっている方が、
			// もし4隅の部分で一方の辺がLOD差がなく一方の辺がLOD差がある場合に、境界のジオメトリを作るのが扱いやすい。
			// よってインデックスが偶数のグリッドと奇数のグリッドで対角線を逆にする。
			// TRIANGLE_STRIPと同じ。
			// NumGridDivisionが偶数であることを前提にしている

			if ((Row + Column) % 2 == 0)
			{
				_Indices.Emplace(GetMeshIndex(Row, Column));
				_Indices.Emplace(GetMeshIndex(Row + 1, Column + 1));
				_Indices.Emplace(GetMeshIndex(Row, Column + 1));

				_Indices.Emplace(GetMeshIndex(Row, Column));
				_Indices.Emplace(GetMeshIndex(Row + 1, Column));
				_Indices.Emplace(GetMeshIndex(Row + 1, Column + 1));
			}
			else
			{
				_Indices.Emplace(GetMeshIndex(Row, Column));
				_Indices.Emplace(GetMeshIndex(Row + 1, Column));
				_Indices.Emplace(GetMeshIndex(Row, Column + 1));

				_Indices.Emplace(GetMeshIndex(Row + 1, Column));
				_Indices.Emplace(GetMeshIndex(Row + 1, Column + 1));
				_Indices.Emplace(GetMeshIndex(Row, Column + 1));
			}
		}
	}

	return 6 * (NumGridDivision - 2) * (NumGridDivision - 2);
}

uint32 UOceanQuadtreeMeshComponent::CreateBoundaryMesh(EAdjacentQuadNodeLODDifference RightAdjLODDiff, EAdjacentQuadNodeLODDifference LeftAdjLODDiff, EAdjacentQuadNodeLODDifference BottomAdjLODDiff, EAdjacentQuadNodeLODDifference TopAdjLODDiff)
{
	check(NumGridDivision % 2 == 0);
	uint32 NumIndices = 0;

	// 境界部分は、LODの差に合わせて隣と接する部分のトライアングルの辺を2倍あるいは4倍にする
	// 隣と接する辺の長いトライアングル単位でインデックスを追加していくため、LODの差が1ならグリッドをたどるループのステップを2、2以上なら4単位で行う

	// Right
	{
		if (RightAdjLODDiff == EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST)
		{
			for (int32 Row = 0; Row < NumGridDivision; Row++)
			{
				if (Row % 2 == 0)
				{
					if (Row > 0) // Bottomと重複しないように
					{
						_Indices.Emplace(GetMeshIndex(Row, 0));
						_Indices.Emplace(GetMeshIndex(Row + 1, 1));
						_Indices.Emplace(GetMeshIndex(Row, 1));
						NumIndices += 3;
					}

					_Indices.Emplace(GetMeshIndex(Row, 0));
					_Indices.Emplace(GetMeshIndex(Row + 1, 0));
					_Indices.Emplace(GetMeshIndex(Row + 1, 1));
					NumIndices += 3;
				}
				else
				{
					_Indices.Emplace(GetMeshIndex(Row, 0));
					_Indices.Emplace(GetMeshIndex(Row + 1, 0));
					_Indices.Emplace(GetMeshIndex(Row, 1));
					NumIndices += 3;

					if (Row < NumGridDivision) // Topと重複しないように
					{
						_Indices.Emplace(GetMeshIndex(Row + 1, 0));
						_Indices.Emplace(GetMeshIndex(Row + 1, 1));
						_Indices.Emplace(GetMeshIndex(Row, 1));
						NumIndices += 3;
					}
				}
			}
		}
		else
		{
			int32 Step = 1 << (int32)RightAdjLODDiff;

			for (int32 Row = 0; Row < NumGridDivision; Row += Step)
			{
				// 接する部分の辺の長いトライアングル
				_Indices.Emplace(GetMeshIndex(Row, 0));
				_Indices.Emplace(GetMeshIndex(Row + Step, 0));
				_Indices.Emplace(GetMeshIndex(Row + (Step >> 1), 1));
				NumIndices += 3;

				// 辺の長いトライアングルの山の下側を埋めるトライアングル
				for (int32 i = 0; i < (Step >> 1); i++)
				{
					if (Row == 0 && i == 0)
					{
						// Bottomと重複しないように4隅のトライアングルの分は作らない
						continue;
					}

					_Indices.Emplace(GetMeshIndex(Row, 0));
					_Indices.Emplace(GetMeshIndex(Row + i + 1, 1));
					_Indices.Emplace(GetMeshIndex(Row + i, 1));
					NumIndices += 3;
				}

				// 辺の長いトライアングルの山の上側を埋めるトライアングル
				for (int32 i = (Step >> 1); i < Step; i++)
				{
					if (Row == (NumGridDivision - Step) && i == (Step - 1))
					{
						// Topと重複しないように4隅のトライアングルの分は作らない
						continue;
					}

					_Indices.Emplace(GetMeshIndex(Row + Step, 0));
					_Indices.Emplace(GetMeshIndex(Row + i + 1, 1));
					_Indices.Emplace(GetMeshIndex(Row + i, 1));
					NumIndices += 3;
				}
			}
		}
	}

	// Left
	{
		if (LeftAdjLODDiff == EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST)
		{
			for (int32 Row = 0; Row < NumGridDivision; Row++)
			{
				if ((Row + NumGridDivision - 1) % 2 == 0)
				{
					_Indices.Emplace(GetMeshIndex(Row, NumGridDivision - 1));
					_Indices.Emplace(GetMeshIndex(Row + 1, NumGridDivision));
					_Indices.Emplace(GetMeshIndex(Row, NumGridDivision));
					NumIndices += 3;

					if (Row < (NumGridDivision - 1)) // Topと重複しないように
					{
						_Indices.Emplace(GetMeshIndex(Row, NumGridDivision - 1));
						_Indices.Emplace(GetMeshIndex(Row + 1, NumGridDivision - 1));
						_Indices.Emplace(GetMeshIndex(Row + 1, NumGridDivision));
						NumIndices += 3;
					}
				}
				else
				{
					if (Row > 0) // Bottomと重複しないように
					{
						_Indices.Emplace(GetMeshIndex(Row, NumGridDivision - 1));
						_Indices.Emplace(GetMeshIndex(Row + 1, NumGridDivision - 1));
						_Indices.Emplace(GetMeshIndex(Row, NumGridDivision));
						NumIndices += 3;
					}

					_Indices.Emplace(GetMeshIndex(Row + 1, NumGridDivision - 1));
					_Indices.Emplace(GetMeshIndex(Row + 1, NumGridDivision));
					_Indices.Emplace(GetMeshIndex(Row, NumGridDivision));
					NumIndices += 3;
				}
			}
		}
		else
		{
			int32 Step = 1 << (int32)LeftAdjLODDiff;

			for (int32 Row = 0; Row < NumGridDivision; Row += Step)
			{
				// 接する部分の辺の長いトライアングル
				_Indices.Emplace(GetMeshIndex(Row, NumGridDivision));
				_Indices.Emplace(GetMeshIndex(Row + (Step >> 1), NumGridDivision - 1));
				_Indices.Emplace(GetMeshIndex(Row + Step, NumGridDivision));
				NumIndices += 3;

				// 辺の長いトライアングルの山の下側を埋めるトライアングル
				for (int32 i = 0; i < (Step >> 1); i++)
				{
					if (Row == 0 && i == 0)
					{
						// Bottomと重複しないように4隅のトライアングルの分は作らない
						continue;
					}

					_Indices.Emplace(GetMeshIndex(Row, NumGridDivision));
					_Indices.Emplace(GetMeshIndex(Row + i, NumGridDivision - 1));
					_Indices.Emplace(GetMeshIndex(Row + i + 1, NumGridDivision - 1));
					NumIndices += 3;
				}

				// 辺の長いトライアングルの山の上側を埋めるトライアングル
				for (int32 i = (Step >> 1); i < Step; i++)
				{
					if (Row == (NumGridDivision - Step) && i == (Step - 1))
					{
						// Topと重複しないように4隅のトライアングルの分は作らない
						continue;
					}

					_Indices.Emplace(GetMeshIndex(Row + Step, NumGridDivision));
					_Indices.Emplace(GetMeshIndex(Row + i, NumGridDivision - 1));
					_Indices.Emplace(GetMeshIndex(Row + i + 1, NumGridDivision - 1));
					NumIndices += 3;
				}
			}
		}
	}

	// Bottom
	{
		if (BottomAdjLODDiff == EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST)
		{
			for (int32 Column = 0; Column < NumGridDivision; Column++)
			{
				if (Column % 2 == 0)
				{
					_Indices.Emplace(GetMeshIndex(0, Column));
					_Indices.Emplace(GetMeshIndex(1, Column + 1));
					_Indices.Emplace(GetMeshIndex(0, Column + 1));
					NumIndices += 3;

					if (Column > 0) // Rightと重複しないように
					{
						_Indices.Emplace(GetMeshIndex(0, Column));
						_Indices.Emplace(GetMeshIndex(1, Column));
						_Indices.Emplace(GetMeshIndex(1, Column + 1));
						NumIndices += 3;
					}
				}
				else
				{
					_Indices.Emplace(GetMeshIndex(0, Column));
					_Indices.Emplace(GetMeshIndex(1, Column));
					_Indices.Emplace(GetMeshIndex(0, Column + 1));
					NumIndices += 3;

					if (Column < NumGridDivision - 1) // Leftと重複しないように
					{
						_Indices.Emplace(GetMeshIndex(1, Column));
						_Indices.Emplace(GetMeshIndex(1, Column + 1));
						_Indices.Emplace(GetMeshIndex(0, Column + 1));
						NumIndices += 3;
					}
				}
			}
		}
		else
		{
			int32 Step = 1 << (int32)BottomAdjLODDiff;

			for (int32 Column = 0; Column < NumGridDivision; Column += Step)
			{
				// 接する部分の辺の長いトライアングル
				_Indices.Emplace(GetMeshIndex(0, Column));
				_Indices.Emplace(GetMeshIndex(1, Column + (Step >> 1)));
				_Indices.Emplace(GetMeshIndex(0, Column + Step));
				NumIndices += 3;

				// 辺の長いトライアングルの山の右側を埋めるトライアングル
				for (int32 i = 0; i < (Step >> 1); i++)
				{
					if (Column == 0 && i == 0)
					{
						// Bottomと重複しないように4隅のトライアングルの分は作らない
						continue;
					}

					_Indices.Emplace(GetMeshIndex(0, Column));
					_Indices.Emplace(GetMeshIndex(1, Column + i));
					_Indices.Emplace(GetMeshIndex(1, Column + i + 1));
					NumIndices += 3;
				}

				// 辺の長いトライアングルの山の左側を埋めるトライアングル
				for (int32 i = (Step >> 1); i < Step; i++)
				{
					if (Column == (NumGridDivision - Step) && i == (Step - 1))
					{
						// Topと重複しないように4隅のトライアングルの分は作らない
						continue;
					}

					_Indices.Emplace(GetMeshIndex(0, Column + Step));
					_Indices.Emplace(GetMeshIndex(1, Column + i));
					_Indices.Emplace(GetMeshIndex(1, Column + i + 1));
					NumIndices += 3;
				}
			}
		}
	}

	// Top
	{
		if (TopAdjLODDiff == EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST)
		{
			for (int32 Column = 0; Column < NumGridDivision; Column++)
			{
				// 4隅のグリッドをトライアングル2つに分割する対角線は、メッシュ全体の対角線の方向になっている方が、
				// もし4隅の部分で一方の辺がLOD差がなく一方の辺がLOD差がある場合に、境界のジオメトリを作るのが扱いやすい。
				// よってインデックスが偶数のグリッドと奇数のグリッドで対角線を逆にする。
				// TRIANGLE_STRIPと同じ。
				// NumGridDivisionが偶数であることを前提にしている

				if ((NumGridDivision - 1 + Column) % 2 == 0)
				{
					if (Column < NumGridDivision - 1) // Leftと重複しないように
					{
						_Indices.Emplace(GetMeshIndex(NumGridDivision - 1, Column));
						_Indices.Emplace(GetMeshIndex(NumGridDivision, Column + 1));
						_Indices.Emplace(GetMeshIndex(NumGridDivision - 1, Column + 1));
						NumIndices += 3;
					}

					_Indices.Emplace(GetMeshIndex(NumGridDivision - 1, Column));
					_Indices.Emplace(GetMeshIndex(NumGridDivision, Column));
					_Indices.Emplace(GetMeshIndex(NumGridDivision, Column + 1));
					NumIndices += 3;
				}
				else
				{
					if (Column > 0) // Rightと重複しないように
					{
						_Indices.Emplace(GetMeshIndex(NumGridDivision - 1, Column));
						_Indices.Emplace(GetMeshIndex(NumGridDivision, Column));
						_Indices.Emplace(GetMeshIndex(NumGridDivision - 1, Column + 1));
						NumIndices += 3;
					}

					_Indices.Emplace(GetMeshIndex(NumGridDivision, Column));
					_Indices.Emplace(GetMeshIndex(NumGridDivision, Column + 1));
					_Indices.Emplace(GetMeshIndex(NumGridDivision - 1, Column + 1));
					NumIndices += 3;
				}
			}
		}
		else
		{
			int32 Step = 1 << (int32)TopAdjLODDiff;

			for (int32 Column = 0; Column < NumGridDivision; Column += Step)
			{
				// 接する部分の辺の長いトライアングル
				_Indices.Emplace(GetMeshIndex(NumGridDivision, Column));
				_Indices.Emplace(GetMeshIndex(NumGridDivision, Column + Step));
				_Indices.Emplace(GetMeshIndex(NumGridDivision - 1, Column + (Step >> 1)));
				NumIndices += 3;

				// 辺の長いトライアングルの山の右側を埋めるトライアングル
				for (int32 i = 0; i < (Step >> 1); i++)
				{
					if (Column == 0 && i == 0)
					{
						// Bottomと重複しないように4隅のトライアングルの分は作らない
						continue;
					}

					_Indices.Emplace(GetMeshIndex(NumGridDivision, Column));
					_Indices.Emplace(GetMeshIndex(NumGridDivision - 1, Column + i + 1));
					_Indices.Emplace(GetMeshIndex(NumGridDivision - 1, Column + i));
					NumIndices += 3;
				}

				// 辺の長いトライアングルの山の左側を埋めるトライアングル
				for (int32 i = (Step >> 1); i < Step; i++)
				{
					if (Column == (NumGridDivision - Step) && i == (Step - 1))
					{
						// Topと重複しないように4隅のトライアングルの分は作らない
						continue;
					}

					_Indices.Emplace(GetMeshIndex(NumGridDivision, Column + Step));
					_Indices.Emplace(GetMeshIndex(NumGridDivision - 1, Column + i + 1));
					_Indices.Emplace(GetMeshIndex(NumGridDivision - 1, Column + i));
					NumIndices += 3;
				}
			}
		}
	}

	return NumIndices;
}

int32 UOceanQuadtreeMeshComponent::GetMeshIndex(int32 Row, int32 Column)
{
	return Row * (NumGridDivision + 1) + Column;
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

