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
	FVector2D BottomRight = FVector2D::ZeroVector;
	/** Length of edge. */
	float Length = 0.0f;
	/** LOD level. */
	int32 LOD = 0;
	/** Indices of child nodes in render list. If a child does not exist, it is INDEX_NONE. */
	int32 ChildNodeIndices[4] = {INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE};
};

/** 
 * Check this node is leaf or not.
 */
bool IsLeaf(const FQuadNode& Node);

/** 
 * Build render list recursively.
 * @return assigned index for Node. If not assigned, INDEX_NONE.
 */
int32 BuildQuadNodeRenderListRecursively(int32 MaxLOD, int32 NumRowColumn, float MaxScreenCoverage, float PatchLength, const FVector& CameraPosition, const FMatrix& ViewProjectionMatrix, FQuadNode& Node, const class FSceneView& View, TArray<FQuadNode>& OutRenderList);
} // namespace Quadtree

