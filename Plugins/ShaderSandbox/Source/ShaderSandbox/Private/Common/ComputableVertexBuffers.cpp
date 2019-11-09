#include "Common/ComputableVertexBuffers.h"
#include "LocalVertexFactory.h"
#include "DynamicMeshBuilder.h"

#include "Rendering/StaticMeshVertexBuffer.h"
#include "StaticMeshVertexData.h"
#include "GPUSkinCache.h"

FComputableMeshVertexBuffer::FComputableMeshVertexBuffer() :
	TangentsData(nullptr),
	TexcoordData(nullptr),
	TangentsDataPtr(nullptr),
	TexcoordDataPtr(nullptr),
	NumTexCoords(0),
	NumVertices(0),
	bUseFullPrecisionUVs(!GVertexElementTypeSupport.IsSupported(VET_Half2)),
	bUseHighPrecisionTangentBasis(false)
{}

FComputableMeshVertexBuffer::~FComputableMeshVertexBuffer()
{
	CleanUp();
}

/** Delete existing resources */
void FComputableMeshVertexBuffer::CleanUp()
{
	if (TangentsData)
	{
		delete TangentsData;
		TangentsData = nullptr;
	}
	if (TexcoordData)
	{
		delete TexcoordData;
		TexcoordData = nullptr;
	}
}

void FComputableMeshVertexBuffer::Init(uint32 InNumVertices, uint32 InNumTexCoords)
{
	NumTexCoords = InNumTexCoords;
	NumVertices = InNumVertices;

	// Allocate the vertex data storage type.
	AllocateData();

	// Allocate the vertex data buffer.
	TangentsData->ResizeBuffer(NumVertices);
	TangentsDataPtr = NumVertices ? TangentsData->GetDataPointer() : nullptr;
	TexcoordData->ResizeBuffer(NumVertices * GetNumTexCoords());
	TexcoordDataPtr = NumVertices ? TexcoordData->GetDataPointer() : nullptr;
}

void FComputableMeshVertexBuffer::ConvertHalfTexcoordsToFloat(const uint8* InData)
{
	check(TexcoordData);
	SetUseFullPrecisionUVs(true);

	FStaticMeshVertexDataInterface* OriginalTexcoordData = TexcoordData;

	typedef TStaticMeshVertexUVsDatum<typename TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::HighPrecision>::UVsTypeT> UVType;
	TexcoordData = new TStaticMeshVertexData<UVType>(OriginalTexcoordData->GetAllowCPUAccess());
	TexcoordData->ResizeBuffer(NumVertices * GetNumTexCoords());
	TexcoordDataPtr = TexcoordData->GetDataPointer();
	TexcoordStride = sizeof(UVType);

	FVector2D* DestTexcoordDataPtr = (FVector2D*)TexcoordDataPtr;
	FVector2DHalf* SourceTexcoordDataPtr = (FVector2DHalf*)(InData ? InData : OriginalTexcoordData->GetDataPointer());
	for (uint32 i = 0; i < NumVertices * GetNumTexCoords(); i++)
	{
		*DestTexcoordDataPtr++ = *SourceTexcoordDataPtr++;
	}

	delete OriginalTexcoordData;
	OriginalTexcoordData = nullptr;
}

FVertexBufferRHIRef FComputableMeshVertexBuffer::CreateTangentsRHIBuffer_RenderThread()
{
	if (GetNumVertices())
	{
		FResourceArrayInterface* RESTRICT ResourceArray = TangentsData ? TangentsData->GetResourceArray() : nullptr;
		const uint32 SizeInBytes = ResourceArray ? ResourceArray->GetResourceDataSize() : 0;
		FRHIResourceCreateInfo CreateInfo(ResourceArray);
		CreateInfo.bWithoutNativeResource = !TangentsData;
		return RHICreateVertexBuffer(SizeInBytes, BUF_Static | BUF_ShaderResource, CreateInfo);
	}
	return nullptr;
}

