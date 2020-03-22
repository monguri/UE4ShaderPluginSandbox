#include "Quadtree.h"

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
} // namespace

namespace Quadtree
{
// フラスタムカリング。ビューフラスタムの中にQuadNodeが一部でも入っていたらtrue。そうでなければfalse。
bool IsQuadNodeFrustumCulled(const FMatrix& ViewProjectionMatrix, const Quadtree::FQuadNode& Node)
{
	//FSceneView::ProjectWorldToScreen()を参考にしている

	// NDCへの変換でWで除算したいのでWが0に近い場合はカリング対象として扱う
	const FVector4& BottomLeftProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(Node.BottomLeft.X, Node.BottomLeft.Y, 0.0f, 1.0f));
	if (BottomLeftProjSpace.W < KINDA_SMALL_NUMBER)
	{
		return true;
	}
	
	const FVector4& BottomRightProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(Node.BottomLeft.X + Node.Length, Node.BottomLeft.Y, 0.0f, 1.0f));
	if (BottomRightProjSpace.W < KINDA_SMALL_NUMBER)
	{
		return true;
	}

	const FVector4& TopLeftProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(Node.BottomLeft.X, Node.BottomLeft.Y + Node.Length, 0.0f, 1.0f));
	if (TopLeftProjSpace.W < KINDA_SMALL_NUMBER)
	{
		return true;
	}

	const FVector4& TopRightProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(Node.BottomLeft.X + Node.Length, Node.BottomLeft.Y + Node.Length, 0.0f, 1.0f));
	if (TopRightProjSpace.W < KINDA_SMALL_NUMBER)
	{
		return true;
	}

	const FVector4& BottomLeftNDC = BottomLeftProjSpace / BottomLeftProjSpace.W;
	const FVector4& BottomRightNDC = BottomRightProjSpace / BottomRightProjSpace.W;
	const FVector4& TopLeftNDC = TopLeftProjSpace / TopLeftProjSpace.W;
	const FVector4& TopRightNDC = TopRightProjSpace / TopRightProjSpace.W;

	// フラスタム外にあるときは、4コーナーがすべて、NDCキューブの6面のうちいずれかの面の外にあるときになっている
	if (BottomLeftNDC.X < -1.0f && BottomRightNDC.X < -1.0f && TopLeftNDC.X < -1.0f && TopRightNDC.X < -1.0f)
	{
		return true;
	}
	if (BottomLeftNDC.X > 1.0f && BottomRightNDC.X > 1.0f && TopLeftNDC.X > 1.0f && TopRightNDC.X > 1.0f)
	{
		return true;
	}
	if (BottomLeftNDC.Y < -1.0f && BottomRightNDC.Y < -1.0f && TopLeftNDC.Y < -1.0f && TopRightNDC.Y < -1.0f)
	{
		return true;
	}
	if (BottomLeftNDC.Y > 1.0f && BottomRightNDC.Y > 1.0f && TopLeftNDC.Y > 1.0f && TopRightNDC.Y > 1.0f)
	{
		return true;
	}
	if (BottomLeftNDC.Z < -1.0f && BottomRightNDC.Z < -1.0f && TopLeftNDC.Z < -1.0f && TopRightNDC.Z < -1.0f)
	{
		return true;
	}
	if (BottomLeftNDC.Z > 1.0f && BottomRightNDC.Z > 1.0f && TopLeftNDC.Z > 1.0f && TopRightNDC.Z > 1.0f)
	{
		return true;
	}

	return false;
}

// QuadNodeのメッシュのグリッドの中でもっともカメラに近いもののスクリーン表示面積率を取得する
float EstimateGridScreenCoverage(int32 NumRowColumn, const FVector& CameraPosition, const FMatrix& ViewProjectionMatrix, const Quadtree::FQuadNode& Node, FIntPoint& OutNearestGrid)
{
	// 表示面積が最大のグリッドを調べたいので、カメラに最も近いグリッドを調べる。
	// グリッドの中には表示されていないものもありうるが、ここに渡されるQuadNodeはフラスタムカリングはされてない前提なので
	// カメラに最も近いグリッドは表示されているはず。

	// QuadNodeメッシュのグリッドへの縦横分割数は同じであり、メッシュは正方形であるという前提がある
	float GridLength = Node.Length / NumRowColumn;
	FVector2D NearestGridBottomLeft;

	// カメラの真下にグリッドがあればそれ。なければClampする
	int32 Row = FMath::Clamp<int32>((CameraPosition.X - Node.BottomLeft.X) / GridLength, 0, NumRowColumn - 1); // floatはint32キャストで切り捨て
	int32 Column = FMath::Clamp<int32>((CameraPosition.Y - Node.BottomLeft.Y) / GridLength, 0, NumRowColumn - 1); // floatはint32キャストで切り捨て
	OutNearestGrid = FIntPoint(Row, Column);
	NearestGridBottomLeft.X = Node.BottomLeft.X + Row * GridLength;
	NearestGridBottomLeft.Y = Node.BottomLeft.Y + Column * GridLength;

	// 最近接グリッドのスクリーンでの表示面積率計算
	const FVector4& NearestGridBottomLeftProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(NearestGridBottomLeft.X, NearestGridBottomLeft.Y, 0.0f, 1.0f));
	const FVector4& NearestGridBottomRightProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(NearestGridBottomLeft.X + GridLength, NearestGridBottomLeft.Y, 0.0f, 1.0f));
	const FVector4& NearestGridTopLeftProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(NearestGridBottomLeft.X, NearestGridBottomLeft.Y + GridLength, 0.0f, 1.0f));
	const FVector4& NearestGridTopRightProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(NearestGridBottomLeft.X + GridLength, NearestGridBottomLeft.Y + GridLength, 0.0f, 1.0f));

	const FVector2D& NearestGridBottomLeftNDC = FVector2D(NearestGridBottomLeftProjSpace) / NearestGridBottomLeftProjSpace.W;
	const FVector2D& NearestGridBottomRightNDC = FVector2D(NearestGridBottomRightProjSpace) / NearestGridBottomRightProjSpace.W;
	const FVector2D& NearestGridTopLeftNDC = FVector2D(NearestGridTopLeftProjSpace) / NearestGridTopLeftProjSpace.W;

	// 面積を求めるにはヘロンの公式を使うのが厳密だが、Sqrtの処理が多く入るので処理負荷のため、おおよそ平行四辺形ととらえて面積を計算する。
	// あとで2乗するので平均をとるときに正負の符号は無視。
	const FVector2D& HorizEdge = NearestGridBottomRightNDC - NearestGridBottomLeftNDC;
	const FVector2D& VertEdge = NearestGridTopLeftNDC - NearestGridBottomLeftNDC;

	// グリッドのインデックスが増える方向はX軸Y軸方向でUE4は左手系なので、カメラがZ軸正にあるという前提なら外積の符号は反転させたものが平行四辺形の面積になる
	// NDCは[-1,1]なのでXY面も面積は4なので、表示面積率としては1/4を乗算したものになる
	float Ret = -(HorizEdge ^ VertEdge) * 0.25f;
	return Ret;
}

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

