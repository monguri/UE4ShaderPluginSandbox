#include "Quadtree.h"
#include "SceneView.h"

namespace
{
bool IsInNDCCube(const FVector4& PositionProjectionSpace)
{
	if (PositionProjectionSpace.W > KINDA_SMALL_NUMBER)
	{
		const FVector4& PositionNDC = PositionProjectionSpace / PositionProjectionSpace.W;
		if (PositionNDC.X > -1.0f && PositionNDC.X < 1.0f
			&& PositionNDC.Y > -1.0f && PositionNDC.Y < 1.0f
			&& PositionNDC.Z > -1.0f && PositionNDC.Z < 1.0f)
		{
			return true;
		}
	}

	return false;
}

// フラスタムカリング。ビューフラスタムの中にQuadNodeが一部でも入っていたらtrue。そうでなければfalse。
bool IsQuadNodeFrustumCulled(const Quadtree::FQuadNode& Node, const FSceneView& View)
{
	//FSceneView::ProjectWorldToScreen()を参考にしている
	const FMatrix& ViewProjMat = View.ViewMatrices.GetViewProjectionMatrix();

	// コーナーのすべての点がNDC（正規化デバイス座標系）での[-1,1]のキューブに入ってなければカリング対象

	const FVector4& BottomLeftProjSpace = ViewProjMat.TransformFVector4(FVector4(Node.BottomLeft.X, Node.BottomLeft.Y, 0.0f, 1.0f));
	if (IsInNDCCube(BottomLeftProjSpace))
	{
		return false;
	}

	const FVector4& BottomRightProjSpace = ViewProjMat.TransformFVector4(FVector4(Node.BottomLeft.X + Node.Length, Node.BottomLeft.Y, 0.0f, 1.0f));
	if (IsInNDCCube(BottomRightProjSpace))
	{
		return false;
	}

	const FVector4& TopLeftProjSpace = ViewProjMat.TransformFVector4(FVector4(Node.BottomLeft.X, Node.BottomLeft.Y + Node.Length, 0.0f, 1.0f));
	if (IsInNDCCube(TopLeftProjSpace))
	{
		return false;
	}

	const FVector4& TopRightProjSpace = ViewProjMat.TransformFVector4(FVector4(Node.BottomLeft.X + Node.Length, Node.BottomLeft.Y + Node.Length, 0.0f, 1.0f));
	if (IsInNDCCube(TopRightProjSpace))
	{
		return false;
	}

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

int32 BuildQuadNodeRenderListRecursively(float MaxScreenCoverage, float PatchLength, FQuadNode& Node, const ::FSceneView& View, TArray<FQuadNode>& OutRenderList)
{
	bool bCulled = IsQuadNodeFrustumCulled(Node, View);
	if (bCulled)
	{
		return INDEX_NONE;
	}

	// QuadNodeに許容するスクリーンカバレッジ比率の上限。
	//float MaxScreenCoverage = MaxPixelCoverage / View.SceneViewInitOptions.GetConstrainedViewRect().Size().X / View.SceneViewInitOptions.GetConstrainedViewRect().Size().Y;

	// QuadNodeの全グリッドのスクリーンカバレッジ比率の中で最大のもの。
	float GridCoverage = EstimateGridScreenCoverage(Node, View);

	bool bVisible = true;
	if (GridCoverage > MaxScreenCoverage // グリッドがスクリーンカバレッジ上限より大きければ子に分割する。自分を描画しないのはリストの使用側でIsLeafで判定する。
		&& Node.Length > PatchLength) // パッチサイズ以下の縦横長さであればそれ以上分割しない。上の条件だけだとカメラが近いといくらでも小さく分割してしまうので
	{
		// LODとchildListIndicesは初期値通り
		FQuadNode ChildNodeBottomLeft;
		ChildNodeBottomLeft.BottomLeft = Node.BottomLeft;
		ChildNodeBottomLeft.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[0] = BuildQuadNodeRenderListRecursively(MaxScreenCoverage, PatchLength, ChildNodeBottomLeft, View, OutRenderList);

		FQuadNode ChildNodeBottomRight;
		ChildNodeBottomRight.BottomLeft = Node.BottomLeft + FVector2D(Node.Length / 2.0f, 0.0f);
		ChildNodeBottomRight.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[1] = BuildQuadNodeRenderListRecursively(MaxScreenCoverage, PatchLength, ChildNodeBottomRight, View, OutRenderList);

		FQuadNode ChildNodeTopLeft;
		ChildNodeTopLeft.BottomLeft = Node.BottomLeft + FVector2D(0.0f, Node.Length / 2.0f);
		ChildNodeTopLeft.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[2] = BuildQuadNodeRenderListRecursively(MaxScreenCoverage, PatchLength, ChildNodeTopLeft, View, OutRenderList);

		FQuadNode ChildNodeTopRight;
		ChildNodeTopRight.BottomLeft = Node.BottomLeft + FVector2D(Node.Length / 2.0f, Node.Length / 2.0f);
		ChildNodeTopRight.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[3] = BuildQuadNodeRenderListRecursively(MaxScreenCoverage, PatchLength, ChildNodeTopRight, View, OutRenderList);

		// すべての子ノードがフラスタムカリング対象だったら、自分も不可視なはずで、カリング計算で漏れたとみるべきなのでカリングする
		bVisible = !IsLeaf(Node);
	}

	if (bVisible)
	{
		// TODO:ここの計算も関数化したいな。。。
		// メッシュのLODをスクリーンカバレッジから決める
		// LODレベルが上がるごとに縦横2倍するので、グリッドカバレッジが上限値の1/4^xになっていればxがLODレベルである
		int32 LOD = 0;
		for (; LOD < 8 - 1; LOD++)  //TODO:最大LODレベル8がマジックナンバー
		{
			if (GridCoverage > MaxScreenCoverage)
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


