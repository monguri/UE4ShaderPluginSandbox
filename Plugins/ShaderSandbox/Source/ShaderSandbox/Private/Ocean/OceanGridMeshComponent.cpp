#include "Ocean/OceanGridMeshComponent.h"
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

namespace
{
/**
 * 平均0、標準偏差1のガウシアン分布でランダム値を生成する。
 */
float GaussianRand()
{
	float U1 = FMath::FRand();
	float U2 = FMath::FRand();

	if (U1 < SMALL_NUMBER)
	{
		U1 = SMALL_NUMBER;
	}

	// TODO:計算の理屈がわからん
	return FMath::Sqrt(-2.0f * FMath::Loge(U1)) * FMath::Cos(2.0f * PI * U2);
}

/**
 * Phillips Spectrum
 * K: 正規化された波数ベクトル
 */
float CalculatePhillipsCoefficient(const FVector2D& K, float Gravity, const FOceanSpectrumParameters& Params)
{
	float Amplitude = Params.AmplitudeScale * 1.0e-7f; //TODO：根拠不明の値

	// この風速において最大の波長の波。
	float MaxLength = Params.WindSpeed * Params.WindSpeed / Gravity;

	float KSqr = K.X * K.X + K.Y * K.Y;
	float KCos = K.X * Params.WindDirection.X + K.Y * Params.WindDirection.Y;
	float Phillips = Amplitude * FMath::Exp(-1.0f / (MaxLength * MaxLength * KSqr)) / (KSqr * KSqr * KSqr) * (KCos * KCos); // TODO:KSqrは2乗すればK^4になるから十分では？

	// 逆方向の波は弱くする
	if (KCos < 0)
	{
		Phillips *= Params.WindDependency;
	}

	// 最大波長よりもずっと小さい波は削減する。Tessendorfの計算にはなかった要素。TODO:いるのか？
	float CutLength = MaxLength / 1000;
	return Phillips * FMath::Exp(-KSqr * CutLength * CutLength);
}

void InitHeightMap(const FOceanSpectrumParameters& Params, float GravityZ, TResourceArray<FVector2D>& OutH0, TResourceArray<float>& OutOmega0)
{
	FVector2D K;
	float GravityConstant = FMath::Abs(GravityZ);

	FMath::RandInit(FPlatformTime::Cycles());

	for (uint32 i = 0; i < Params.DispMapDimension; i++)
	{
		// Kは正規化された波数ベクトル。範囲は[-|DX/W, |DX/W], [-|DY/H, |DY/H]
		K.Y = (-(int32)Params.DispMapDimension / 2.0f + i) * (2 * PI / Params.PatchLength);

		for (uint32 j = 0; j < Params.DispMapDimension; j++)
		{
			K.X = (-(int32)Params.DispMapDimension / 2.0f + j) * (2 * PI / Params.PatchLength);

			float PhillipsCoef = CalculatePhillipsCoefficient(K, GravityConstant, Params);
			float PhillipsSqrt = (K.X == 0 || K.Y == 0) ? 0.0f : FMath::Sqrt(PhillipsCoef);
			OutH0[i * (Params.DispMapDimension) + j].X = PhillipsSqrt * GaussianRand() * UE_HALF_SQRT_2;
			OutH0[i * (Params.DispMapDimension) + j].Y = PhillipsSqrt * GaussianRand() * UE_HALF_SQRT_2;

			// The angular frequency is following the dispersion relation:
			// OutOmega0^2 = g * k
			OutOmega0[i * (Params.DispMapDimension) + j] = FMath::Sqrt(GravityConstant * K.Size());
		}
	}
}
} // namespace

