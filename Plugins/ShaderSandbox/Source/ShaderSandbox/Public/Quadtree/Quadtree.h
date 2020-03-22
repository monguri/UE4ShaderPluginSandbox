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

bool IsQuadNodeFrustumCulled(const FMatrix& ViewProjectionMatrix, const Quadtree::FQuadNode& Node);
float EstimateGridScreenCoverage(int32 NumRowColumn, const FVector& CameraPosition, const FMatrix& ViewProjectionMatrix, const Quadtree::FQuadNode& Node, FIntPoint& OutNearestGrid);

/** 
 * Check this node is leaf or not.
 */
bool IsLeaf(const FQuadNode& Node);

/** 
 * Build render list recursively.
 * @return assigned index for Node. If not assigned, INDEX_NONE.
 */
int32 BuildQuadNodeRenderListRecursively(int32 NumRowColumn, float MaxScreenCoverage, float PatchLength, const FVector& CameraPosition, const FMatrix& ViewProjectionMatrix, FQuadNode& Node, const class FSceneView& View, TArray<FQuadNode>& OutRenderList);
} // namespace Quadtree

