#pragma once

#include "Common/DeformableVertexBuffers.h"
#include "Rendering/PositionVertexBuffer.h"

struct FClothVertexBuffers : public FDeformableVertexBuffers 
{
	/** The buffer containing the previous frame position vertex data. */
	FDeformablePositionVertexBuffer PrevPositionVertexBuffer;
	/** The buffer containing the translation by acceralation vertex data. */
	FPositionVertexBuffer AccelerationMoveVertexBuffer;

	virtual ~FClothVertexBuffers() {}
	/* This is a temporary function to refactor and convert old code, do not copy this as is and try to build your data as SoA from the beginning.*/
	void InitFromClothVertexAttributes(class FLocalVertexFactory* VertexFactory, const TArray<struct FDynamicMeshVertex>& Vertices,  const TArray<float>& InvMasses, const TArray<FVector>& AccelerationMoves, uint32 NumTexCoords = 1, uint32 LightMapIndex = 0);

	/* Set translation by acceralation vertex buffer. */
	void SetAccelerationMoves(const TArray<FVector>& AccelerationMoves);
};

