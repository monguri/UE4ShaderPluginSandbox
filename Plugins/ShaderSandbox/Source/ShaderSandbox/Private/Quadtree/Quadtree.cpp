#include "Quadtree.h"

namespace
{
// フラスタムカリング。ビューフラスタムの中にQuadNodeが一部でも入っていたらtrue。そうでなければfalse。
bool IsQuadNodeFrustumCulled(const FMatrix& ViewProjectionMatrix, const Quadtree::FQuadNode& Node)
{
	//FSceneView::ProjectWorldToScreen()を参考にしている

	const FVector4& BottomRightProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(Node.BottomRight.X, Node.BottomRight.Y, 0.0f, 1.0f));
	const FVector4& BottomLeftProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(Node.BottomRight.X + Node.Length, Node.BottomRight.Y, 0.0f, 1.0f));
	const FVector4& TopRightProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(Node.BottomRight.X, Node.BottomRight.Y + Node.Length, 0.0f, 1.0f));
	const FVector4& TopLeftProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(Node.BottomRight.X + Node.Length, Node.BottomRight.Y + Node.Length, 0.0f, 1.0f));

	// NDCへの変換でWで除算したいのでWが0に近い場合はKINDA_SMALL_NUMBERで割る。その場合は本来より結果が小さい値になるので大抵カリングされない
	const FVector4& BottomRightNDC = BottomRightProjSpace / FMath::Max(FMath::Abs(BottomRightProjSpace.W), KINDA_SMALL_NUMBER);
	const FVector4& BottomLeftNDC = BottomLeftProjSpace / FMath::Max(FMath::Abs(BottomLeftProjSpace.W), KINDA_SMALL_NUMBER);
	const FVector4& TopRightNDC = TopRightProjSpace / FMath::Max(FMath::Abs(TopRightProjSpace.W), KINDA_SMALL_NUMBER);
	const FVector4& TopLeftNDC = TopLeftProjSpace / FMath::Max(FMath::Abs(TopLeftProjSpace.W), KINDA_SMALL_NUMBER);

	// フラスタム外にあるときは、4コーナーがすべて、NDCキューブの6面のうちいずれかの面の外にあるときになっている
	if (BottomRightNDC.X < -1.0f && BottomLeftNDC.X < -1.0f && TopRightNDC.X < -1.0f && TopLeftNDC.X < -1.0f)
	{
		return true;
	}
	if (BottomRightNDC.X > 1.0f && BottomLeftNDC.X > 1.0f && TopRightNDC.X > 1.0f && TopLeftNDC.X > 1.0f)
	{
		return true;
	}
	if (BottomRightNDC.Y < -1.0f && BottomLeftNDC.Y < -1.0f && TopRightNDC.Y < -1.0f && TopLeftNDC.Y < -1.0f)
	{
		return true;
	}
	if (BottomRightNDC.Y > 1.0f && BottomLeftNDC.Y > 1.0f && TopRightNDC.Y > 1.0f && TopLeftNDC.Y > 1.0f)
	{
		return true;
	}
	if (BottomRightNDC.Z < -1.0f && BottomLeftNDC.Z < -1.0f && TopRightNDC.Z < -1.0f && TopLeftNDC.Z < -1.0f)
	{
		return true;
	}
	if (BottomRightNDC.Z > 1.0f && BottomLeftNDC.Z > 1.0f && TopRightNDC.Z > 1.0f && TopLeftNDC.Z > 1.0f)
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
	FVector2D NearestGridBottomRight;

	// カメラの真下にグリッドがあればそれ。なければClampする
	int32 Row = FMath::Clamp<int32>((CameraPosition.X - Node.BottomRight.X) / GridLength, 0, NumRowColumn - 1); // floatはint32キャストで切り捨て
	int32 Column = FMath::Clamp<int32>((CameraPosition.Y - Node.BottomRight.Y) / GridLength, 0, NumRowColumn - 1); // floatはint32キャストで切り捨て
	OutNearestGrid = FIntPoint(Row, Column);
	NearestGridBottomRight.X = Node.BottomRight.X + Row * GridLength;
	NearestGridBottomRight.Y = Node.BottomRight.Y + Column * GridLength;

	// 最近接グリッドのスクリーンでの表示面積率計算
	const FVector4& NearestGridBottomRightProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(NearestGridBottomRight.X, NearestGridBottomRight.Y, 0.0f, 1.0f));
	const FVector4& NearestGridBottomLeftProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(NearestGridBottomRight.X + GridLength, NearestGridBottomRight.Y, 0.0f, 1.0f));
	const FVector4& NearestGridTopRightProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(NearestGridBottomRight.X, NearestGridBottomRight.Y + GridLength, 0.0f, 1.0f));
	const FVector4& NearestGridTopLeftProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(NearestGridBottomRight.X + GridLength, NearestGridBottomRight.Y + GridLength, 0.0f, 1.0f));

	// NDCへの変換でWで除算したいのでWが0に近い場合はKINDA_SMALL_NUMBERで割る。その場合は本来より結果が小さい値になるので、より、葉になりやすくなり表示されやすくなる。
	const FVector2D& NearestGridBottomRightNDC = FVector2D(NearestGridBottomRightProjSpace) / FMath::Max(FMath::Abs(NearestGridBottomRightProjSpace.W), KINDA_SMALL_NUMBER);
	const FVector2D& NearestGridBottomLeftNDC = FVector2D(NearestGridBottomLeftProjSpace) / FMath::Max(FMath::Abs(NearestGridBottomLeftProjSpace.W), KINDA_SMALL_NUMBER);
	const FVector2D& NearestGridTopRightNDC = FVector2D(NearestGridTopRightProjSpace) / FMath::Max(FMath::Abs(NearestGridTopRightProjSpace.W), KINDA_SMALL_NUMBER);

	// 面積を求めるにはヘロンの公式を使うのが厳密だが、Sqrtの処理が多く入るので処理負荷のため、おおよそ平行四辺形ととらえて面積を計算する。
	// あとで2乗するので平均をとるときに正負の符号は無視。
	const FVector2D& HorizEdge = NearestGridBottomLeftNDC - NearestGridBottomRightNDC;
	const FVector2D& VertEdge = NearestGridTopRightNDC - NearestGridBottomRightNDC;

	// グリッドのインデックスが増える方向はX軸Y軸方向でUE4は左手系なので、カメラがZ軸正にあるという前提なら外積の符号は反転させたものが平行四辺形の面積になる
	// NDCは[-1,1]なのでXY面も面積は4なので、表示面積率としては1/4を乗算したものになる
	// ただし、カメラと近く、メッシュの目が粗いものほど、平行四辺形近似は粗くなる
	// コーナーがカメラの後ろにまわる場合などは符号が一定には扱えなくなるので、それでも同じ式で扱うために絶対値で扱う
	float Ret = FMath::Abs(HorizEdge ^ VertEdge) * 0.25f;
	return Ret;
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

