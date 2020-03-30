#include "Ocean/OceanSimulator.h"
#include "Containers/DynamicRHIResourceArray.h"
#include "GlobalShader.h"
#include "RHIResources.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"

namespace OceanSimulator
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

	// TODO:計算の理屈を調べる
	return FMath::Sqrt(-2.0f * FMath::Loge(U1)) * FMath::Cos(2.0f * PI * U2);
}

/**
 * Phillips Spectrum
 * K: 正規化された波数ベクトル
 */
float CalculatePhillipsCoefficient(const FVector2D& K, float Gravity, const FOceanSpectrumParameters& Params)
{
	float Amplitude = Params.AmplitudeScale * 1.0e-7f; // いい感じの見栄えにするための調整値

	// この風速において最大の波長の波。
	float MaxLength = Params.WindSpeed * Params.WindSpeed / Gravity;

	float KSqr = K.X * K.X + K.Y * K.Y;
	float KCos = K.X * Params.WindDirection.X + K.Y * Params.WindDirection.Y;
	float Phillips = Amplitude * FMath::Exp(-1.0f / (MaxLength * MaxLength * KSqr)) / (KSqr * KSqr * KSqr) * (KCos * KCos);

	// 逆方向の波は弱くする
	if (KCos < 0)
	{
		Phillips *= (1.0f - Params.WindDependency);
	}

	// 最大波長よりもずっと小さい波は削減する。とりあえずパラメータ化せずに1/1000をカットオフ値に
	float CutLength = MaxLength / 1000;
	return Phillips * FMath::Exp(-KSqr * CutLength * CutLength);
}

void CreateInitialHeightMap(const FOceanSpectrumParameters& Params, float GravityZ, TResourceArray<FComplex>& OutH0, TResourceArray<float>& OutOmega0)
{
	// CSで実装してもいいが、初期化時にしか走らない処理なのでデバッグしやすさのためにCPU実装にしておく
	FVector2D K;
	float GravityConstant = FMath::Abs(GravityZ);

	FMath::RandInit(FPlatformTime::Cycles());

	for (uint32 i = 0; i < Params.DispMapDimension; i++)
	{
		// Kは正規化された波数ベクトル
		K.Y = (-(int32)Params.DispMapDimension / 2.0f + i) * (2 * PI / Params.PatchLength);

		for (uint32 j = 0; j < Params.DispMapDimension; j++)
		{
			K.X = (-(int32)Params.DispMapDimension / 2.0f + j) * (2 * PI / Params.PatchLength);

			float PhillipsCoef = CalculatePhillipsCoefficient(K, GravityConstant, Params);
			float PhillipsSqrt = (K.X == 0 || K.Y == 0) ? 0.0f : FMath::Sqrt(PhillipsCoef);
			OutH0[i * (Params.DispMapDimension) + j].X = PhillipsSqrt * GaussianRand() * UE_HALF_SQRT_2;
			OutH0[i * (Params.DispMapDimension) + j].Y = PhillipsSqrt * GaussianRand() * UE_HALF_SQRT_2;

			// 周波数分布についてはdispersion relation、omega_0^2 = g * kを用いる
			OutOmega0[i * (Params.DispMapDimension) + j] = FMath::Sqrt(GravityConstant * K.Size());
		}
	}
}

class FOceanDebugH0CS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FOceanDebugH0CS);
	SHADER_USE_PARAMETER_STRUCT(FOceanDebugH0CS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, MapSize)
		SHADER_PARAMETER_SRV(StructuredBuffer<FComplex>, H0Buffer)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, H0DebugTexture)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FOceanDebugH0CS, "/Plugin/ShaderSandbox/Private/OceanSimulation.usf", "DebugH0CS", SF_Compute);

class FOceanDebugHtCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FOceanDebugHtCS);
	SHADER_USE_PARAMETER_STRUCT(FOceanDebugHtCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, MapSize)
		SHADER_PARAMETER_SRV(StructuredBuffer<FComplex>, HtBuffer)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, HtDebugTexture)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FOceanDebugHtCS, "/Plugin/ShaderSandbox/Private/OceanSimulation.usf", "DebugHtCS", SF_Compute);

class FOceanDebugDkxCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FOceanDebugDkxCS);
	SHADER_USE_PARAMETER_STRUCT(FOceanDebugDkxCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, MapSize)
		SHADER_PARAMETER_SRV(StructuredBuffer<FComplex>, DkxBuffer)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, DkxDebugTexture)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FOceanDebugDkxCS, "/Plugin/ShaderSandbox/Private/OceanSimulation.usf", "DebugDkxCS", SF_Compute);

class FOceanDebugDkyCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FOceanDebugDkyCS);
	SHADER_USE_PARAMETER_STRUCT(FOceanDebugDkyCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, MapSize)
		SHADER_PARAMETER_SRV(StructuredBuffer<FComplex>, DkyBuffer)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, DkyDebugTexture)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FOceanDebugDkyCS, "/Plugin/ShaderSandbox/Private/OceanSimulation.usf", "DebugDkyCS", SF_Compute);

class FOceanDebugDxyzCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FOceanDebugDxyzCS);
	SHADER_USE_PARAMETER_STRUCT(FOceanDebugDxyzCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, MapSize)
		SHADER_PARAMETER(float, DxyzDebugAmplitude)
		SHADER_PARAMETER_SRV(Texture2D<float4>, InDisplacementMap)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, DxyzDebugTexture)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FOceanDebugDxyzCS, "/Plugin/ShaderSandbox/Private/OceanSimulation.usf", "DebugDxyzCS", SF_Compute);

class FOceanUpdateSpectrumCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FOceanUpdateSpectrumCS);
	SHADER_USE_PARAMETER_STRUCT(FOceanUpdateSpectrumCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, MapSize)
		SHADER_PARAMETER(float, Time)
		SHADER_PARAMETER_SRV(StructuredBuffer<FComplex>, H0Buffer)
		SHADER_PARAMETER_SRV(StructuredBuffer<float>, OmegaBuffer)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FComplex>, OutHtBuffer)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FComplex>, OutDkxBuffer)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FComplex>, OutDkyBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FOceanUpdateSpectrumCS, "/Plugin/ShaderSandbox/Private/OceanSimulation.usf", "UpdateSpectrumCS", SF_Compute);

class FOceanHorizontalIFFTCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FOceanHorizontalIFFTCS);
	SHADER_USE_PARAMETER_STRUCT(FOceanHorizontalIFFTCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(StructuredBuffer<FComplex>, InDkBuffer)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FComplex>, FFTWorkBufferUAV)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FOceanHorizontalIFFTCS, "/Plugin/ShaderSandbox/Private/OceanSimulation.usf", "HorizontalIFFT512x512CS", SF_Compute);

class FOceanDkxVerticalIFFTCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FOceanDkxVerticalIFFTCS);
	SHADER_USE_PARAMETER_STRUCT(FOceanDkxVerticalIFFTCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_UAV(RWStructuredBuffer<float>, OutDxBuffer)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FComplex>, FFTWorkBufferUAV)
		SHADER_PARAMETER(float, ChoppyScale)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FOceanDkxVerticalIFFTCS, "/Plugin/ShaderSandbox/Private/OceanSimulation.usf", "DkxVerticalIFFT512x512CS", SF_Compute);

class FOceanDkyVerticalIFFTCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FOceanDkyVerticalIFFTCS);
	SHADER_USE_PARAMETER_STRUCT(FOceanDkyVerticalIFFTCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_UAV(RWStructuredBuffer<float>, OutDyBuffer)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FComplex>, FFTWorkBufferUAV)
		SHADER_PARAMETER(float, ChoppyScale)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FOceanDkyVerticalIFFTCS, "/Plugin/ShaderSandbox/Private/OceanSimulation.usf", "DkyVerticalIFFT512x512CS", SF_Compute);

class FOceanDkzVerticalIFFTCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FOceanDkzVerticalIFFTCS);
	SHADER_USE_PARAMETER_STRUCT(FOceanDkzVerticalIFFTCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_UAV(RWStructuredBuffer<float>, OutDzBuffer)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FComplex>, FFTWorkBufferUAV)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FOceanDkzVerticalIFFTCS, "/Plugin/ShaderSandbox/Private/OceanSimulation.usf", "DkzVerticalIFFT512x512CS", SF_Compute);

class FOceanUpdateDisplacementMapCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FOceanUpdateDisplacementMapCS);
	SHADER_USE_PARAMETER_STRUCT(FOceanUpdateDisplacementMapCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, MapSize)
		SHADER_PARAMETER_SRV(StructuredBuffer<float>, InDxBuffer)
		SHADER_PARAMETER_SRV(StructuredBuffer<float>, InDyBuffer)
		SHADER_PARAMETER_SRV(StructuredBuffer<float>, InDzBuffer)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, OutDisplacementMap) // TODO:なぜ<float4>という書き方で大丈夫？
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FOceanUpdateDisplacementMapCS, "/Plugin/ShaderSandbox/Private/OceanSimulation.usf", "UpdateDisplacementMapCS", SF_Compute);

class FOceanGenerateGradientFoldingMapCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FOceanGenerateGradientFoldingMapCS);
	SHADER_USE_PARAMETER_STRUCT(FOceanGenerateGradientFoldingMapCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, MapSize)
		SHADER_PARAMETER(float, PatchLength)
		SHADER_PARAMETER_SRV(Texture2D<float4>, InDisplacementMap)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, OutGradientFoldingMap)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FOceanGenerateGradientFoldingMapCS, "/Plugin/ShaderSandbox/Private/OceanSimulation.usf", "GenerateGradientFoldingMapCS", SF_Compute);

