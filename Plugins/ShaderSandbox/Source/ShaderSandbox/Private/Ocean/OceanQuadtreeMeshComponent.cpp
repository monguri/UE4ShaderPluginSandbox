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

namespace
{
/**
 * ����0�A�W���΍�1�̃K�E�V�A�����z�Ń����_���l�𐶐�����B
 */
float GaussianRand()
{
	float U1 = FMath::FRand();
	float U2 = FMath::FRand();

	if (U1 < SMALL_NUMBER)
	{
		U1 = SMALL_NUMBER;
	}

	// TODO:�v�Z�̗������킩���
	return FMath::Sqrt(-2.0f * FMath::Loge(U1)) * FMath::Cos(2.0f * PI * U2);
}

/**
 * Phillips Spectrum
 * K: ���K�����ꂽ�g���x�N�g��
 */
float CalculatePhillipsCoefficient(const FVector2D& K, float Gravity, const FOceanSpectrumParameters& Params)
{
	float Amplitude = Params.AmplitudeScale * 1.0e-7f; //TODO�F�����s���̒l

	// ���̕����ɂ����čő�̔g���̔g�B
	float MaxLength = Params.WindSpeed * Params.WindSpeed / Gravity;

	float KSqr = K.X * K.X + K.Y * K.Y;
	float KCos = K.X * Params.WindDirection.X + K.Y * Params.WindDirection.Y;
	float Phillips = Amplitude * FMath::Exp(-1.0f / (MaxLength * MaxLength * KSqr)) / (KSqr * KSqr * KSqr) * (KCos * KCos); // TODO:KSqr��2�悷���K^4�ɂȂ邩��\���ł́H

	// �t�����̔g�͎キ����
	if (KCos < 0)
	{
		Phillips *= Params.WindDependency;
	}

	// �ő�g�����������Ə������g�͍팸����BTessendorf�̌v�Z�ɂ͂Ȃ������v�f�BTODO:����̂��H
	float CutLength = MaxLength / 1000;
	return Phillips * FMath::Exp(-KSqr * CutLength * CutLength);
}

void InitHeightMap(const FOceanSpectrumParameters& Params, float GravityZ, TResourceArray<Complex>& OutH0, TResourceArray<float>& OutOmega0)
{
	FVector2D K;
	float GravityConstant = FMath::Abs(GravityZ);

	FMath::RandInit(FPlatformTime::Cycles());

	for (uint32 i = 0; i < Params.DispMapDimension; i++)
	{
		// K�͐��K�����ꂽ�g���x�N�g���B�͈͂�[-|DX/W, |DX/W], [-|DY/H, |DY/H]
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
	{
		TArray<FDynamicMeshVertex> Vertices;
		Vertices.Reset(Component->GetVertices().Num());
		IndexBuffer.Indices = Component->GetIndices();

		for (int32 VertIdx = 0; VertIdx < Component->GetVertices().Num(); VertIdx++)
		{
			// TODO:Tangent�͂Ƃ肠����FDynamicMeshVertex�̃f�t�H���g�l�܂����ɂ���BColor��DynamicMeshVertex�̃f�t�H���g�l���̗p���Ă���
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
		check(SizeX == SizeY); // �����`�ł���O��
		check(FMath::IsPowerOfTwo(SizeX)); // 2�̗ݏ�̃T�C�Y�ł���O��
		uint32 DispMapDimension = SizeX;
		
		// Phyllips Spectrum���g����������
		// Height map H(0)
		H0Data.Init(Complex::ZeroVector, DispMapDimension * DispMapDimension);
		// Complex::ZeroVector�Ƃ������O�������i�D�������AZero�݂����ȐV�����萔����낤�Ǝv����typedef FVector2D Complex�ł͂ł��Ȃ��̂ō��͑Ë�����

		Omega0Data.Init(0.0f, DispMapDimension * DispMapDimension);

		FOceanSpectrumParameters Params;
		Params.DispMapDimension = DispMapDimension;
		Params.PatchLength = Component->GetGridWidth() * Component->GetNumColumn(); // GetPatchLength()�͌��󖢎g�p�B��������l���ă��b�V���T�C�Y���g���̂�����������
		Params.AmplitudeScale = Component->GetAmplitudeScale();
		Params.WindDirection = Component->GetWindDirection();
		Params.WindSpeed = Component->GetWindSpeed();
		Params.WindDependency = Component->GetWindDependency();
		Params.ChoppyScale = Component->GetChoppyScale();
		Params.AccumulatedTime = Component->GetAccumulatedTime() * Component->GetTimeScale();
		Params.DxyzDebugAmplitude = Component->DxyzDebugAmplitude;

		InitHeightMap(Params, Component->GetWorld()->GetGravityZ(), H0Data, Omega0Data);

		H0Buffer.Initialize(H0Data, sizeof(Complex));
		Omega0Buffer.Initialize(Omega0Data, sizeof(float));

		HtZeroInitData.Init(Complex::ZeroVector, DispMapDimension * DispMapDimension);
		HtBuffer.Initialize(HtZeroInitData, sizeof(Complex));

		DkxZeroInitData.Init(Complex::ZeroVector, DispMapDimension * DispMapDimension);
		DkxBuffer.Initialize(DkxZeroInitData, sizeof(Complex));

		DkyZeroInitData.Init(Complex::ZeroVector, DispMapDimension * DispMapDimension);
		DkyBuffer.Initialize(DkyZeroInitData, sizeof(Complex));

		FFTWorkZeroInitData.Init(Complex::ZeroVector, DispMapDimension * DispMapDimension);
		FFTWorkBuffer.Initialize(FFTWorkZeroInitData, sizeof(Complex));

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

	void EnqueSimulateOceanCommand(FRHICommandListImmediate& RHICmdList, UOceanQuadtreeMeshComponent* Component) const
	{
		FTextureRenderTargetResource* TextureRenderTargetResource = Component->GetDisplacementMap()->GetRenderTargetResource();
		if (TextureRenderTargetResource == nullptr)
		{
			return;
		}

		FOceanSpectrumParameters Params;
		Params.DispMapDimension = TextureRenderTargetResource->GetSizeX(); // TODO:�����`�O���SizeY�͌��ĂȂ�
		Params.PatchLength = Component->GetGridWidth() * Component->GetNumColumn(); // GetPatchLength()�͌��󖢎g�p�B��������l���ă��b�V���T�C�Y���g���̂�����������
		Params.AmplitudeScale = Component->GetAmplitudeScale();
		Params.WindDirection = Component->GetWindDirection();
		Params.WindSpeed = Component->GetWindSpeed();
		Params.WindDependency = Component->GetWindDependency();
		Params.ChoppyScale = Component->GetChoppyScale();
		Params.AccumulatedTime = Component->GetAccumulatedTime() * Component->GetTimeScale();
		Params.DxyzDebugAmplitude = Component->DxyzDebugAmplitude;

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

	TResourceArray<Complex> H0Data;
	TResourceArray<float> Omega0Data;
	TResourceArray<Complex> HtZeroInitData;
	TResourceArray<Complex> DkxZeroInitData;
	TResourceArray<Complex> DkyZeroInitData;
	TResourceArray<Complex> FFTWorkZeroInitData;
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

	FMaterialRelevance MaterialRelevance;
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

	// �f�t�H���g�l�ł́AVertexBuffer��128x128�̃O���b�h�A�O���b�h�̏c����1cm�ɂ���B�`�掞�̓X�P�[�����Ďg���B
	// �����ł͐����`�̒��S�����_�ɂ��镽�s�ړ��͂��Ȃ��B���ۂɃ��b�V����`��ɓn���Ƃ��ɕ��s�ړ����s���B
	InitGridMeshSetting(NumGridDivision, NumGridDivision, GridLength, GridLength);

	_PatchLength = PatchLength;
	_TimeScale = TimeScale;
	_AmplitudeScale = AmplitudeScale;
	_WindDirection = WindDirection.GetSafeNormal(); // ���K�����Ă���
	_WindSpeed = WindSpeed;
	_WindDependency = WindDependency;
	_ChoppyScale = ChoppyScale;

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
