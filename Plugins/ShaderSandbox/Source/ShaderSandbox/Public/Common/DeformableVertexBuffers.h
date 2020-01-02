#pragma once

#include "CoreMinimal.h"
#include "RenderResource.h"
#include "Rendering/StaticMeshVertexBuffer.h"

class FDeformableMeshVertexBuffer : public FRenderResource
{
public:
	/** Default constructor. */
	FDeformableMeshVertexBuffer();

	/** Destructor. */
	~FDeformableMeshVertexBuffer();

	/** Delete existing resources */
	void CleanUp();

	void Init(uint32 InNumVertices, uint32 InNumTexCoords);

	FORCEINLINE_DEBUGGABLE void SetVertexTangents(uint32 VertexIndex, FVector X, FVector Y, FVector Z)
	{
		checkSlow(VertexIndex < GetNumVertices());

		typedef TStaticMeshVertexTangentDatum<typename TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::Default>::TangentTypeT> TangentType;
		TangentType* ElementData = reinterpret_cast<TangentType*>(TangentsDataPtr);
		check((void*)((&ElementData[VertexIndex]) + 1) <= (void*)(TangentsDataPtr + TangentsData->GetResourceSize()));
		check((void*)((&ElementData[VertexIndex]) + 0) >= (void*)(TangentsDataPtr));
		ElementData[VertexIndex].SetTangents(X, Y, Z);
	}

	/**
	* Set the vertex UV values at the given index in the vertex buffer
	*
	* @param VertexIndex - index into the vertex buffer
	* @param UVIndex - [0,MAX_STATIC_TEXCOORDS] value to index into UVs array
	* @param Vec2D - UV values to set
	*/
	FORCEINLINE_DEBUGGABLE void SetVertexUV(uint32 VertexIndex, uint32 UVIndex, const FVector2D& Vec2D)
	{
		checkSlow(VertexIndex < GetNumVertices());
		checkSlow(UVIndex < GetNumTexCoords());

		if (GetUseFullPrecisionUVs())
		{
			typedef TStaticMeshVertexUVsDatum<typename TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::HighPrecision>::UVsTypeT> UVType;
			size_t UvStride = sizeof(UVType) * GetNumTexCoords();

			UVType* ElementData = reinterpret_cast<UVType*>(TexcoordDataPtr + (VertexIndex * UvStride));
			check((void*)((&ElementData[UVIndex]) + 1) <= (void*)(TexcoordDataPtr + TexcoordData->GetResourceSize()));
			check((void*)((&ElementData[UVIndex]) + 0) >= (void*)(TexcoordDataPtr));
			ElementData[UVIndex].SetUV(Vec2D);
		}
		else
		{
			typedef TStaticMeshVertexUVsDatum<typename TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::Default>::UVsTypeT> UVType;
			size_t UvStride = sizeof(UVType) * GetNumTexCoords();

			UVType* ElementData = reinterpret_cast<UVType*>(TexcoordDataPtr + (VertexIndex * UvStride));
			check((void*)((&ElementData[UVIndex]) + 1) <= (void*)(TexcoordDataPtr + TexcoordData->GetResourceSize()));
			check((void*)((&ElementData[UVIndex]) + 0) >= (void*)(TexcoordDataPtr));
			ElementData[UVIndex].SetUV(Vec2D);
		}
	}

	FORCEINLINE_DEBUGGABLE uint32 GetNumVertices() const
	{
		return NumVertices;
	}

	FORCEINLINE_DEBUGGABLE uint32 GetNumTexCoords() const
	{
		return NumTexCoords;
	}

	FORCEINLINE_DEBUGGABLE bool GetUseFullPrecisionUVs() const
	{
		return bUseFullPrecisionUVs;
	}

	FORCEINLINE_DEBUGGABLE void SetUseFullPrecisionUVs(bool UseFull)
	{
		bUseFullPrecisionUVs = UseFull;
	}

	/** Create an RHI vertex buffer with CPU data. CPU data may be discarded after creation (see TResourceArray::Discard) */
	FVertexBufferRHIRef CreateTangentsRHIBuffer_RenderThread();
	FVertexBufferRHIRef CreateTexCoordRHIBuffer_RenderThread();

