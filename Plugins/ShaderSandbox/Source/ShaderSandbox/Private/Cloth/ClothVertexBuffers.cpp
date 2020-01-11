#include "Cloth/ClothVertexBuffers.h"
#include "Cloth/ClothGridMeshParameters.h"
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

void FClothVertexBuffers::InitFromClothVertexAttributes(FLocalVertexFactory* VertexFactory, const TArray<FDynamicMeshVertex>& Vertices, const TArray<float>& InvMasses, const TArray<FVector>& AccelerationMoves, uint32 NumTexCoords, uint32 LightMapIndex)
{
	check(NumTexCoords < MAX_STATIC_TEXCOORDS && NumTexCoords > 0);
	check(LightMapIndex < NumTexCoords);
	check(Vertices.Num() == InvMasses.Num());
	check(Vertices.Num() == AccelerationMoves.Num());

	if (Vertices.Num())
	{
		PositionVertexBuffer.Init(Vertices.Num());
		DeformableMeshVertexBuffer.Init(Vertices.Num(), NumTexCoords);
		ColorVertexBuffer.Init(Vertices.Num());
		PrevPositionVertexBuffer.Init(Vertices.Num());
		AccelerationMoveVertexBuffer.Init(Vertices.Num());

		for (int32 i = 0; i < Vertices.Num(); i++)
		{
			const FDynamicMeshVertex& Vertex = Vertices[i];
			float InvMass = InvMasses[i];

			PositionVertexBuffer.VertexPosition(i) = FVector4(Vertex.Position, InvMass);
			DeformableMeshVertexBuffer.SetVertexTangents(i, Vertex.TangentX.ToFVector(), Vertex.GetTangentY(), Vertex.TangentZ.ToFVector());
			for (uint32 j = 0; j < NumTexCoords; j++)
			{
				DeformableMeshVertexBuffer.SetVertexUV(i, j, Vertex.TextureCoordinate[j]);
			}
			ColorVertexBuffer.VertexColor(i) = Vertex.Color;
			// 前フレームの位置は初期化では現フレームと同じにする
			PrevPositionVertexBuffer.VertexPosition(i) = FVector4(Vertex.Position, InvMass);
			AccelerationMoveVertexBuffer.VertexPosition(i) = AccelerationMoves[i];
		}
	}
	else
	{
		PositionVertexBuffer.Init(1);
		DeformableMeshVertexBuffer.Init(1, 1);
		ColorVertexBuffer.Init(1);
		PrevPositionVertexBuffer.Init(1);
		AccelerationMoveVertexBuffer.Init(1);

		PositionVertexBuffer.VertexPosition(0) = FVector4(0, 0, 0, 0);
		DeformableMeshVertexBuffer.SetVertexTangents(0, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1));
		DeformableMeshVertexBuffer.SetVertexUV(0, 0, FVector2D(0, 0));
		ColorVertexBuffer.VertexColor(0) = FColor(1,1,1,1);
		PrevPositionVertexBuffer.VertexPosition(0) = FVector4(0, 0, 0, 0);

		float IterDeltaTime = 1.0f / FGridClothParameters::BASE_FREQUENCY;
		float SqrIterDeltaTime = IterDeltaTime * IterDeltaTime;

		AccelerationMoveVertexBuffer.VertexPosition(0) = FGridClothParameters::GRAVITY * SqrIterDeltaTime;
		NumTexCoords = 1;
		LightMapIndex = 0;
	}

	FClothVertexBuffers* Self = this;
	ENQUEUE_RENDER_COMMAND(InitClothVertexBuffers)(
		[VertexFactory, Self, LightMapIndex](FRHICommandListImmediate& RHICmdList)
		{
			InitOrUpdateResourceMacroCloth(&Self->PositionVertexBuffer);
			InitOrUpdateResourceMacroCloth(&Self->DeformableMeshVertexBuffer);
			InitOrUpdateResourceMacroCloth(&Self->ColorVertexBuffer);
			InitOrUpdateResourceMacroCloth(&Self->PrevPositionVertexBuffer);
			InitOrUpdateResourceMacroCloth(&Self->AccelerationMoveVertexBuffer);

			FLocalVertexFactory::FDataType Data;
			Self->PositionVertexBuffer.BindPositionVertexBuffer(VertexFactory, Data);
			Self->DeformableMeshVertexBuffer.BindTangentVertexBuffer(VertexFactory, Data);
			Self->DeformableMeshVertexBuffer.BindPackedTexCoordVertexBuffer(VertexFactory, Data);
			Self->DeformableMeshVertexBuffer.BindLightMapVertexBuffer(VertexFactory, Data, LightMapIndex);
			Self->ColorVertexBuffer.BindColorVertexBuffer(VertexFactory, Data);
			// PrevPositionVertexBuffer、AccelerationMoveVertexBufferはシミュレーション用のデータなのでLocalVertexFactoryとバインドする必要はない
			VertexFactory->SetData(Data);

			InitOrUpdateResourceMacroCloth(VertexFactory);
		});
}

void FClothVertexBuffers::SetAccelerationMoves(const TArray<FVector>& AccelerationMoves)
{
	for (int32 i = 0; i < AccelerationMoves.Num(); i++)
	{
		AccelerationMoveVertexBuffer.VertexPosition(i) = AccelerationMoves[i];
	}

	FClothVertexBuffers* Self = this;
	ENQUEUE_RENDER_COMMAND(UpdateAccelerationVertexBuffers)(
		[Self](FRHICommandListImmediate& RHICmdList)
		{
			InitOrUpdateResourceMacroCloth(&Self->AccelerationMoveVertexBuffer);
		});
}