FVertexBufferRHIRef FComputableMeshVertexBuffer::CreateTexCoordRHIBuffer_RenderThread()
{
	if (GetNumTexCoords())
	{
		FResourceArrayInterface* RESTRICT ResourceArray = TexcoordData ? TexcoordData->GetResourceArray() : nullptr;
		const uint32 SizeInBytes = ResourceArray ? ResourceArray->GetResourceDataSize() : 0;
		FRHIResourceCreateInfo CreateInfo(ResourceArray);
		CreateInfo.bWithoutNativeResource = !TexcoordData;
		return RHICreateVertexBuffer(SizeInBytes, BUF_Static | BUF_ShaderResource, CreateInfo);
	}
	return nullptr;
}

void FComputableMeshVertexBuffer::InitRHI()
{
	TangentsVertexBuffer.VertexBufferRHI = CreateTangentsRHIBuffer_RenderThread();
	TexCoordVertexBuffer.VertexBufferRHI = CreateTexCoordRHIBuffer_RenderThread();
	if (TangentsVertexBuffer.VertexBufferRHI && (RHISupportsManualVertexFetch(GMaxRHIShaderPlatform) || IsGPUSkinCacheAvailable(GMaxRHIShaderPlatform)))
	{
		TangentsSRV = RHICreateShaderResourceView(
			TangentsData ? TangentsVertexBuffer.VertexBufferRHI : nullptr,
			GetUseHighPrecisionTangentBasis() ? 8 : 4,
			GetUseHighPrecisionTangentBasis() ? PF_R16G16B16A16_SNORM : PF_R8G8B8A8_SNORM);
	}
	if (TexCoordVertexBuffer.VertexBufferRHI && RHISupportsManualVertexFetch(GMaxRHIShaderPlatform))
	{
		TextureCoordinatesSRV = RHICreateShaderResourceView(
			TexcoordData ? TexCoordVertexBuffer.VertexBufferRHI : nullptr,
			GetUseFullPrecisionUVs() ? 8 : 4,
			GetUseFullPrecisionUVs() ? PF_G32R32F : PF_G16R16F);
	}
}

void FComputableMeshVertexBuffer::ReleaseRHI()
{
	TangentsSRV.SafeRelease();
	TextureCoordinatesSRV.SafeRelease();
	TangentsVertexBuffer.ReleaseRHI();
	TexCoordVertexBuffer.ReleaseRHI();
}

void FComputableMeshVertexBuffer::InitResource()
{
	FRenderResource::InitResource();
	TangentsVertexBuffer.InitResource();
	TexCoordVertexBuffer.InitResource();
}

void FComputableMeshVertexBuffer::ReleaseResource()
{
	FRenderResource::ReleaseResource();
	TangentsVertexBuffer.ReleaseResource();
	TexCoordVertexBuffer.ReleaseResource();
}

void FComputableMeshVertexBuffer::AllocateData()
{
	// Clear any old VertexData before allocating.
	CleanUp();

	uint32 VertexStride = 0;
	if (GetUseHighPrecisionTangentBasis())
	{
		typedef TStaticMeshVertexTangentDatum<typename TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::HighPrecision>::TangentTypeT> TangentType;
		TangentsStride = sizeof(TangentType);
		TangentsData = new TStaticMeshVertexData<TangentType>(true);
	}
	else
	{
		typedef TStaticMeshVertexTangentDatum<typename TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::Default>::TangentTypeT> TangentType;
		TangentsStride = sizeof(TangentType);
		TangentsData = new TStaticMeshVertexData<TangentType>(true);
	}

	if (GetUseFullPrecisionUVs())
	{
		typedef TStaticMeshVertexUVsDatum<typename TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::HighPrecision>::UVsTypeT> UVType;
		TexcoordStride = sizeof(UVType);
		TexcoordData = new TStaticMeshVertexData<UVType>(true);
	}
	else
	{
		typedef TStaticMeshVertexUVsDatum<typename TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::Default>::UVsTypeT> UVType;
		TexcoordStride = sizeof(UVType);
		TexcoordData = new TStaticMeshVertexData<UVType>(true);
	}
}