int32 BuildQuadNodeRenderListRecursively(int32 NumRowColumn, float MaxScreenCoverage, float PatchLength, const FVector& CameraPosition, const FMatrix& ViewProjectionMatrix, FQuadNode& Node, TArray<FQuadNode>& OutRenderList)
{
	bool bCulled = IsQuadNodeFrustumCulled(ViewProjectionMatrix, Node);
	if (bCulled)
	{
		return INDEX_NONE;
	}

	// QuadNodeに許容するスクリーン表示面積率の上限。
	//float MaxScreenCoverage = MaxPixelCoverage / View.SceneViewInitOptions.GetConstrainedViewRect().Size().X / View.SceneViewInitOptions.GetConstrainedViewRect().Size().Y;

	// QuadNodeの全グリッドのスクリーン表示面積率の中で最大のもの。
	FIntPoint NearestGrid;
	float GridCoverage = EstimateGridScreenCoverage(NumRowColumn, CameraPosition, ViewProjectionMatrix, Node, NearestGrid);

	bool bVisible = true;
	if (GridCoverage > MaxScreenCoverage // グリッドが表示面積率上限より大きければ子に分割する。自分を描画しないのはリストの使用側でIsLeafで判定する。
		&& Node.Length > PatchLength) // パッチサイズ以下の縦横長さであればそれ以上分割しない。上の条件だけだとカメラが近いといくらでも小さく分割してしまうので
	{
		// LODとchildListIndicesは初期値通り
		FQuadNode ChildNodeBottomLeft;
		ChildNodeBottomLeft.BottomLeft = Node.BottomLeft;
		ChildNodeBottomLeft.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[0] = BuildQuadNodeRenderListRecursively(NumRowColumn, MaxScreenCoverage, PatchLength, CameraPosition, ViewProjectionMatrix, ChildNodeBottomLeft, OutRenderList);

		FQuadNode ChildNodeBottomRight;
		ChildNodeBottomRight.BottomLeft = Node.BottomLeft + FVector2D(Node.Length / 2.0f, 0.0f);
		ChildNodeBottomRight.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[1] = BuildQuadNodeRenderListRecursively(NumRowColumn, MaxScreenCoverage, PatchLength, CameraPosition, ViewProjectionMatrix, ChildNodeBottomRight, OutRenderList);

		FQuadNode ChildNodeTopLeft;
		ChildNodeTopLeft.BottomLeft = Node.BottomLeft + FVector2D(0.0f, Node.Length / 2.0f);
		ChildNodeTopLeft.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[2] = BuildQuadNodeRenderListRecursively(NumRowColumn, MaxScreenCoverage, PatchLength, CameraPosition, ViewProjectionMatrix, ChildNodeTopLeft, OutRenderList);

		FQuadNode ChildNodeTopRight;
		ChildNodeTopRight.BottomLeft = Node.BottomLeft + FVector2D(Node.Length / 2.0f, Node.Length / 2.0f);
		ChildNodeTopRight.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[3] = BuildQuadNodeRenderListRecursively(NumRowColumn, MaxScreenCoverage, PatchLength, CameraPosition, ViewProjectionMatrix, ChildNodeTopRight, OutRenderList);

		// すべての子ノードがフラスタムカリング対象だったら、自分も不可視なはずで、カリング計算で漏れたとみるべきなのでカリングする
		bVisible = !IsLeaf(Node);
	}

	if (bVisible)
	{
		// TODO:ここの計算も関数化したいな。。。
		// メッシュのLODをスクリーン表示面積率から決める
		// LODレベルが上がるごとに縦横2倍するので、グリッドのスクリーン表示面積率が上限値の1/4^xになっていればxがLODレベルである
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


