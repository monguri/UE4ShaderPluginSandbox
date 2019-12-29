#include "Common/DeformableVertexBuffers.h"
#include "LocalVertexFactory.h"
#include "DynamicMeshBuilder.h"
#include "StaticMeshVertexData.h"

FDeformableMeshVertexBuffer::FDeformableMeshVertexBuffer() :
	TangentsData(nullptr),
	TexcoordData(nullptr),
	TangentsDataPtr(nullptr),
	TexcoordDataPtr(nullptr),
	NumTexCoords(0),
	NumVertices(0),
	bUseFullPrecisionUVs(!GVertexElementTypeSupport.IsSupported(VET_Half2))
{}

FDeformableMeshVertexBuffer::~FDeformableMeshVertexBuffer()
{
	CleanUp();
}

/** Delete existing resources */
void FDeformableMeshVertexBuffer::CleanUp()
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

void FDeformableMeshVertexBuffer::Init(uint32 InNumVertices, uint32 InNumTexCoords)
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

void FDeformableMeshVertexBuffer::ConvertHalfTexcoordsToFloat(const uint8* InData)
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

FVertexBufferRHIRef FDeformableMeshVertexBuffer::CreateTangentsRHIBuffer_RenderThread()
{
	if (GetNumVertices())
	{
		FResourceArrayInterface* RESTRICT ResourceArray = TangentsData ? TangentsData->GetResourceArray() : nullptr;
		const uint32 SizeInBytes = ResourceArray ? ResourceArray->GetResourceDataSize() : 0;
		FRHIResourceCreateInfo CreateInfo(ResourceArray);
		CreateInfo.bWithoutNativeResource = !TangentsData;
		return RHICreateVertexBuffer(SizeInBytes, BUF_Static | BUF_ShaderResource | BUF_UnorderedAccess, CreateInfo);
	}
	return nullptr;
}

FVertexBufferRHIRef FDeformableMeshVertexBuffer::CreateTexCoordRHIBuffer_RenderThread()
{
	if (GetNumTexCoords())
	{
		FResourceArrayInterface* RESTRICT ResourceArray = TexcoordData ? TexcoordData->GetResourceArray() : nullptr;
		const uint32 SizeInBytes = ResourceArray ? ResourceArray->GetResourceDataSize() : 0;
		FRHIResourceCreateInfo CreateInfo(ResourceArray);
		CreateInfo.bWithoutNativeResource = !TexcoordData;
		return RHICreateVertexBuffer(SizeInBytes, BUF_Static | BUF_ShaderResource | BUF_UnorderedAccess, CreateInfo);
	}
	return nullptr;
}

void FDeformableMeshVertexBuffer::InitRHI()
{
	TangentsVertexBuffer.VertexBufferRHI = CreateTangentsRHIBuffer_RenderThread();
	TexCoordVertexBuffer.VertexBufferRHI = CreateTexCoordRHIBuffer_RenderThread();
	if (TangentsVertexBuffer.VertexBufferRHI)
	{
		TangentsSRV = RHICreateShaderResourceView(
			TangentsData ? TangentsVertexBuffer.VertexBufferRHI : nullptr,
			4,
			PF_R8G8B8A8_SNORM);
		TangentsUAV = RHICreateUnorderedAccessView(
			TangentsData ? TangentsVertexBuffer.VertexBufferRHI : nullptr,
			PF_R8G8B8A8_SNORM);
	}
	if (TexCoordVertexBuffer.VertexBufferRHI)
	{
		TextureCoordinatesSRV = RHICreateShaderResourceView(
			TexcoordData ? TexCoordVertexBuffer.VertexBufferRHI : nullptr,
			GetUseFullPrecisionUVs() ? 8 : 4,
			GetUseFullPrecisionUVs() ? PF_G32R32F : PF_G16R16F);
		TextureCoordinatesUAV = RHICreateUnorderedAccessView(
			TexcoordData ? TexCoordVertexBuffer.VertexBufferRHI : nullptr,
			GetUseFullPrecisionUVs() ? PF_G32R32F : PF_G16R16F);
	}
}

void FDeformableMeshVertexBuffer::ReleaseRHI()
{
	TangentsSRV.SafeRelease();
	TangentsUAV.SafeRelease();
	TextureCoordinatesSRV.SafeRelease();
	TextureCoordinatesUAV.SafeRelease();
	TangentsVertexBuffer.ReleaseRHI();
	TexCoordVertexBuffer.ReleaseRHI();
}