void FComputableMeshVertexBuffer::BindTangentVertexBuffer(const FVertexFactory* VertexFactory, FStaticMeshDataType& Data) const
{
	{
		Data.TangentsSRV = TangentsSRV;
	}

	{
		uint32 TangentSizeInBytes = 0;
		uint32 TangentXOffset = 0;
		uint32 TangentZOffset = 0;
		EVertexElementType TangentElemType = VET_None;

		if (GetUseHighPrecisionTangentBasis())
		{
			typedef TStaticMeshVertexTangentDatum<typename TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::HighPrecision>::TangentTypeT> TangentType;
			TangentElemType = TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::HighPrecision>::VertexElementType;
			TangentXOffset = STRUCT_OFFSET(TangentType, TangentX);
			TangentZOffset = STRUCT_OFFSET(TangentType, TangentZ);
			TangentSizeInBytes = sizeof(TangentType);

		}
		else
		{
			typedef TStaticMeshVertexTangentDatum<typename TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::Default>::TangentTypeT> TangentType;
			TangentElemType = TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::Default>::VertexElementType;
			TangentXOffset = STRUCT_OFFSET(TangentType, TangentX);
			TangentZOffset = STRUCT_OFFSET(TangentType, TangentZ);
			TangentSizeInBytes = sizeof(TangentType);
		}

		Data.TangentBasisComponents[0] = FVertexStreamComponent(
			&TangentsVertexBuffer,
			TangentXOffset,
			TangentSizeInBytes,
			TangentElemType,
			EVertexStreamUsage::ManualFetch
		);

		Data.TangentBasisComponents[1] = FVertexStreamComponent(
			&TangentsVertexBuffer,
			TangentZOffset,
			TangentSizeInBytes,
			TangentElemType,
			EVertexStreamUsage::ManualFetch
		);
	}
}

void FComputableMeshVertexBuffer::BindPackedTexCoordVertexBuffer(const FVertexFactory* VertexFactory, FStaticMeshDataType& Data) const
{
	Data.TextureCoordinates.Empty();
	Data.NumTexCoords = GetNumTexCoords();

	{
		Data.TextureCoordinatesSRV = TextureCoordinatesSRV;
	}

	{
		EVertexElementType UVDoubleWideVertexElementType = VET_None;
		EVertexElementType UVVertexElementType = VET_None;
		uint32 UVSizeInBytes = 0;
		if (GetUseFullPrecisionUVs())
		{
			UVSizeInBytes = sizeof(TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::HighPrecision>::UVsTypeT);
			UVDoubleWideVertexElementType = VET_Float4;
			UVVertexElementType = VET_Float2;
		}
		else
		{
			UVSizeInBytes = sizeof(TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::Default>::UVsTypeT);
			UVDoubleWideVertexElementType = VET_Half4;
			UVVertexElementType = VET_Half2;
		}

		uint32 UvStride = UVSizeInBytes * GetNumTexCoords();

		int32 UVIndex;
		for (UVIndex = 0; UVIndex < (int32)GetNumTexCoords() - 1; UVIndex += 2)
		{
			Data.TextureCoordinates.Add(FVertexStreamComponent(
				&TexCoordVertexBuffer,
				UVSizeInBytes * UVIndex,
				UvStride,
				UVDoubleWideVertexElementType,
				EVertexStreamUsage::ManualFetch
			));
		}

		// possible last UV channel if we have an odd number
		if (UVIndex < (int32)GetNumTexCoords())
		{
			Data.TextureCoordinates.Add(FVertexStreamComponent(
				&TexCoordVertexBuffer,
				UVSizeInBytes * UVIndex,
				UvStride,
				UVVertexElementType,
				EVertexStreamUsage::ManualFetch
			));
		}
	}
}