void SimulateOcean(FRHICommandListImmediate& RHICmdList, const FOceanSpectrumParameters& Params, const FOceanBufferViews& Views)
{
	uint32 DispatchCountX = FMath::DivideAndRoundUp((Params.DispMapDimension), (uint32)8);
	uint32 DispatchCountY = FMath::DivideAndRoundUp(Params.DispMapDimension, (uint32)8);
	check(DispatchCountX <= 65535);
	check(DispatchCountY <= 65535);

	TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);

	FRDGBuilder GraphBuilder(RHICmdList);

	// TODO:ブロックより関数化
	if (Views.H0DebugViewUAV != nullptr)
	{
		TShaderMapRef<FOceanDebugH0CS> OceanDebugH0CS(ShaderMap);

		FOceanDebugH0CS::FParameters* OceanDebugH0Params = GraphBuilder.AllocParameters<FOceanDebugH0CS::FParameters>();
		OceanDebugH0Params->MapSize = Params.DispMapDimension;
		OceanDebugH0Params->H0Buffer = Views.H0SRV;
		OceanDebugH0Params->H0DebugTexture = Views.H0DebugViewUAV;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("OceanDebugH0CS"),
			*OceanDebugH0CS,
			OceanDebugH0Params,
			FIntVector(DispatchCountX, DispatchCountY, 1)
		);
	}

	{
		TShaderMapRef<FOceanUpdateSpectrumCS> OceanUpdateSpectrumCS(ShaderMap);

		FOceanUpdateSpectrumCS::FParameters* UpdateSpectrumParams = GraphBuilder.AllocParameters<FOceanUpdateSpectrumCS::FParameters>();
		UpdateSpectrumParams->MapSize = Params.DispMapDimension;
		UpdateSpectrumParams->Time = Params.AccumulatedTime;
		UpdateSpectrumParams->H0Buffer = Views.H0SRV;
		UpdateSpectrumParams->OmegaBuffer = Views.OmegaSRV;
		UpdateSpectrumParams->OutHtBuffer = Views.HtUAV;
		UpdateSpectrumParams->OutDkxBuffer = Views.DkxUAV;
		UpdateSpectrumParams->OutDkyBuffer = Views.DkyUAV;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("OceanUpdateSpectrumCS"),
			*OceanUpdateSpectrumCS,
			UpdateSpectrumParams,
			FIntVector(DispatchCountX, DispatchCountY, 1)
		);
	}

	if (Views.HtDebugViewUAV != nullptr)
	{
		TShaderMapRef<FOceanDebugHtCS> OceanDebugHtCS(ShaderMap);

		FOceanDebugHtCS::FParameters* OceanDebugHtParams = GraphBuilder.AllocParameters<FOceanDebugHtCS::FParameters>();
		OceanDebugHtParams->MapSize = Params.DispMapDimension;
		OceanDebugHtParams->HtBuffer = Views.HtSRV;
		OceanDebugHtParams->HtDebugTexture = Views.HtDebugViewUAV;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("OceanDebugHtCS"),
			*OceanDebugHtCS,
			OceanDebugHtParams,
			FIntVector(DispatchCountX, DispatchCountY, 1)
		);
	}

	if (Views.DkxDebugViewUAV != nullptr)
	{
		TShaderMapRef<FOceanDebugDkxCS> OceanDebugDkxCS(ShaderMap);

		FOceanDebugDkxCS::FParameters* OceanDebugDkxParams = GraphBuilder.AllocParameters<FOceanDebugDkxCS::FParameters>();
		OceanDebugDkxParams->MapSize = Params.DispMapDimension;
		OceanDebugDkxParams->DkxBuffer = Views.DkxSRV;
		OceanDebugDkxParams->DkxDebugTexture = Views.DkxDebugViewUAV;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("OceanDebugDkxCS"),
			*OceanDebugDkxCS,
			OceanDebugDkxParams,
			FIntVector(DispatchCountX, DispatchCountY, 1)
		);
	}

	if (Views.DkyDebugViewUAV != nullptr)
	{
		TShaderMapRef<FOceanDebugDkyCS> OceanDebugDkyCS(ShaderMap);

		FOceanDebugDkyCS::FParameters* OceanDebugDkyParams = GraphBuilder.AllocParameters<FOceanDebugDkyCS::FParameters>();
		OceanDebugDkyParams->MapSize = Params.DispMapDimension;
		OceanDebugDkyParams->DkyBuffer = Views.DkySRV;
		OceanDebugDkyParams->DkyDebugTexture = Views.DkyDebugViewUAV;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("OceanDebugDkyCS"),
			*OceanDebugDkyCS,
			OceanDebugDkyParams,
			FIntVector(DispatchCountX, DispatchCountY, 1)
		);
	}

	{
		TShaderMapRef<FOceanHorizontalIFFTCS> OceanHorizIFFTCS(ShaderMap);

		FOceanHorizontalIFFTCS::FParameters* HorizIFFTParams = GraphBuilder.AllocParameters<FOceanHorizontalIFFTCS::FParameters>();
		HorizIFFTParams->InDkBuffer = Views.DkxSRV;
		HorizIFFTParams->FFTWorkBufferUAV = Views.FFTWorkUAV;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("OceanDkxHorizontalIFFTCS"),
			*OceanHorizIFFTCS,
			HorizIFFTParams,
			FIntVector(Params.DispMapDimension, 1, 1)
		);
	}

	{
		TShaderMapRef<FOceanDkxVerticalIFFTCS> OceanVertIFFTCS(ShaderMap);

		FOceanDkxVerticalIFFTCS::FParameters* VertIFFTParams = GraphBuilder.AllocParameters<FOceanDkxVerticalIFFTCS::FParameters>();
		VertIFFTParams->OutDxBuffer = Views.DxUAV;
		VertIFFTParams->FFTWorkBufferUAV = Views.FFTWorkUAV;
		VertIFFTParams->ChoppyScale = Params.ChoppyScale;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("OceanDkxVerticalIFFTCS"),
			*OceanVertIFFTCS,
			VertIFFTParams,
			FIntVector(Params.DispMapDimension, 1, 1)
		);
	}

	{
		TShaderMapRef<FOceanHorizontalIFFTCS> OceanHorizIFFTCS(ShaderMap);

		FOceanHorizontalIFFTCS::FParameters* HorizIFFTParams = GraphBuilder.AllocParameters<FOceanHorizontalIFFTCS::FParameters>();
		HorizIFFTParams->InDkBuffer = Views.DkySRV;
		HorizIFFTParams->FFTWorkBufferUAV = Views.FFTWorkUAV;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("OceanDkyHorizontalIFFTCS"),
			*OceanHorizIFFTCS,
			HorizIFFTParams,
			FIntVector(Params.DispMapDimension, 1, 1)
		);
	}

	{
		TShaderMapRef<FOceanDkyVerticalIFFTCS> OceanVertIFFTCS(ShaderMap);

		FOceanDkyVerticalIFFTCS::FParameters* VertIFFTParams = GraphBuilder.AllocParameters<FOceanDkyVerticalIFFTCS::FParameters>();
		VertIFFTParams->OutDyBuffer = Views.DyUAV;
		VertIFFTParams->FFTWorkBufferUAV = Views.FFTWorkUAV;
		VertIFFTParams->ChoppyScale = Params.ChoppyScale;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("OceanDkyVerticalIFFTCS"),
			*OceanVertIFFTCS,
			VertIFFTParams,
			FIntVector(Params.DispMapDimension, 1, 1)
		);
	}

	{
		TShaderMapRef<FOceanHorizontalIFFTCS> OceanHorizIFFTCS(ShaderMap);

		FOceanHorizontalIFFTCS::FParameters* HorizIFFTParams = GraphBuilder.AllocParameters<FOceanHorizontalIFFTCS::FParameters>();
		HorizIFFTParams->InDkBuffer = Views.HtSRV;
		HorizIFFTParams->FFTWorkBufferUAV = Views.FFTWorkUAV;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("OceanDkzHorizontalIFFTCS"),
			*OceanHorizIFFTCS,
			HorizIFFTParams,
			FIntVector(Params.DispMapDimension, 1, 1)
		);
	}

	{
		TShaderMapRef<FOceanDkzVerticalIFFTCS> OceanVertIFFTCS(ShaderMap);

		FOceanDkzVerticalIFFTCS::FParameters* VertIFFTParams = GraphBuilder.AllocParameters<FOceanDkzVerticalIFFTCS::FParameters>();
		VertIFFTParams->OutDzBuffer = Views.DzUAV;
		VertIFFTParams->FFTWorkBufferUAV = Views.FFTWorkUAV;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("OceanDkzVerticalIFFTCS"),
			*OceanVertIFFTCS,
			VertIFFTParams,
			FIntVector(Params.DispMapDimension, 1, 1)
		);
	}

	{
		TShaderMapRef<FOceanUpdateDisplacementMapCS> OceanUpdateDisplacementMapCS(ShaderMap);

		FOceanUpdateDisplacementMapCS::FParameters* UpdateDisplacementMapParams = GraphBuilder.AllocParameters<FOceanUpdateDisplacementMapCS::FParameters>();
		UpdateDisplacementMapParams->MapSize = Params.DispMapDimension;
		UpdateDisplacementMapParams->InDxBuffer = Views.DxSRV;
		UpdateDisplacementMapParams->InDyBuffer = Views.DySRV;
		UpdateDisplacementMapParams->InDzBuffer = Views.DzSRV;
		UpdateDisplacementMapParams->OutDisplacementMap = Views.DisplacementMapUAV;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("OceanUpdateDisplacementMapCS"),
			*OceanUpdateDisplacementMapCS,
			UpdateDisplacementMapParams,
			FIntVector(DispatchCountX, DispatchCountY, 1)
		);
	}

	if (Views.DxyzDebugViewUAV != nullptr)
	{
		TShaderMapRef<FOceanDebugDxyzCS> OceanDebugHtCS(ShaderMap);

		FOceanDebugDxyzCS::FParameters* OceanDebugDxyzParams = GraphBuilder.AllocParameters<FOceanDebugDxyzCS::FParameters>();
		OceanDebugDxyzParams->MapSize = Params.DispMapDimension;
		OceanDebugDxyzParams->DxyzDebugAmplitude = Params.DxyzDebugAmplitude;
		OceanDebugDxyzParams->InDisplacementMap = Views.DisplacementMapSRV;
		OceanDebugDxyzParams->DxyzDebugTexture = Views.DxyzDebugViewUAV;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("OceanDebugDxyzCS"),
			*OceanDebugHtCS,
			OceanDebugDxyzParams,
			FIntVector(DispatchCountX, DispatchCountY, 1)
		);
	}

	{
		TShaderMapRef<FOceanGenerateGradientFoldingMapCS> OceanGenerateGradientFoldingMapCS(ShaderMap);

		FOceanGenerateGradientFoldingMapCS::FParameters* GenerateGradientFoldingMapParams = GraphBuilder.AllocParameters<FOceanGenerateGradientFoldingMapCS::FParameters>();
		GenerateGradientFoldingMapParams->MapSize = Params.DispMapDimension;
		GenerateGradientFoldingMapParams->PatchLength = Params.PatchLength;
		GenerateGradientFoldingMapParams->InDisplacementMap = Views.DisplacementMapSRV;
		GenerateGradientFoldingMapParams->OutGradientFoldingMap = Views.GradientFoldingMapUAV;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("OceanGenerateGradientFoldingMapCS"),
			*OceanGenerateGradientFoldingMapCS,
			GenerateGradientFoldingMapParams,
			FIntVector(DispatchCountX, DispatchCountY, 1)
		);
	}

	GraphBuilder.Execute();
}

class FOceanSinWaveCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FOceanSinWaveCS);
	SHADER_USE_PARAMETER_STRUCT(FOceanSinWaveCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, MapWidth)
		SHADER_PARAMETER(uint32, MapHeight)
		SHADER_PARAMETER(float, MeshWidth)
		SHADER_PARAMETER(float, MeshHeight)
		SHADER_PARAMETER(float, WaveLengthRow)
		SHADER_PARAMETER(float, WaveLengthColumn)
		SHADER_PARAMETER(float, Period)
		SHADER_PARAMETER(float, Amplitude)
		SHADER_PARAMETER(float, Time)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, OutDisplacementMap)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FOceanSinWaveCS, "/Plugin/ShaderSandbox/Private/OceanSimulation.usf", "SinWaveDisplacementMap", SF_Compute);

void TestSinWave(FRHICommandListImmediate& RHICmdList, const FOceanSinWaveParameters& Params, FUnorderedAccessViewRHIRef DisplacementMapUAV)
{
	FRDGBuilder GraphBuilder(RHICmdList);

	TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);

	uint32 DispatchCountX = FMath::DivideAndRoundUp(Params.MapWidth, (uint32)8);
	uint32 DispatchCountY = FMath::DivideAndRoundUp(Params.MapHeight, (uint32)8);
	check(DispatchCountX <= 65535);
	check(DispatchCountY <= 65535);

	TShaderMapRef<FOceanSinWaveCS> OceanSinWaveCS(ShaderMap);

	FOceanSinWaveCS::FParameters* OceanSinWaveParams = GraphBuilder.AllocParameters<FOceanSinWaveCS::FParameters>();
	OceanSinWaveParams->MapWidth = Params.MapWidth;
	OceanSinWaveParams->MapHeight = Params.MapHeight;
	OceanSinWaveParams->MeshWidth = Params.MeshWidth;
	OceanSinWaveParams->MeshHeight = Params.MeshHeight;
	OceanSinWaveParams->WaveLengthColumn = Params.WaveLengthColumn;
	OceanSinWaveParams->WaveLengthRow = Params.WaveLengthRow;
	OceanSinWaveParams->WaveLengthColumn = Params.WaveLengthColumn;
	OceanSinWaveParams->Period = Params.Period;
	OceanSinWaveParams->Amplitude = Params.Amplitude;
	OceanSinWaveParams->Time = Params.AccumulatedTime;
	OceanSinWaveParams->OutDisplacementMap = DisplacementMapUAV;

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("OceanSinWaveCS"),
		*OceanSinWaveCS,
		OceanSinWaveParams,
		FIntVector(DispatchCountX, DispatchCountY, 1)
	);

	GraphBuilder.Execute();
}
} // namespace OceanSimulator