int32 BuildQuadNodeRenderListRecursively(int32 MaxLOD, int32 NumRowColumn, float MaxScreenCoverage, float PatchLength, const FVector& CameraPosition, const FMatrix& ViewProjectionMatrix, FQuadNode& Node, TArray<FQuadNode>& OutRenderList)
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
		FQuadNode ChildNodeBottomRight;
		ChildNodeBottomRight.BottomRight = Node.BottomRight;
		ChildNodeBottomRight.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[0] = BuildQuadNodeRenderListRecursively(MaxLOD, NumRowColumn, MaxScreenCoverage, PatchLength, CameraPosition, ViewProjectionMatrix, ChildNodeBottomRight, OutRenderList);

		FQuadNode ChildNodeBottomLeft;
		ChildNodeBottomLeft.BottomRight = Node.BottomRight + FVector2D(Node.Length / 2.0f, 0.0f);
		ChildNodeBottomLeft.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[1] = BuildQuadNodeRenderListRecursively(MaxLOD, NumRowColumn, MaxScreenCoverage, PatchLength, CameraPosition, ViewProjectionMatrix, ChildNodeBottomLeft, OutRenderList);

		FQuadNode ChildNodeTopRight;
		ChildNodeTopRight.BottomRight = Node.BottomRight + FVector2D(0.0f, Node.Length / 2.0f);
		ChildNodeTopRight.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[2] = BuildQuadNodeRenderListRecursively(MaxLOD, NumRowColumn, MaxScreenCoverage, PatchLength, CameraPosition, ViewProjectionMatrix, ChildNodeTopRight, OutRenderList);

		FQuadNode ChildNodeTopLeft;
		ChildNodeTopLeft.BottomRight = Node.BottomRight + FVector2D(Node.Length / 2.0f, Node.Length / 2.0f);
		ChildNodeTopLeft.Length = Node.Length / 2.0f;
		Node.ChildNodeIndices[3] = BuildQuadNodeRenderListRecursively(MaxLOD, NumRowColumn, MaxScreenCoverage, PatchLength, CameraPosition, ViewProjectionMatrix, ChildNodeTopLeft, OutRenderList);

		// すべての子ノードがフラスタムカリング対象だったら、自分も不可視なはずで、カリング計算で漏れたとみるべきなのでカリングする
		bVisible = !IsLeaf(Node);
	}

	if (bVisible)
	{
		// TODO:ここの計算も関数化したいな。。。
		// メッシュのLODをスクリーン表示面積率から決める
		// LODレベルが上がるごとに縦横2倍するので、グリッドのスクリーン表示面積率が上限値の1/4^xになっていればxがLODレベルである
		int32 LOD = 0;
		for (; LOD < MaxLOD - 1; LOD++)  //TODO:最大LODレベル8がマジックナンバー
		{
			if (GridCoverage > MaxScreenCoverage)
			{
				break;
			}

			GridCoverage *= 4;
		}

		// TODO:LODの最大レベルと、それよりひとつ下のレベルは使わない。あとで考え直す
		Node.LOD = FMath::Max(FMath::Min(LOD, MaxLOD - 2), 0);
	}
	else
	{
		return INDEX_NONE;
	}

	int32 Index = OutRenderList.Add(Node);
	return Index;
}
} // namespace Quadtree


