#include "Ocean/OceanSimulator.h"
#include "GlobalShader.h"
#include "RHIResources.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"

class FOceanDebugH0CS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FOceanDebugH0CS);
	SHADER_USE_PARAMETER_STRUCT(FOceanDebugH0CS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, MapSize)
		SHADER_PARAMETER_SRV(StructuredBuffer<FVector2D>, H0Buffer)
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
		SHADER_PARAMETER_SRV(StructuredBuffer<FVector2D>, HtBuffer)
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
		SHADER_PARAMETER_SRV(StructuredBuffer<FVector2D>, DkxBuffer)
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
		SHADER_PARAMETER_SRV(StructuredBuffer<FVector2D>, DkyBuffer)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, DkyDebugTexture)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FOceanDebugDkyCS, "/Plugin/ShaderSandbox/Private/OceanSimulation.usf", "DebugDkyCS", SF_Compute);

class FOceanUpdateSpectrumCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FOceanUpdateSpectrumCS);
	SHADER_USE_PARAMETER_STRUCT(FOceanUpdateSpectrumCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, MapSize)
		SHADER_PARAMETER(float, Time)
		SHADER_PARAMETER_SRV(StructuredBuffer<FVector2D>, H0Buffer)
		SHADER_PARAMETER_SRV(StructuredBuffer<float>, OmegaBuffer)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FVector2D>, OutHtBuffer)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FVector2D>, OutDkxBuffer)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FVector2D>, OutDkyBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FOceanUpdateSpectrumCS, "/Plugin/ShaderSandbox/Private/OceanSimulation.usf", "UpdateSpectrumCS", SF_Compute);

class FOceanIFFTCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FOceanIFFTCS);
	SHADER_USE_PARAMETER_STRUCT(FOceanIFFTCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		//SHADER_PARAMETER(uint32, MapSize)
		SHADER_PARAMETER_SRV(StructuredBuffer<FVector2D>, InDkxBuffer)
		SHADER_PARAMETER_SRV(StructuredBuffer<FVector2D>, InDkyBuffer)
		SHADER_PARAMETER_SRV(StructuredBuffer<FVector2D>, InDkzBuffer)
		SHADER_PARAMETER_SRV(StructuredBuffer<FVector2D>, FFTWorkBufferSRV)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<float>, OutDxBuffer)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<float>, OutDyBuffer)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<float>, OutDzBuffer)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FVector2D>, FFTWorkBufferUAV)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FOceanIFFTCS, "/Plugin/ShaderSandbox/Private/OceanSimulation.usf", "OceanIFFT512x512CS", SF_Compute);

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
		TShaderMapRef<FOceanIFFTCS> OceanIFFTCS(ShaderMap);

		FOceanIFFTCS::FParameters* IFFTParams = GraphBuilder.AllocParameters<FOceanIFFTCS::FParameters>();
		// サイズをパラメータで与えない。IFFTはデータは512x512、RADIX8を前提としている
		IFFTParams->InDkxBuffer = Views.DkxSRV;
		IFFTParams->InDkyBuffer = Views.DkySRV;
		IFFTParams->InDkzBuffer = Views.HtSRV;
		IFFTParams->FFTWorkBufferSRV = Views.FFTWorkSRV;
		IFFTParams->OutDxBuffer = Views.DxUAV;
		IFFTParams->OutDyBuffer = Views.DyUAV;
		IFFTParams->OutDzBuffer = Views.DzUAV;
		IFFTParams->FFTWorkBufferUAV = Views.FFTWorkUAV;

		const int32 RADIX = 8;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("OceanIFFTCS"),
			*OceanIFFTCS,
			IFFTParams,
			FIntVector(RADIX, 1, 1)
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

