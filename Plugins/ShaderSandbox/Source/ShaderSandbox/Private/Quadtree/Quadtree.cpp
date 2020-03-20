#include "Quadtree.h"
#include "SceneView.h"

namespace
{
// フラスタムカリング。ビューフラスタムの中にQuadNodeが一部でも入っていたらtrue。そうでなければfalse。
bool IsQuadNodeFrustumCulled(const Quadtree::FQuadNode& Node, const FSceneView& View)
{
	return true;
}

// QuadNodeのメッシュのグリッドの中でもっともカメラに近いもののスクリーン表示面積率を取得する
float EstimateGridScreenCoverage(const Quadtree::FQuadNode& Node, const FSceneView& View)
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

int32 BuildQuadNodeRenderListRecursively(FQuadNode& Node, const ::FSceneView& View, TArray<FQuadNode>& OutRenderList)
{
	bool bCulled = IsQuadNodeFrustumCulled(Node, View);
	if (bCulled)
	{
		return INDEX_NONE;
	}

	// TODO:縦横8ピクセルのスクリーンカバレッジ比率が上限。それ以上であればグリッドが大きすぎるので子に分割する。マジックナンバー
	// TODO:本来はこの比率を引数で渡すべき。再帰のたびに計算している
	float MaxCoverage = 8 / View.SceneViewInitOptions.GetConstrainedViewRect().Size().X * 8 / View.SceneViewInitOptions.GetConstrainedViewRect().Size().Y;
	float GridCoverage = EstimateGridScreenCoverage(Node, View);

	float PatchLength = 2000; // TODO:これも引数か何かで与えるべき
	bool bVisible = true;
	if (GridCoverage > MaxCoverage && Node.Length > PatchLength)
	{
		// LODとchildListIndicesは初期値通り
		FQuadNode ChildNodeBottomLeft;
		ChildNodeBottomLeft.BottomLeft = Node.BottomLeft;
		ChildNodeBottomLeft.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[0] = BuildQuadNodeRenderListRecursively(ChildNodeBottomLeft, View, OutRenderList);

		FQuadNode ChildNodeBottomRight;
		ChildNodeBottomRight.BottomLeft = Node.BottomLeft + FVector2D(Node.Length / 2.0f, 0.0f);
		ChildNodeBottomRight.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[1] = BuildQuadNodeRenderListRecursively(ChildNodeBottomRight, View, OutRenderList);

		FQuadNode ChildNodeTopLeft;
		ChildNodeTopLeft.BottomLeft = Node.BottomLeft + FVector2D(0.0f, Node.Length / 2.0f);
		ChildNodeTopLeft.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[2] = BuildQuadNodeRenderListRecursively(ChildNodeTopLeft, View, OutRenderList);

		FQuadNode ChildNodeTopRight;
		ChildNodeTopRight.BottomLeft = Node.BottomLeft + FVector2D(Node.Length / 2.0f, Node.Length / 2.0f);
		ChildNodeTopRight.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[3] = BuildQuadNodeRenderListRecursively(ChildNodeTopRight, View, OutRenderList);

		bVisible = !IsLeaf(Node);
	}

	if (bVisible)
	{
		// メッシュのLODをスクリーンカバレッジから決める
		// LODレベルが上がるごとに縦横2倍するので、グリッドカバレッジが上限値の1/4^xになっていればxがLODレベルである
		int32 LOD = 0;
		for (; LOD < 8 - 1; LOD++)  //TODO:最大LODレベル8がマジックナンバー
		{
			if (GridCoverage > MaxCoverage)
			{
				break;
			}

			GridCoverage *= 4;
		}

		// TODO:LODの最大レベルと、それよりひとつ下のレベルは使わない。あとで考え直す
		Node.LOD = FMath::Min(LOD, 8 - 2);
	}
	else
	{
		return INDEX_NONE;
	}

	int32 Index = OutRenderList.Add(Node);
	return Index;
}
} // namespace Quadtree


