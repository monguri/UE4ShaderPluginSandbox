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

void FClothVertexBuffers::InitFromClothVertexAttributes(FLocalVertexFactory* VertexFactory, const TArray<FDynamicMeshVertex>& Vertices, const TArray<float>& InvMasses, const TArray<FVector>& Accelerations, uint32 NumTexCoords, uint32 LightMapIndex)
{
	check(NumTexCoords < MAX_STATIC_TEXCOORDS && NumTexCoords > 0);
	check(LightMapIndex < NumTexCoords);
	check(Vertices.Num() == InvMasses.Num());
	check(Vertices.Num() == Accelerations.Num());

	if (Vertices.Num())
	{
		PositionVertexBuffer.Init(Vertices.Num());
		ComputableMeshVertexBuffer.Init(Vertices.Num(), NumTexCoords);
		ColorVertexBuffer.Init(Vertices.Num());
		PrevPositionVertexBuffer.Init(Vertices.Num());
		AcceralationVertexBuffer.Init(Vertices.Num());

		for (int32 i = 0; i < Vertices.Num(); i++)
		{
			const FDynamicMeshVertex& Vertex = Vertices[i];
			float InvMass = InvMasses[i];

			PositionVertexBuffer.VertexPosition(i) = FVector4(Vertex.Position, InvMass);
			ComputableMeshVertexBuffer.SetVertexTangents(i, Vertex.TangentX.ToFVector(), Vertex.GetTangentY(), Vertex.TangentZ.ToFVector());
			for (uint32 j = 0; j < NumTexCoords; j++)
			{
				ComputableMeshVertexBuffer.SetVertexUV(i, j, Vertex.TextureCoordinate[j]);
			}
			ColorVertexBuffer.VertexColor(i) = Vertex.Color;
			// 前フレームの位置は初期化では現フレームと同じにする
			PrevPositionVertexBuffer.VertexPosition(i) = FVector4(Vertex.Position, InvMass);
			AcceralationVertexBuffer.VertexPosition(i) = Accelerations[i];
		}
	}
	else
	{
		PositionVertexBuffer.Init(1);
		ComputableMeshVertexBuffer.Init(1, 1);
		ColorVertexBuffer.Init(1);
		PrevPositionVertexBuffer.Init(1);
		AcceralationVertexBuffer.Init(1);

		PositionVertexBuffer.VertexPosition(0) = FVector4(0, 0, 0, 0);
		ComputableMeshVertexBuffer.SetVertexTangents(0, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1));
		ComputableMeshVertexBuffer.SetVertexUV(0, 0, FVector2D(0, 0));
		ColorVertexBuffer.VertexColor(0) = FColor(1,1,1,1);
		PrevPositionVertexBuffer.VertexPosition(0) = FVector4(0, 0, 0, 0);
		AcceralationVertexBuffer.VertexPosition(0) = FVector(0, 0, -980.0f);
		NumTexCoords = 1;
		LightMapIndex = 0;
	}

	FClothVertexBuffers* Self = this;
	ENQUEUE_RENDER_COMMAND(InitClothVertexBuffers)(
		[VertexFactory, Self, LightMapIndex](FRHICommandListImmediate& RHICmdList)
		{
			InitOrUpdateResourceMacroCloth(&Self->PositionVertexBuffer);
			InitOrUpdateResourceMacroCloth(&Self->ComputableMeshVertexBuffer);
			InitOrUpdateResourceMacroCloth(&Self->ColorVertexBuffer);
			InitOrUpdateResourceMacroCloth(&Self->PrevPositionVertexBuffer);
			InitOrUpdateResourceMacroCloth(&Self->AcceralationVertexBuffer);

			FLocalVertexFactory::FDataType Data;
			Self->PositionVertexBuffer.BindPositionVertexBuffer(VertexFactory, Data);
			Self->ComputableMeshVertexBuffer.BindTangentVertexBuffer(VertexFactory, Data);
			Self->ComputableMeshVertexBuffer.BindPackedTexCoordVertexBuffer(VertexFactory, Data);
			Self->ComputableMeshVertexBuffer.BindLightMapVertexBuffer(VertexFactory, Data, LightMapIndex);
			Self->ColorVertexBuffer.BindColorVertexBuffer(VertexFactory, Data);
			// PrevPositionVertexBuffer、AcceralationVertexBufferはシミュレーション用のデータなのでLocalVertexFactoryとバインドする必要はない
			VertexFactory->SetData(Data);

			InitOrUpdateResourceMacroCloth(VertexFactory);
		});
}

void FClothVertexBuffers::SetAccelerations(const TArray<FVector>& Accelerations)
{
	for (int32 i = 0; i < Accelerations.Num(); i++)
	{
		AcceralationVertexBuffer.VertexPosition(i) = Accelerations[i];
	}
}

