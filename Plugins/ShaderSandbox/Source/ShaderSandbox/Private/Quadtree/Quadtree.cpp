#include "Quadtree.h"

namespace
{
// フラスタムカリング。カメラフラスタムの中にQuadNodeが一部でも入っていたらtrue。そうでなければfalse。
bool IsQuadNodeInCameraFrustum(const Quadtree::FQuadNode& Node, const class FSceneView& View)
{
	return false;
}

// QuadNodeのメッシュのグリッドの中でもっともカメラに近いもののスクリーン表示面積率を取得する
float EstimateGridScreenCoverage(const Quadtree::FQuadNode& Node, const class FSceneView& View)
{
	return 0.0f;
}

} // namespace

namespace Quadtree
{
bool IsLeaf(const FQuadNode& Node)
{
	// ChildNodeIndices[]でひとつでも有効なインデックスのものがあるかどうかでLeafかどうか判定している
	for (int32 i = 0; i < 4; i++)
	{
		if (Node.ChildNodeIndices[i] != INDEX_NONE)
		{
			return false;
		}
	}

	return true;
}

int32 BuildQuadNodeRenderListRecursively(FQuadNode& Node, const class FSceneView& View, TArray<FQuadNode>& OutRenderList)
{
	return INDEX_NONE;
}
} // namespace Quadtree