void FComputableMeshVertexBuffer::BindLightMapVertexBuffer(const FVertexFactory* VertexFactory, FStaticMeshDataType& Data, int LightMapCoordinateIndex) const
{
	LightMapCoordinateIndex = LightMapCoordinateIndex < (int32)GetNumTexCoords() ? LightMapCoordinateIndex : (int32)GetNumTexCoords() - 1;
	check(LightMapCoordinateIndex >= 0);

	Data.LightMapCoordinateIndex = LightMapCoordinateIndex;
	Data.NumTexCoords = GetNumTexCoords();

	{
		Data.TextureCoordinatesSRV = TextureCoordinatesSRV;
	}

	{
		EVertexElementType UVVertexElementType = VET_None;
		uint32 UVSizeInBytes = 0;

		if (GetUseFullPrecisionUVs())
		{
			UVSizeInBytes = sizeof(TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::HighPrecision>::UVsTypeT);
			UVVertexElementType = VET_Float2;
		}
		else
		{
			UVSizeInBytes = sizeof(TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::Default>::UVsTypeT);
			UVVertexElementType = VET_Half2;
		}

		uint32 UvStride = UVSizeInBytes * GetNumTexCoords();

		if (LightMapCoordinateIndex >= 0 && (uint32)LightMapCoordinateIndex < GetNumTexCoords())
		{
			Data.LightMapCoordinateComponent = FVertexStreamComponent(
				&TexCoordVertexBuffer,
				UVSizeInBytes * LightMapCoordinateIndex,
				UvStride,
				UVVertexElementType, 
				EVertexStreamUsage::ManualFetch
			);
		}
	}
}

/** The implementation of the static mesh position-only vertex data storage type. */
class FComputablePositionVertexData :
	public TStaticMeshVertexData<FComputablePositionVertex>
{
public:
	FComputablePositionVertexData(bool InNeedsCPUAccess=true )
		: TStaticMeshVertexData<FComputablePositionVertex>( InNeedsCPUAccess )
	{
	}
};

FComputablePositionVertexBuffer::FComputablePositionVertexBuffer():
	VertexData(NULL),
	Data(NULL),
	Stride(0),
	NumVertices(0)
{}

FComputablePositionVertexBuffer::~FComputablePositionVertexBuffer()
{
	CleanUp();
}

/** Delete existing resources */
void FComputablePositionVertexBuffer::CleanUp()
{
	if (VertexData)
	{
		delete VertexData;
		VertexData = NULL;
	}
}

void FComputablePositionVertexBuffer::Init(uint32 InNumVertices)
{
	NumVertices = InNumVertices;

	// Allocate the vertex data storage type.
	AllocateData();

	// Allocate the vertex data buffer.
	VertexData->ResizeBuffer(NumVertices);
	Data = NumVertices ? VertexData->GetDataPointer() : nullptr;
}

FVertexBufferRHIRef FComputablePositionVertexBuffer::CreateRHIBuffer_RenderThread()
{
	if (NumVertices)
	{
		FResourceArrayInterface* RESTRICT ResourceArray = VertexData ? VertexData->GetResourceArray() : nullptr;
		const uint32 SizeInBytes = ResourceArray ? ResourceArray->GetResourceDataSize() : 0;
		FRHIResourceCreateInfo CreateInfo(ResourceArray);
		CreateInfo.bWithoutNativeResource = !VertexData;
		return RHICreateVertexBuffer(SizeInBytes, BUF_Static | BUF_ShaderResource, CreateInfo);
	}
	return nullptr;
}

void FComputablePositionVertexBuffer::InitRHI()
{
	VertexBufferRHI = CreateRHIBuffer_RenderThread();
	// we have decide to create the SRV based on GMaxRHIShaderPlatform because this is created once and shared between feature levels for editor preview.
	// Also check to see whether cpu access has been activated on the vertex data
	if (VertexBufferRHI)
	{
		// we have decide to create the SRV based on GMaxRHIShaderPlatform because this is created once and shared between feature levels for editor preview.
		bool bSRV = RHISupportsManualVertexFetch(GMaxRHIShaderPlatform) || IsGPUSkinCacheAvailable(GMaxRHIShaderPlatform);

		// When bAllowCPUAccess is true, the meshes is likely going to be used for Niagara to spawn particles on mesh surface.
		// And it can be the case for CPU *and* GPU access: no differenciation today. That is why we create a SRV in this case.
		// This also avoid setting lots of states on all the members of all the different buffers used by meshes. Follow up: https://jira.it.epicgames.net/browse/UE-69376.
		bSRV |= VertexData->GetAllowCPUAccess();
		if(bSRV)
		{
			PositionComponentSRV = RHICreateShaderResourceView(VertexBufferRHI, 4, PF_R32_FLOAT);
		}
	}
}

