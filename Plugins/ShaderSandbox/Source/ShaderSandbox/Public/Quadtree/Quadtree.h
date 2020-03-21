#pragma once

#include "CoreMinimal.h"

namespace Quadtree
{
/** Node of Quadtree. Square. */
struct FQuadNode
{
	/** Position of bottom left corner. */
	FVector2D BottomLeft = FVector2D::ZeroVector;
	/** Length of edge. */
	float Length = 0.0f;
	/** LOD level. */
	int32 LOD = 0;
	/** Indices of child nodes in render list. If a child does not exist, it is INDEX_NONE. */
	int32 ChildNodeIndices[4] = {INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE};
};

// TODO:
bool IsQuadNodeFrustumCulled(const Quadtree::FQuadNode& Node, const ::FSceneView& View);

/** 
 * Check this node is leaf or not.
 */
bool IsLeaf(const FQuadNode& Node);

/** 
 * Build render list recursively.
 * @return assigned index for Node. If not assigned, INDEX_NONE.
 */
int32 BuildQuadNodeRenderListRecursively(float MaxScreenCoverage, float PatchLength, FQuadNode& Node, const class FSceneView& View, TArray<FQuadNode>& OutRenderList);
} // namespace Quadtree

