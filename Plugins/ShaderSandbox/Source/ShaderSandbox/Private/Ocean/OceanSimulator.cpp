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
		SHADER_PARAMETER(uint32, MapWidth)
		SHADER_PARAMETER(uint32, MapHeight)
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

class FOceanUpdateSpectrumCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FOceanUpdateSpectrumCS);
	SHADER_USE_PARAMETER_STRUCT(FOceanUpdateSpectrumCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, MapWidth)
		SHADER_PARAMETER(uint32, MapHeight)
		SHADER_PARAMETER_SRV(StructuredBuffer<FVector2D>, H0Buffer)
		SHADER_PARAMETER_SRV(StructuredBuffer<float>, OmegaBuffer)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, HtBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FOceanUpdateSpectrumCS, "/Plugin/ShaderSandbox/Private/OceanSimulation.usf", "UpdateSpectrumCS", SF_Compute);

void SimulateOcean(FRHICommandListImmediate& RHICmdList, const FOceanSpectrumParameters& Params, FRHIShaderResourceView* H0SRV, FRHIShaderResourceView* OmegaSRV, FRHIUnorderedAccessView* HtUAV, FRHIUnorderedAccessView* DisplacementMapUAV, FRHIUnorderedAccessView* H0DebugViewUAV)
{

	TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);

	FRDGBuilder GraphBuilder(RHICmdList);

	// TODO:ÉuÉçÉbÉNÇÊÇËä÷êîâª
	if (H0DebugViewUAV != nullptr)
	{
		uint32 DispatchCountX = FMath::DivideAndRoundUp((Params.DispMapDimension + 4), (uint32)8);
		uint32 DispatchCountY = FMath::DivideAndRoundUp(Params.DispMapDimension, (uint32)8);
		check(DispatchCountX <= 65535);
		check(DispatchCountY <= 65535);

		TShaderMapRef<FOceanDebugH0CS> OceanDebugH0CS(ShaderMap);

		FOceanDebugH0CS::FParameters* OceanDebugH0Params = GraphBuilder.AllocParameters<FOceanDebugH0CS::FParameters>();
		OceanDebugH0Params->MapWidth = Params.DispMapDimension + 4;
		OceanDebugH0Params->MapHeight = Params.DispMapDimension;
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
		uint32 DispatchCountX = FMath::DivideAndRoundUp(Params.DispMapDimension, (uint32)8);
		uint32 DispatchCountY = FMath::DivideAndRoundUp(Params.DispMapDimension, (uint32)8);
		check(DispatchCountX <= 65535);
		check(DispatchCountY <= 65535);

		TShaderMapRef<FOceanUpdateSpectrumCS> OceanUpdateSpectrumCS(ShaderMap);

		FOceanUpdateSpectrumCS::FParameters* UpdateSpectrumParams = GraphBuilder.AllocParameters<FOceanUpdateSpectrumCS::FParameters>();
		UpdateSpectrumParams->MapWidth = Params.DispMapDimension + 4;
		UpdateSpectrumParams->MapHeight = Params.DispMapDimension;
		UpdateSpectrumParams->H0Buffer = H0SRV;
		UpdateSpectrumParams->OmegaBuffer = OmegaSRV;
		UpdateSpectrumParams->HtBuffer = HtUAV;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("OceanUpdateSpectrumCS"),
			*OceanUpdateSpectrumCS,
			UpdateSpectrumParams,
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