/** almost all is copy of FCustomMeshSceneProxy. */
class FOceanGridMeshSceneProxy final : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	FOceanGridMeshSceneProxy(UOceanGridMeshComponent* Component)
		: FPrimitiveSceneProxy(Component)
		, VertexFactory(GetScene().GetFeatureLevel(), "FOceanGridMeshSceneProxy")
		, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
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

		int32 SizeX, SizeY;
		Component->GetDisplacementMap()->GetSize(SizeX, SizeY);
		check(SizeX == SizeY); // 正方形である前提
		check(FMath::IsPowerOfTwo(SizeX)); // 2の累乗のサイズである前提
		uint32 DispMapDimension = SizeX;
		
		// Phyllips Spectrumを使った初期化
		// Height map H(0)
		H0Data.Init(FVector2D::ZeroVector, DispMapDimension * DispMapDimension);

		Omega0Data.Init(0.0f, DispMapDimension * DispMapDimension);

		FOceanSpectrumParameters Params;
		Params.DispMapDimension = DispMapDimension;
		Params.PatchLength = Component->GetGridWidth() * Component->GetNumColumn();
		Params.AmplitudeScale = Component->GetAmplitudeScale();
		Params.WindDirection = Component->GetWindDirection();
		Params.WindSpeed = Component->GetWindSpeed();
		Params.WindDependency = Component->GetWindDependency();
		Params.ChoppyScale = Component->GetChoppyScale();
		Params.AccumulatedTime = Component->GetAccumulatedTime() * Component->GetTimeScale();

		InitHeightMap(Params, Component->GetWorld()->GetGravityZ(), H0Data, Omega0Data);

		H0Buffer.Initialize(H0Data, sizeof(FVector2D));
		Omega0Buffer.Initialize(Omega0Data, sizeof(float));

		HtZeroInitData.Init(0.0f, DispMapDimension * DispMapDimension * 2);
		HtBuffer.Initialize(HtZeroInitData, 2 * sizeof(float));

		DkxZeroInitData.Init(0.0f, DispMapDimension * DispMapDimension * 2);
		DkxBuffer.Initialize(DkxZeroInitData, 2 * sizeof(float));

		DkyZeroInitData.Init(0.0f, DispMapDimension * DispMapDimension * 2);
		DkyBuffer.Initialize(DkyZeroInitData, 2 * sizeof(float));

		DxyzZeroInitData.Init(0.0f, 3 * DispMapDimension * DispMapDimension * 2);
		DxyzBuffer.Initialize(DxyzZeroInitData, 2 * sizeof(float));
	}

	virtual ~FOceanGridMeshSceneProxy()
	{
		VertexBuffers.PositionVertexBuffer.ReleaseResource();
		VertexBuffers.DeformableMeshVertexBuffer.ReleaseResource();
		VertexBuffers.ColorVertexBuffer.ReleaseResource();
		IndexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();
		H0Buffer.ReleaseResource();
		Omega0Buffer.ReleaseResource();
		HtBuffer.ReleaseResource();
		DkxBuffer.ReleaseResource();
		DkyBuffer.ReleaseResource();
		DxyzBuffer.ReleaseResource();
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		QUICK_SCOPE_CYCLE_COUNTER( STAT_OceanGridMeshSceneProxy_GetDynamicMeshElements );

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

	void EnqueTestSinWaveCommand(FRHICommandListImmediate& RHICmdList, UOceanGridMeshComponent* Component) const
	{
		FTextureRenderTargetResource* TextureRenderTargetResource = Component->GetDisplacementMap()->GetRenderTargetResource();
		if (TextureRenderTargetResource == nullptr)
		{
			return;
		}

		FOceanSinWaveParameters Params;
		Params.MapWidth = TextureRenderTargetResource->GetSizeX();
		Params.MapHeight = TextureRenderTargetResource->GetSizeY();
		Params.MeshWidth = Component->GetGridWidth() * Component->GetNumColumn();
		Params.MeshHeight = Component->GetGridHeight() * Component->GetNumRow();
		Params.WaveLengthRow = Component->GetWaveLengthRow();
		Params.WaveLengthColumn = Component->GetWaveLengthColumn();
		Params.Period = Component->GetPeriod();
		Params.Amplitude = Component->GetAmplitude();
		Params.AccumulatedTime = Component->GetAccumulatedTime();

		TestSinWave(RHICmdList, Params, Component->GetDisplacementMapUAV());
	}

	void EnqueSimulateOceanCommand(FRHICommandListImmediate& RHICmdList, UOceanGridMeshComponent* Component) const
	{
		FTextureRenderTargetResource* TextureRenderTargetResource = Component->GetDisplacementMap()->GetRenderTargetResource();
		if (TextureRenderTargetResource == nullptr)
		{
			return;
		}

		FOceanSpectrumParameters Params;
		Params.DispMapDimension = TextureRenderTargetResource->GetSizeX(); // TODO:正方形前提でSizeYは見てない
		Params.PatchLength = Component->GetGridWidth() * Component->GetNumColumn(); // TODO:こちらも同様。幅しか見てない
		Params.AmplitudeScale = Component->GetAmplitudeScale();
		Params.WindDirection = Component->GetWindDirection();
		Params.WindSpeed = Component->GetWindSpeed();
		Params.WindDependency = Component->GetWindDependency();
		Params.ChoppyScale = Component->GetChoppyScale();
		Params.AccumulatedTime = Component->GetAccumulatedTime() * Component->GetTimeScale();

		SimulateOcean(
			RHICmdList,
			Params,
			H0Buffer.GetSRV(),
			Omega0Buffer.GetSRV(),
			HtBuffer.GetSRV(),
			HtBuffer.GetUAV(),
			DkxBuffer.GetSRV(),
			DkxBuffer.GetUAV(),
			DkyBuffer.GetSRV(),
			DkyBuffer.GetUAV(),
			Component->GetDisplacementMapUAV(),
			Component->GetH0DebugViewUAV(),
			Component->GetHtDebugViewUAV(),
			Component->GetDkxDebugViewUAV(),
			Component->GetDkyDebugViewUAV()
		);
	}

private:

	UMaterialInterface* Material;
	FDeformableVertexBuffers VertexBuffers;
	FDynamicMeshIndexBuffer32 IndexBuffer;
	FLocalVertexFactory VertexFactory;

	TResourceArray<FVector2D> H0Data;
	TResourceArray<float> Omega0Data;
	TResourceArray<float> HtZeroInitData;
	TResourceArray<float> DkxZeroInitData;
	TResourceArray<float> DkyZeroInitData;
	TResourceArray<float> DxyzZeroInitData;
	FResourceArrayStructuredBuffer H0Buffer;
	FResourceArrayStructuredBuffer Omega0Buffer;
	FResourceArrayStructuredBuffer HtBuffer;
	FResourceArrayStructuredBuffer DkxBuffer;
	FResourceArrayStructuredBuffer DkyBuffer;
	FResourceArrayStructuredBuffer DxyzBuffer;

	FMaterialRelevance MaterialRelevance;
};

