#pragma once

#include "Common/ComputableVertexBuffers.h"
#include "Rendering/PositionVertexBuffer.h"

struct FClothVertexBuffers : public FComputableVertexBuffers 
{
	/** The buffer containing the previous frame position vertex data. */
	FComputablePositionVertexBuffer PrevPositionVertexBuffer;
	/** The buffer containing the acceralation vertex data. */
	FPositionVertexBuffer AcceralationVertexBuffer; // FPositionVertexBufferを加速度用の頂点属性バッファに使用する

	virtual ~FClothVertexBuffers() {}
	/* This is a temporary function to refactor and convert old code, do not copy this as is and try to build your data as SoA from the beginning.*/
	void InitFromClothVertexAttributes(class FLocalVertexFactory* VertexFactory, const TArray<struct FDynamicMeshVertex>& Vertices,  const TArray<float>& InvMasses, const TArray<FVector>& Accelerations, uint32 NumTexCoords = 1, uint32 LightMapIndex = 0);

	/* Set acceralation vertex buffer. */
	void SetAccelerations(const TArray<FVector>& Accelerations);
};