void FDeformableMeshVertexBuffer::InitResource()
{
	FRenderResource::InitResource();
	TangentsVertexBuffer.InitResource();
	TexCoordVertexBuffer.InitResource();
}

void FDeformableMeshVertexBuffer::ReleaseResource()
{
	FRenderResource::ReleaseResource();
	TangentsVertexBuffer.ReleaseResource();
	TexCoordVertexBuffer.ReleaseResource();
}

void FDeformableMeshVertexBuffer::AllocateData()
{
	// Clear any old VertexData before allocating.
	CleanUp();

	uint32 VertexStride = 0;
	typedef TStaticMeshVertexTangentDatum<typename TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::Default>::TangentTypeT> TangentType;
	TangentsStride = sizeof(TangentType);
	TangentsData = new TStaticMeshVertexData<TangentType>(true);

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

void FDeformableMeshVertexBuffer::BindTangentVertexBuffer(const FVertexFactory* VertexFactory, FStaticMeshDataType& Data) const
{
	{
		Data.TangentsSRV = TangentsSRV;
		// TODO:ここに相当するUAVの処理は必要ない？
	}

	{
		uint32 TangentSizeInBytes = 0;
		uint32 TangentXOffset = 0;
		uint32 TangentZOffset = 0;
		EVertexElementType TangentElemType = VET_None;

		typedef TStaticMeshVertexTangentDatum<typename TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::Default>::TangentTypeT> TangentType;
		TangentElemType = TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::Default>::VertexElementType;
		TangentXOffset = STRUCT_OFFSET(TangentType, TangentX);
		TangentZOffset = STRUCT_OFFSET(TangentType, TangentZ);
		TangentSizeInBytes = sizeof(TangentType);

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

void FDeformableMeshVertexBuffer::BindPackedTexCoordVertexBuffer(const FVertexFactory* VertexFactory, FStaticMeshDataType& Data) const
{
	Data.TextureCoordinates.Empty();
	Data.NumTexCoords = GetNumTexCoords();

	{
		Data.TextureCoordinatesSRV = TextureCoordinatesSRV;
		// TODO:ここに相当するUAVの処理は必要ない？
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

void FDeformableMeshVertexBuffer::BindLightMapVertexBuffer(const FVertexFactory* VertexFactory, FStaticMeshDataType& Data, int LightMapCoordinateIndex) const
{
	LightMapCoordinateIndex = LightMapCoordinateIndex < (int32)GetNumTexCoords() ? LightMapCoordinateIndex : (int32)GetNumTexCoords() - 1;
	check(LightMapCoordinateIndex >= 0);

	Data.LightMapCoordinateIndex = LightMapCoordinateIndex;
	Data.NumTexCoords = GetNumTexCoords();

	{
		Data.TextureCoordinatesSRV = TextureCoordinatesSRV;
		// TODO:ここに相当するUAVの処理は必要ない？
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
class FDeformablePositionVertexData :
	public TStaticMeshVertexData<FDeformablePositionVertex>
{
public:
	FDeformablePositionVertexData(bool InNeedsCPUAccess=true )
		: TStaticMeshVertexData<FDeformablePositionVertex>( InNeedsCPUAccess )
	{
	}
};

FDeformablePositionVertexBuffer::FDeformablePositionVertexBuffer():
	VertexData(NULL),
	Data(NULL),
	Stride(0),
	NumVertices(0)
{}

FDeformablePositionVertexBuffer::~FDeformablePositionVertexBuffer()
{
	CleanUp();
}

/** Delete existing resources */
void FDeformablePositionVertexBuffer::CleanUp()
{
	if (VertexData)
	{
		delete VertexData;
		VertexData = NULL;
	}
}

void FDeformablePositionVertexBuffer::Init(uint32 InNumVertices)
{
	NumVertices = InNumVertices;

	// Allocate the vertex data storage type.
	AllocateData();

	// Allocate the vertex data buffer.
	VertexData->ResizeBuffer(NumVertices);
	Data = NumVertices ? VertexData->GetDataPointer() : nullptr;
}

FVertexBufferRHIRef FDeformablePositionVertexBuffer::CreateRHIBuffer_RenderThread()
{
	if (NumVertices)
	{
		FResourceArrayInterface* RESTRICT ResourceArray = VertexData ? VertexData->GetResourceArray() : nullptr;
		const uint32 SizeInBytes = ResourceArray ? ResourceArray->GetResourceDataSize() : 0;
		FRHIResourceCreateInfo CreateInfo(ResourceArray);
		CreateInfo.bWithoutNativeResource = !VertexData;
		return RHICreateVertexBuffer(SizeInBytes, BUF_Static | BUF_ShaderResource | BUF_UnorderedAccess, CreateInfo);
	}
	return nullptr;
}

void FDeformablePositionVertexBuffer::InitRHI()
{
	VertexBufferRHI = CreateRHIBuffer_RenderThread();
	// we have decide to create the SRV based on GMaxRHIShaderPlatform because this is created once and shared between feature levels for editor preview.
	// Also check to see whether cpu access has been activated on the vertex data
	if (VertexBufferRHI)
	{
		PositionComponentSRV = RHICreateShaderResourceView(VertexBufferRHI, 4, PF_R32_FLOAT);
		PositionComponentUAV = RHICreateUnorderedAccessView(VertexBufferRHI, PF_R32_FLOAT);
	}
}

void FDeformablePositionVertexBuffer::ReleaseRHI()
{
	PositionComponentSRV.SafeRelease();
	PositionComponentUAV.SafeRelease();
	FVertexBuffer::ReleaseRHI();
}

void FDeformablePositionVertexBuffer::AllocateData()
{
	// Clear any old VertexData before allocating.
	CleanUp();

	VertexData = new FDeformablePositionVertexData();
	// Calculate the vertex stride.
	Stride = VertexData->GetStride();
}

void FDeformablePositionVertexBuffer::BindPositionVertexBuffer(const class FVertexFactory* VertexFactory, struct FStaticMeshDataType& StaticMeshData) const
{
	StaticMeshData.PositionComponent = FVertexStreamComponent(
		this,
		STRUCT_OFFSET(FDeformablePositionVertex, Position),
		GetStride(),
		VET_Float4
	);
	StaticMeshData.PositionComponentSRV = PositionComponentSRV;
	// TODO:ここに相当するUAVの処理は必要ない？
}

/** The implementation of the static mesh color-only vertex data storage type. */
class FDeformableColorVertexData :
	public TStaticMeshVertexData<FColor>
{
public:
	FDeformableColorVertexData( bool InNeedsCPUAccess=true )
		: TStaticMeshVertexData<FColor>( InNeedsCPUAccess )
	{
	}
};


FDeformableColorVertexBuffer::FDeformableColorVertexBuffer():
	VertexData(NULL),
	Data(NULL),
	Stride(0),
	NumVertices(0)
{
}

FDeformableColorVertexBuffer::~FDeformableColorVertexBuffer()
{
	CleanUp();
}

/** Delete existing resources */
void FDeformableColorVertexBuffer::CleanUp()
{
	if (VertexData)
	{
		delete VertexData;
		VertexData = NULL;
	}
}

void FDeformableColorVertexBuffer::Init(uint32 InNumVertices)
{
	NumVertices = InNumVertices;

	// Allocate the vertex data storage type.
	AllocateData();

	// Allocate the vertex data buffer.
	VertexData->ResizeBuffer(NumVertices);
	Data = InNumVertices ? VertexData->GetDataPointer() : nullptr;
}

FVertexBufferRHIRef FDeformableColorVertexBuffer::CreateRHIBuffer_RenderThread()
{
	if (NumVertices)
	{
		FResourceArrayInterface* RESTRICT ResourceArray = VertexData ? VertexData->GetResourceArray() : nullptr;
		const uint32 SizeInBytes = ResourceArray ? ResourceArray->GetResourceDataSize() : 0;
		FRHIResourceCreateInfo CreateInfo(ResourceArray);
		CreateInfo.bWithoutNativeResource = !VertexData;
		return RHICreateVertexBuffer(SizeInBytes, BUF_Static | BUF_ShaderResource | BUF_UnorderedAccess, CreateInfo);
	}
	return nullptr;
}

void FDeformableColorVertexBuffer::InitRHI()
{
	VertexBufferRHI = CreateRHIBuffer_RenderThread();
	if (VertexBufferRHI)
	{
		ColorComponentsSRV = RHICreateShaderResourceView(VertexData ? VertexBufferRHI : nullptr, 4, PF_R8G8B8A8);
		ColorComponentsUAV = RHICreateUnorderedAccessView(VertexData ? VertexBufferRHI : nullptr, PF_R8G8B8A8);
	}
}

void FDeformableColorVertexBuffer::ReleaseRHI()
{
	ColorComponentsSRV.SafeRelease();
	ColorComponentsUAV.SafeRelease();
	FVertexBuffer::ReleaseRHI();
}

void FDeformableColorVertexBuffer::AllocateData( )
{
	// Clear any old VertexData before allocating.
	CleanUp();

	VertexData = new FDeformableColorVertexData();
	// Calculate the vertex stride.
	Stride = VertexData->GetStride();
}

void FDeformableColorVertexBuffer::BindColorVertexBuffer(const FVertexFactory* VertexFactory, FStaticMeshDataType& StaticMeshData) const
{
	if (GetNumVertices() == 0)
	{
		BindDefaultColorVertexBuffer(VertexFactory, StaticMeshData, NullBindStride::ZeroForDefaultBufferBind);
		return;
	}

	StaticMeshData.ColorComponentsSRV = ColorComponentsSRV;
	// TODO:ここに相当するUAVの処理は必要ない？
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

void FDeformableColorVertexBuffer::BindDefaultColorVertexBuffer(const FVertexFactory* VertexFactory, FStaticMeshDataType& StaticMeshData, NullBindStride BindStride)
{
	StaticMeshData.ColorComponentsSRV = GNullColorVertexBuffer.VertexBufferSRV;
	// TODO:ここに相当するUAVの処理は必要ない？
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

void FDeformableVertexBuffers::InitFromDynamicVertex(FLocalVertexFactory* VertexFactory, const TArray<FDynamicMeshVertex>& Vertices, uint32 NumTexCoords, uint32 LightMapIndex)
{
	check(NumTexCoords < MAX_STATIC_TEXCOORDS && NumTexCoords > 0);
	check(LightMapIndex < NumTexCoords);

	if (Vertices.Num())
	{
		PositionVertexBuffer.Init(Vertices.Num());
		DeformableMeshVertexBuffer.Init(Vertices.Num(), NumTexCoords);
		ColorVertexBuffer.Init(Vertices.Num());

		for (int32 i = 0; i < Vertices.Num(); i++)
		{
			const FDynamicMeshVertex& Vertex = Vertices[i];

			PositionVertexBuffer.VertexPosition(i) = Vertex.Position;
			DeformableMeshVertexBuffer.SetVertexTangents(i, Vertex.TangentX.ToFVector(), Vertex.GetTangentY(), Vertex.TangentZ.ToFVector());
			for (uint32 j = 0; j < NumTexCoords; j++)
			{
				DeformableMeshVertexBuffer.SetVertexUV(i, j, Vertex.TextureCoordinate[j]);
			}
			ColorVertexBuffer.VertexColor(i) = Vertex.Color;
		}
	}
	else
	{
		PositionVertexBuffer.Init(1);
		DeformableMeshVertexBuffer.Init(1, 1);
		ColorVertexBuffer.Init(1);

		PositionVertexBuffer.VertexPosition(0) = FVector(0, 0, 0);
		DeformableMeshVertexBuffer.SetVertexTangents(0, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1));
		DeformableMeshVertexBuffer.SetVertexUV(0, 0, FVector2D(0, 0));
		ColorVertexBuffer.VertexColor(0) = FColor(1,1,1,1);
		NumTexCoords = 1;
		LightMapIndex = 0;
	}

	FDeformableVertexBuffers* Self = this;
	ENQUEUE_RENDER_COMMAND(InitDeformableVertexBuffers)(
		[VertexFactory, Self, LightMapIndex](FRHICommandListImmediate& RHICmdList)
		{
			InitOrUpdateResourceMacro(&Self->PositionVertexBuffer);
			InitOrUpdateResourceMacro(&Self->DeformableMeshVertexBuffer);
			InitOrUpdateResourceMacro(&Self->ColorVertexBuffer);

			FLocalVertexFactory::FDataType Data;
			Self->PositionVertexBuffer.BindPositionVertexBuffer(VertexFactory, Data);
			Self->DeformableMeshVertexBuffer.BindTangentVertexBuffer(VertexFactory, Data);
			Self->DeformableMeshVertexBuffer.BindPackedTexCoordVertexBuffer(VertexFactory, Data);
			Self->DeformableMeshVertexBuffer.BindLightMapVertexBuffer(VertexFactory, Data, LightMapIndex);
			Self->ColorVertexBuffer.BindColorVertexBuffer(VertexFactory, Data);
			VertexFactory->SetData(Data);

			InitOrUpdateResourceMacro(VertexFactory);
		});
}

