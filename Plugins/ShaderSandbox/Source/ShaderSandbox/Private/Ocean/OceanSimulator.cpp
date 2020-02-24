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
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, OutHtBuffer)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, OutDkxBuffer)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, OutDkyBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FOceanUpdateSpectrumCS, "/Plugin/ShaderSandbox/Private/OceanSimulation.usf", "UpdateSpectrumCS", SF_Compute);

void SimulateOcean(FRHICommandListImmediate& RHICmdList, const FOceanSpectrumParameters& Params, FRHIShaderResourceView* H0SRV, FRHIShaderResourceView* OmegaSRV, FRHIShaderResourceView* HtSRV, FRHIUnorderedAccessView* HtUAV, FRHIShaderResourceView* DkxSRV, FRHIUnorderedAccessView* DkxUAV, FRHIShaderResourceView* DkySRV, FRHIUnorderedAccessView* DkyUAV, FRHIUnorderedAccessView* DisplacementMapUAV, FRHIUnorderedAccessView* H0DebugViewUAV, FRHIUnorderedAccessView* HtDebugViewUAV, FRHIUnorderedAccessView* DkxDebugViewUAV, FRHIUnorderedAccessView* DkyDebugViewUAV)
{
	uint32 DispatchCountX = FMath::DivideAndRoundUp((Params.DispMapDimension), (uint32)8);
	uint32 DispatchCountY = FMath::DivideAndRoundUp(Params.DispMapDimension, (uint32)8);
	check(DispatchCountX <= 65535);
	check(DispatchCountY <= 65535);

	TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);

	FRDGBuilder GraphBuilder(RHICmdList);

	// TODO:ÉuÉçÉbÉNÇÊÇËä÷êîâª
	if (H0DebugViewUAV != nullptr)
	{
		TShaderMapRef<FOceanDebugH0CS> OceanDebugH0CS(ShaderMap);

		FOceanDebugH0CS::FParameters* OceanDebugH0Params = GraphBuilder.AllocParameters<FOceanDebugH0CS::FParameters>();
		OceanDebugH0Params->MapSize = Params.DispMapDimension;
		OceanDebugH0Params->H0Buffer = H0SRV;
		OceanDebugH0Params->H0DebugTexture = H0DebugViewUAV;

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
		UpdateSpectrumParams->H0Buffer = H0SRV;
		UpdateSpectrumParams->OmegaBuffer = OmegaSRV;
		UpdateSpectrumParams->OutHtBuffer = HtUAV;
		UpdateSpectrumParams->OutDkxBuffer = DkxUAV;
		UpdateSpectrumParams->OutDkyBuffer = DkyUAV;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("OceanUpdateSpectrumCS"),
			*OceanUpdateSpectrumCS,
			UpdateSpectrumParams,
			FIntVector(DispatchCountX, DispatchCountY, 1)
		);
	}

	if (HtDebugViewUAV != nullptr)
	{
		TShaderMapRef<FOceanDebugHtCS> OceanDebugHtCS(ShaderMap);

		FOceanDebugHtCS::FParameters* OceanDebugHtParams = GraphBuilder.AllocParameters<FOceanDebugHtCS::FParameters>();
		OceanDebugHtParams->MapSize = Params.DispMapDimension;
		OceanDebugHtParams->HtBuffer = HtSRV;
		OceanDebugHtParams->HtDebugTexture = HtDebugViewUAV;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("OceanDebugHtCS"),
			*OceanDebugHtCS,
			OceanDebugHtParams,
			FIntVector(DispatchCountX, DispatchCountY, 1)
		);
	}

	if (DkxDebugViewUAV != nullptr)
	{
		TShaderMapRef<FOceanDebugDkxCS> OceanDebugDkxCS(ShaderMap);

		FOceanDebugDkxCS::FParameters* OceanDebugDkxParams = GraphBuilder.AllocParameters<FOceanDebugDkxCS::FParameters>();
		OceanDebugDkxParams->MapSize = Params.DispMapDimension;
		OceanDebugDkxParams->DkxBuffer = DkxSRV;
		OceanDebugDkxParams->DkxDebugTexture = DkxDebugViewUAV;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("OceanDebugDkxCS"),
			*OceanDebugDkxCS,
			OceanDebugDkxParams,
			FIntVector(DispatchCountX, DispatchCountY, 1)
		);
	}

	if (DkyDebugViewUAV != nullptr)
	{
		TShaderMapRef<FOceanDebugDkyCS> OceanDebugDkyCS(ShaderMap);

		FOceanDebugDkyCS::FParameters* OceanDebugDkyParams = GraphBuilder.AllocParameters<FOceanDebugDkyCS::FParameters>();
		OceanDebugDkyParams->MapSize = Params.DispMapDimension;
		OceanDebugDkyParams->DkyBuffer = DkySRV;
		OceanDebugDkyParams->DkyDebugTexture = DkyDebugViewUAV;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("OceanDebugDkyCS"),
			*OceanDebugDkyCS,
			OceanDebugDkyParams,
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
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, DisplacementMap)
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
	OceanSinWaveParams->DisplacementMap = DisplacementMapUAV;

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("OceanSinWaveCS"),
		*OceanSinWaveCS,
		OceanSinWaveParams,
		FIntVector(DispatchCountX, DispatchCountY, 1)
	);

	GraphBuilder.Execute();
}

