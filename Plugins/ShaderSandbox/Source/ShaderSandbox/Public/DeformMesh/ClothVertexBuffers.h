#pragma once

#include "Common/ComputableVertexBuffers.h"

struct FClothVertexBuffers : public FComputableVertexBuffers 
{
	virtual ~FClothVertexBuffers() {}
	/* This is a temporary function to refactor and convert old code, do not copy this as is and try to build your data as SoA from the beginning.*/
	virtual void InitFromDynamicVertex(class FLocalVertexFactory* VertexFactory, TArray<struct FDynamicMeshVertex>& Vertices, uint32 NumTexCoords = 1, uint32 LightMapIndex = 0) override;
};

