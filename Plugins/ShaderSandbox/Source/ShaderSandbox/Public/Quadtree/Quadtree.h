#pragma once

#include "CoreMinimal.h"

namespace Quadtree
{
//
// QuadNode is on XY plane.
// Using UE4 left hand coordinate.
// 
//      Z   
//      |  Y 
//      | /
//      |/
//   X---
//

/** Node of Quadtree. Square. */
struct FQuadNode
{
	/** Position of bottom right corner. */
	FVector BottomRight = FVector::ZeroVector;
	/** Length of edge. */
	float Length = 0.0f;
	/** LOD level. */
	int32 LOD = 0;
	/** Indices of child nodes in render list. If a child does not exist, it is INDEX_NONE. */
	int32 ChildNodeIndices[4] = {INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE};

	bool IsLeaf() const;
	bool ContainsPosition2D(const FVector2D& Position2D) const;
};

/** Mesh for quad node have several pattern by LOD of adjacent quad nodes. An index buffer contains all pattern. This is parameters to reference the index buffer. */
struct FQuadMeshParameter
{
	/** Position of bottom right corner. */
	uint32 IndexBufferOffset = 0;
	/** The number of indices. */
	uint32 NumIndices = 0;

	FQuadMeshParameter(uint32 Offset, uint32 Num) : IndexBufferOffset(Offset), NumIndices(Num) {}
};

enum class EAdjacentQuadNodeLODDifference : uint8
{
	LESS_OR_EQUAL_OR_NOT_EXIST = 0,
	GREATER_BY_1 = 1,
	GREATER_BY_MORE_THAN_2 = 2,
	MAX = 3,
};

/** 
 * Build quad tree from given root node, and return two list.
 * One is all quad node in quadtree, another is list which should be rendered.
 */
void BuildQuadtree(int32 MaxLOD, int32 NumRowColumn, float MaxScreenCoverage, float PatchLength, const FVector& CameraPosition, const FVector2D& ProjectionScale, const FMatrix& ViewProjectionMatrix, FQuadNode& RootNode, TArray<FQuadNode>& OutAllQuadNodeList, TArray<FQuadNode>& OutRenderQuadNodeList);

EAdjacentQuadNodeLODDifference QueryAdjacentNodeType(const FQuadNode& Node, const FVector2D& AdjacentPosition, const TArray<FQuadNode>& RenderQuadNodeList);
} // namespace Quadtree