	// FRenderResource interface.
	virtual void InitRHI() override;
	virtual void ReleaseRHI() override;
	virtual void InitResource() override;
	virtual void ReleaseResource() override;
	virtual FString GetFriendlyName() const override { return TEXT("Deformable-mesh vertices"); }

	void BindTangentVertexBuffer(const class FVertexFactory* VertexFactory, struct FStaticMeshDataType& Data) const;
	void BindPackedTexCoordVertexBuffer(const class FVertexFactory* VertexFactory, struct FStaticMeshDataType& Data) const;
	void BindLightMapVertexBuffer(const class FVertexFactory* VertexFactory, struct FStaticMeshDataType& Data, int LightMapCoordinateIndex) const;

	class FTangentsVertexBuffer : public FVertexBuffer
	{
		virtual FString GetFriendlyName() const override { return TEXT("FTangentsVertexBuffer"); }
	} TangentsVertexBuffer;

	class FTexcoordVertexBuffer : public FVertexBuffer
	{
		virtual FString GetFriendlyName() const override { return TEXT("FTexcoordVertexBuffer"); }
	} TexCoordVertexBuffer;

	FRHIUnorderedAccessView* GetTangentsUAV() const { return TangentsUAV; }

private:

	/** The vertex data storage type */
	FStaticMeshVertexDataInterface* TangentsData;
	FShaderResourceViewRHIRef TangentsSRV;
	FUnorderedAccessViewRHIRef TangentsUAV;

	FStaticMeshVertexDataInterface* TexcoordData;
	FShaderResourceViewRHIRef TextureCoordinatesSRV;
	FUnorderedAccessViewRHIRef TextureCoordinatesUAV;

	/** The cached vertex data pointer. */
	uint8* TangentsDataPtr;
	uint8* TexcoordDataPtr;

	/** The cached Tangent stride. */
	uint32 TangentsStride;

	/** The cached Texcoord stride. */
	uint32 TexcoordStride;

	/** The number of texcoords/vertex in the buffer. */
	uint32 NumTexCoords;

	/** The cached number of vertices. */
	uint32 NumVertices;

	/** Corresponds to UStaticMesh::UseFullPrecisionUVs. if true then 32 bit UVs are used */
	bool bUseFullPrecisionUVs;

	/** Allocates the vertex data storage type. */
	void AllocateData();

	/** Convert half float data to full float if the HW requires it.
	* @param InData - optional half float source data to convert into full float texture coordinate buffer. if null, convert existing half float texture coordinates to a new float buffer.
	*/
	void ConvertHalfTexcoordsToFloat(const uint8* InData);

	void InitTangentAndTexCoordStrides();
};

/** A vertex that stores position and extra w. */
struct FDeformablePositionVertex
{
	FVector4 Position;

	friend FArchive& operator<<(FArchive& Ar, FDeformablePositionVertex& V)
	{
		Ar << V.Position;
		return Ar;
	}
};

class FDeformablePositionVertexBuffer : public FVertexBuffer
{
public:
	/** Default constructor. */
	FDeformablePositionVertexBuffer();

	/** Destructor. */
	~FDeformablePositionVertexBuffer();

	/** Delete existing resources */
	void CleanUp();

	void Init(uint32 NumVertices);

	virtual void InitRHI() override;
	virtual void ReleaseRHI() override;
	virtual FString GetFriendlyName() const override { return TEXT("PositionOnly Static-mesh vertices"); }

	// Vertex data accessors.
	FORCEINLINE FVector4& VertexPosition(uint32 VertexIndex)
	{
		checkSlow(VertexIndex < GetNumVertices());
		return ((FDeformablePositionVertex*)(Data + VertexIndex * Stride))->Position;
	}
	FORCEINLINE const FVector4& VertexPosition(uint32 VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		return ((FDeformablePositionVertex*)(Data + VertexIndex * Stride))->Position;
	}
	// Other accessors.
	FORCEINLINE uint32 GetStride() const
	{
		return Stride;
	}
	FORCEINLINE uint32 GetNumVertices() const
	{
		return NumVertices;
	}