void FComputablePositionVertexBuffer::ReleaseRHI()
{
	PositionComponentSRV.SafeRelease();
	FVertexBuffer::ReleaseRHI();
}

void FComputablePositionVertexBuffer::AllocateData()
{
	// Clear any old VertexData before allocating.
	CleanUp();

	VertexData = new FComputablePositionVertexData();
	// Calculate the vertex stride.
	Stride = VertexData->GetStride();
}

void FComputablePositionVertexBuffer::BindPositionVertexBuffer(const class FVertexFactory* VertexFactory, struct FStaticMeshDataType& StaticMeshData) const
{
	StaticMeshData.PositionComponent = FVertexStreamComponent(
		this,
		STRUCT_OFFSET(FComputablePositionVertex, Position),
		GetStride(),
		VET_Float3
	);
	StaticMeshData.PositionComponentSRV = PositionComponentSRV;
}

/** The implementation of the static mesh color-only vertex data storage type. */
class FComputableColorVertexData :
	public TStaticMeshVertexData<FColor>
{
public:
	FComputableColorVertexData( bool InNeedsCPUAccess=true )
		: TStaticMeshVertexData<FColor>( InNeedsCPUAccess )
	{
	}
};


FComputableColorVertexBuffer::FComputableColorVertexBuffer():
	VertexData(NULL),
	Data(NULL),
	Stride(0),
	NumVertices(0)
{
}

FComputableColorVertexBuffer::~FComputableColorVertexBuffer()
{
	CleanUp();
}

/** Delete existing resources */
void FComputableColorVertexBuffer::CleanUp()
{
	if (VertexData)
	{
		delete VertexData;
		VertexData = NULL;
	}
}

void FComputableColorVertexBuffer::Init(uint32 InNumVertices)
{
	NumVertices = InNumVertices;

	// Allocate the vertex data storage type.
	AllocateData();

	// Allocate the vertex data buffer.
	VertexData->ResizeBuffer(NumVertices);
	Data = InNumVertices ? VertexData->GetDataPointer() : nullptr;
}

FVertexBufferRHIRef FComputableColorVertexBuffer::CreateRHIBuffer_RenderThread()
{
	if (NumVertices)
	{
		FResourceArrayInterface* RESTRICT ResourceArray = VertexData ? VertexData->GetResourceArray() : nullptr;
		const uint32 SizeInBytes = ResourceArray ? ResourceArray->GetResourceDataSize() : 0;
		FRHIResourceCreateInfo CreateInfo(ResourceArray);
		CreateInfo.bWithoutNativeResource = !VertexData;
		return RHICreateVertexBuffer(SizeInBytes, BUF_Static | BUF_ShaderResource, CreateInfo);
	}
	return nullptr;
}

void FComputableColorVertexBuffer::InitRHI()
{
	VertexBufferRHI = CreateRHIBuffer_RenderThread();
	if (VertexBufferRHI)
	{
		ColorComponentsSRV = RHICreateShaderResourceView(VertexData ? VertexBufferRHI : nullptr, 4, PF_R8G8B8A8);
	}
}

void FComputableColorVertexBuffer::ReleaseRHI()
{
	ColorComponentsSRV.SafeRelease();
	FVertexBuffer::ReleaseRHI();
}

void FComputableColorVertexBuffer::AllocateData( )
{
	// Clear any old VertexData before allocating.
	CleanUp();

	VertexData = new FComputableColorVertexData();
	// Calculate the vertex stride.
	Stride = VertexData->GetStride();
}

void FComputableColorVertexBuffer::BindColorVertexBuffer(const FVertexFactory* VertexFactory, FStaticMeshDataType& StaticMeshData) const
{
	if (GetNumVertices() == 0)
	{
		BindDefaultColorVertexBuffer(VertexFactory, StaticMeshData, NullBindStride::ZeroForDefaultBufferBind);
		return;
	}

	StaticMeshData.ColorComponentsSRV = ColorComponentsSRV;
	StaticMeshData.ColorIndexMask = ~0u;

	{	
		StaticMeshData.ColorComponent = FVertexStreamComponent(
			this,
			0,	// Struct offset to color
			GetStride(),
			VET_Color,
			EVertexStreamUsage::ManualFetch
		);
	}
}