//////////////////////////////////////////////////////////////////////////

UOceanGridMeshComponent::UOceanGridMeshComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

UOceanGridMeshComponent::~UOceanGridMeshComponent()
{
	if (_DisplacementMapUAV.IsValid())
	{
		_DisplacementMapUAV.SafeRelease();
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
}

FPrimitiveSceneProxy* UOceanGridMeshComponent::CreateSceneProxy()
{
	FPrimitiveSceneProxy* Proxy = NULL;
	if(_Vertices.Num() > 0 && _Indices.Num() > 0 && DisplacementMap != nullptr)
	{
		Proxy = new FOceanGridMeshSceneProxy(this);
	}
	return Proxy;
}

void UOceanGridMeshComponent::SetSinWaveSettings(float WaveLengthRow, float WaveLengthColumn, float Period, float Amplitude)
{
	_WaveLengthRow = WaveLengthRow;
	_WaveLengthColumn = WaveLengthColumn;
	_Period = Period;
	_Amplitude = Amplitude;

	if (_DisplacementMapUAV.IsValid())
	{
		_DisplacementMapUAV.SafeRelease();
	}

	if (DisplacementMap != nullptr)
	{
		_DisplacementMapUAV = RHICreateUnorderedAccessView(DisplacementMap->GameThread_GetRenderTargetResource()->TextureRHI);
	}
}

void UOceanGridMeshComponent::SetOceanSpectrumSettings(float TimeScale, float AmplitudeScale, FVector2D WindDirection, float WindSpeed, float WindDependency, float ChoppyScale)
{
	_TimeScale = TimeScale;
	_AmplitudeScale = AmplitudeScale;
	_WindDirection = WindDirection.GetSafeNormal(); // 正規化しておく
	_WindSpeed = WindSpeed;
	_WindDependency = WindDependency;
	_ChoppyScale = ChoppyScale;

	if (_DisplacementMapUAV.IsValid())
	{
		_DisplacementMapUAV.SafeRelease();
	}

	if (DisplacementMap != nullptr)
	{
		_DisplacementMapUAV = RHICreateUnorderedAccessView(DisplacementMap->GameThread_GetRenderTargetResource()->TextureRHI);
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
}

#define TEST_SIN_WAVE 0

void UOceanGridMeshComponent::SendRenderDynamicData_Concurrent()
{
	//SCOPE_CYCLE_COUNTER(STAT_OceanGridMeshCompUpdate);
	Super::SendRenderDynamicData_Concurrent();

	if (SceneProxy != nullptr && DisplacementMap != nullptr && _DisplacementMapUAV.IsValid())
	{
		ENQUEUE_RENDER_COMMAND(OceanDeformGridMeshCommand)(
			[this](FRHICommandListImmediate& RHICmdList)
			{
#if TEST_SIN_WAVE > 0
				((const FOceanGridMeshSceneProxy*)SceneProxy)->EnqueTestSinWaveCommand(RHICmdList, this);
#else
				((const FOceanGridMeshSceneProxy*)SceneProxy)->EnqueSimulateOceanCommand(RHICmdList, this);
#endif
			}
		);
	}
}

