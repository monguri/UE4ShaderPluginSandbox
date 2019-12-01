#include "DeformMesh/ClothVertexBuffers.h"
#include "DynamicMeshBuilder.h"

static inline void InitOrUpdateResourceMacroCloth(FRenderResource* Resource)
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

void FClothVertexBuffers::InitFromDynamicVertex(FLocalVertexFactory* VertexFactory, TArray<FDynamicMeshVertex>& Vertices, uint32 NumTexCoords, uint32 LightMapIndex)
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
			InitOrUpdateResourceMacroCloth(&Self->PositionVertexBuffer);
			InitOrUpdateResourceMacroCloth(&Self->ComputableMeshVertexBuffer);
			InitOrUpdateResourceMacroCloth(&Self->ColorVertexBuffer);

			FLocalVertexFactory::FDataType Data;
			Self->PositionVertexBuffer.BindPositionVertexBuffer(VertexFactory, Data);
			Self->ComputableMeshVertexBuffer.BindTangentVertexBuffer(VertexFactory, Data);
			Self->ComputableMeshVertexBuffer.BindPackedTexCoordVertexBuffer(VertexFactory, Data);
			Self->ComputableMeshVertexBuffer.BindLightMapVertexBuffer(VertexFactory, Data, LightMapIndex);
			Self->ColorVertexBuffer.BindColorVertexBuffer(VertexFactory, Data);
			VertexFactory->SetData(Data);

			InitOrUpdateResourceMacroCloth(VertexFactory);
		});
}