void FComputableColorVertexBuffer::BindDefaultColorVertexBuffer(const FVertexFactory* VertexFactory, FStaticMeshDataType& StaticMeshData, NullBindStride BindStride)
{
	StaticMeshData.ColorComponentsSRV = GNullColorVertexBuffer.VertexBufferSRV;
	StaticMeshData.ColorIndexMask = 0;

	{
		const bool bBindForDrawOverride = BindStride == NullBindStride::FColorSizeForComponentOverride;

		StaticMeshData.ColorComponent = FVertexStreamComponent(
			&GNullColorVertexBuffer,
			0, // Struct offset to color
			bBindForDrawOverride ? sizeof(FColor) : 0, //asserted elsewhere
			VET_Color,
			bBindForDrawOverride ? (EVertexStreamUsage::ManualFetch | EVertexStreamUsage::Overridden) : (EVertexStreamUsage::ManualFetch)
		);
	}
}

static inline void InitOrUpdateResourceMacro(FRenderResource* Resource)
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

void FComputableVertexBuffers::InitFromDynamicVertex(FLocalVertexFactory* VertexFactory, TArray<FDynamicMeshVertex>& Vertices, uint32 NumTexCoords, uint32 LightMapIndex)
{
	check(NumTexCoords < MAX_STATIC_TEXCOORDS && NumTexCoords > 0);
	check(LightMapIndex < NumTexCoords);

	if (Vertices.Num())
	{
		PositionVertexBuffer.Init(Vertices.Num());
		ComputableMeshVertexBuffer.Init(Vertices.Num(), NumTexCoords);
		ColorVertexBuffer.Init(Vertices.Num());

		for (int32 i = 0; i < Vertices.Num(); i++)
		{
			const FDynamicMeshVertex& Vertex = Vertices[i];

			PositionVertexBuffer.VertexPosition(i) = Vertex.Position;
			ComputableMeshVertexBuffer.SetVertexTangents(i, Vertex.TangentX.ToFVector(), Vertex.GetTangentY(), Vertex.TangentZ.ToFVector());
			for (uint32 j = 0; j < NumTexCoords; j++)
			{
				ComputableMeshVertexBuffer.SetVertexUV(i, j, Vertex.TextureCoordinate[j]);
			}
			ColorVertexBuffer.VertexColor(i) = Vertex.Color;
		}
	}
	else
	{
		PositionVertexBuffer.Init(1);
		ComputableMeshVertexBuffer.Init(1, 1);
		ColorVertexBuffer.Init(1);

		PositionVertexBuffer.VertexPosition(0) = FVector(0, 0, 0);
		ComputableMeshVertexBuffer.SetVertexTangents(0, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1));
		ComputableMeshVertexBuffer.SetVertexUV(0, 0, FVector2D(0, 0));
		ColorVertexBuffer.VertexColor(0) = FColor(1,1,1,1);
		NumTexCoords = 1;
		LightMapIndex = 0;
	}

	FComputableVertexBuffers* Self = this;
	ENQUEUE_RENDER_COMMAND(StaticMeshVertexBuffersLegacyInit)(
		[VertexFactory, Self, LightMapIndex](FRHICommandListImmediate& RHICmdList)
		{
			InitOrUpdateResourceMacro(&Self->PositionVertexBuffer);
			InitOrUpdateResourceMacro(&Self->ComputableMeshVertexBuffer);
			InitOrUpdateResourceMacro(&Self->ColorVertexBuffer);

			FLocalVertexFactory::FDataType Data;
			Self->PositionVertexBuffer.BindPositionVertexBuffer(VertexFactory, Data);
			Self->ComputableMeshVertexBuffer.BindTangentVertexBuffer(VertexFactory, Data);
			Self->ComputableMeshVertexBuffer.BindPackedTexCoordVertexBuffer(VertexFactory, Data);
			Self->ComputableMeshVertexBuffer.BindLightMapVertexBuffer(VertexFactory, Data, LightMapIndex);
			Self->ColorVertexBuffer.BindColorVertexBuffer(VertexFactory, Data);
			VertexFactory->SetData(Data);

			InitOrUpdateResourceMacro(VertexFactory);
		});
}