	/** Create an RHI vertex buffer with CPU data. CPU data may be discarded after creation (see TResourceArray::Discard) */
	FVertexBufferRHIRef CreateRHIBuffer_RenderThread();

	void BindPositionVertexBuffer(const class FVertexFactory* VertexFactory, struct FStaticMeshDataType& Data) const;

	FRHIUnorderedAccessView* GetUAV() const { return PositionComponentUAV; }

private:
	FShaderResourceViewRHIRef PositionComponentSRV;
	FUnorderedAccessViewRHIRef PositionComponentUAV;

	/** The vertex data storage type */
	class FDeformablePositionVertexData* VertexData;

	/** The cached vertex data pointer. */
	uint8* Data;

	/** The cached vertex stride. */
	uint32 Stride;

	/** The cached number of vertices. */
	uint32 NumVertices;

	/** Allocates the vertex data storage type. */
	void AllocateData();
};

class FDeformableColorVertexBuffer : public FVertexBuffer
{
public:
	enum class NullBindStride
	{
		FColorSizeForComponentOverride, //when we want to bind null buffer with the expectation that it must be overridden.  Stride must be correct so override binds correctly
		ZeroForDefaultBufferBind, //when we want to bind the null color buffer for it to actually be used for draw.  Stride must be 0 so the IA gives correct data for all verts.
	};

	/** Default constructor. */
	FDeformableColorVertexBuffer();

	/** Destructor. */
	~FDeformableColorVertexBuffer();

	/** Delete existing resources */
	void CleanUp();

	void Init(uint32 InNumVertices);

	FORCEINLINE FColor& VertexColor(uint32 VertexIndex)
	{
		checkSlow(VertexIndex < GetNumVertices());
		return *(FColor*)(Data + VertexIndex * Stride);
	}

	FORCEINLINE const FColor& VertexColor(uint32 VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		return *(FColor*)(Data + VertexIndex * Stride);
	}

	// Other accessors.
	FORCEINLINE uint32 GetStride() const
	{
		return Stride;
	}
	FORCEINLINE uint32 GetNumVertices() const
	{
		return NumVertices;
	}

	// FRenderResource interface.
	virtual void InitRHI() override;
	virtual void ReleaseRHI() override;
	virtual FString GetFriendlyName() const override { return TEXT("ColorOnly Deformable Mesh Vertices"); }

	/** Create an RHI vertex buffer with CPU data. CPU data may be discarded after creation (see TResourceArray::Discard) */
	FVertexBufferRHIRef CreateRHIBuffer_RenderThread();

	void BindColorVertexBuffer(const class FVertexFactory* VertexFactory, struct FStaticMeshDataType& StaticMeshData) const;
	static void BindDefaultColorVertexBuffer(const class FVertexFactory* VertexFactory, struct FStaticMeshDataType& StaticMeshData, NullBindStride BindStride);

private:
	/** The vertex data storage type */
	class FDeformableColorVertexData* VertexData;

	FShaderResourceViewRHIRef ColorComponentsSRV;
	FUnorderedAccessViewRHIRef ColorComponentsUAV;

	/** The cached vertex data pointer. */
	uint8* Data;

	/** The cached vertex stride. */
	uint32 Stride;

	/** The cached number of vertices. */
	uint32 NumVertices;

	/** Allocates the vertex data storage type. */
	void AllocateData();
};

struct FDeformableVertexBuffers
{
	/** The buffer containing vertex data. */
	FDeformableMeshVertexBuffer DeformableMeshVertexBuffer;
	/** The buffer containing the position vertex data. */
	FDeformablePositionVertexBuffer PositionVertexBuffer;
	/** The buffer containing the vertex color data. */
	FDeformableColorVertexBuffer ColorVertexBuffer;

#if RHI_RAYTRACING
	FRayTracingGeometry RayTracingGeometry;
#endif

	virtual ~FDeformableVertexBuffers() {}
	/* This is a temporary function to refactor and convert old code, do not copy this as is and try to build your data as SoA from the beginning.*/
	void InitFromDynamicVertex(class FLocalVertexFactory* VertexFactory, const TArray<struct FDynamicMeshVertex>& Vertices, uint32 NumTexCoords = 1, uint32 LightMapIndex = 0);
};

