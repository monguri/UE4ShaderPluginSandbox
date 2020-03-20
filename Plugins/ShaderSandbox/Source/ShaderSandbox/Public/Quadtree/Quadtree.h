#pragma once

#include "CoreMinimal.h"

namespace Quadtree
{
/** Node of Quadtree. Square. */
struct FQuadNode
{
	/** Position of bottom left corner. */
	FVector2D BottomLeft;
	/** Length of edge. */
	float Length;
	/** LOD level. */
	int32 Lod;
	/** Indices of child nodes in render list. If a child does not exist, it is INDEX_NONE. */
	int32 ChildNodeIndices[4];
};

/** 
 * Check this node is leaf or not.
 */
bool IsLeaf(const FQuadNode& Node);

/** 
 * Build render list recursively.
 * @return assigned index for Node. If not assigned, INDEX_NONE.
 */
int32 BuildQuadNodeRenderListRecursively(FQuadNode& Node, const class FSceneView& View, TArray<FQuadNode>& OutRenderList);
} // namespace Quadtree

