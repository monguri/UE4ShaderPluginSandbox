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

	bool IsLeaf() const
	{
		return (ChildNodeIndices[0] == INDEX_NONE) && (ChildNodeIndices[1] == INDEX_NONE) && (ChildNodeIndices[2] == INDEX_NONE) && (ChildNodeIndices[3] == INDEX_NONE);
	}
};

/** 
 * Build quad tree from given root node, and return two list.
 * One is all quad node in quadtree, another is list which should be rendered.
 * @return assigned index for Node. If not assigned, INDEX_NONE.
 */
void BuildQuadtree(int32 MaxLOD, int32 NumRowColumn, float MaxScreenCoverage, float PatchLength, const FVector& CameraPosition, const FMatrix& ViewProjectionMatrix, FQuadNode& RootNode, TArray<FQuadNode>& OutAllQuadNodeList, TArray<FQuadNode>& OutRenderQuadNodeList);
} // namespace Quadtree

