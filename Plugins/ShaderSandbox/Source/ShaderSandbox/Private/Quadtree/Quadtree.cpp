#include "Quadtree/Quadtree.h"

namespace
{
using namespace Quadtree;

// フラスタムカリング。ビューフラスタムの中にQuadNodeが一部でも入っていたらtrue。そうでなければfalse。
bool IsQuadNodeFrustumCulled(const FMatrix& ViewProjectionMatrix, const FQuadNode& Node)
{
	//FSceneView::ProjectWorldToScreen()を参考にしている

	const FVector4& BottomRightProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(Node.BottomRight.X, Node.BottomRight.Y, Node.BottomRight.Z, 1.0f));
	const FVector4& BottomLeftProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(Node.BottomRight.X + Node.Length, Node.BottomRight.Y, Node.BottomRight.Z, 1.0f));
	const FVector4& TopRightProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(Node.BottomRight.X, Node.BottomRight.Y + Node.Length, Node.BottomRight.Z, 1.0f));
	const FVector4& TopLeftProjSpace = ViewProjectionMatrix.TransformFVector4(FVector4(Node.BottomRight.X + Node.Length, Node.BottomRight.Y + Node.Length, Node.BottomRight.Z, 1.0f));

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
	// ZはUE4のNDCでは[0,1]である。
	if (BottomRightNDC.Z < 0.0f && BottomLeftNDC.Z < 0.0f && TopRightNDC.Z < 0.0f && TopLeftNDC.Z < 0.0f)
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
float EstimateGridScreenCoverage(int32 NumRowColumn, const FVector& CameraPosition, const FVector2D& ProjectionScale, const FMatrix& ViewProjectionMatrix, const FQuadNode& Node, FIntPoint& OutNearestGrid)
{
	// 表示面積が最大のグリッドを調べたいので、カメラに最も近いグリッドを調べる。
	// グリッドの中には表示されていないものもありうるが、ここに渡されるQuadNodeはフラスタムカリングはされてない前提なので
	// カメラに最も近いグリッドは表示されているはず。

	// QuadNodeメッシュのグリッドへの縦横分割数は同じであり、メッシュは正方形であるという前提がある
	float GridLength = Node.Length / NumRowColumn;
	FVector NearestGridBottomRight;

	// カメラの真下にグリッドがあればそれ。なければClampする
	int32 Row = FMath::Clamp<int32>((CameraPosition.X - Node.BottomRight.X) / GridLength, 0, NumRowColumn - 1); // floatはint32キャストで切り捨て
	int32 Column = FMath::Clamp<int32>((CameraPosition.Y - Node.BottomRight.Y) / GridLength, 0, NumRowColumn - 1); // floatはint32キャストで切り捨て
	OutNearestGrid = FIntPoint(Row, Column);
	NearestGridBottomRight.X = Node.BottomRight.X + Row * GridLength;
	NearestGridBottomRight.Y = Node.BottomRight.Y + Column * GridLength;
	NearestGridBottomRight.Z = Node.BottomRight.Z;

	// グリッドにカメラは正対していないが、正対してると想定したときのスクリーン表示面積を返す。
	// 実際の正対してないスクリーン表示面積を使うと、かなり矩形がゆがむために、ゆがみが激しいときだとちょっとしたカメラの動きで面積が大きく変わり安定しなくなる。
	// NDCは[-1,1]なのでXY面も面積は4なので、表示面積率としては1/4を乗算したものになる
	float CameraDistanceSquare = (CameraPosition - NearestGridBottomRight).SizeSquared();
	float Ret = GridLength * ProjectionScale.X * GridLength * ProjectionScale.Y * 0.25f / CameraDistanceSquare;
	return Ret;
}

int32 BuildQuadtreeRecursively(int32 MaxLOD, int32 NumRowColumn, float MaxScreenCoverage, float PatchLength, const FVector& CameraPosition, const FVector2D& ProjectionScale, const FMatrix& ViewProjectionMatrix, FQuadNode& Node, TArray<FQuadNode>& OutQuadNodeList)
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
	float GridCoverage = EstimateGridScreenCoverage(NumRowColumn, CameraPosition, ProjectionScale, ViewProjectionMatrix, Node, NearestGrid);

	if (GridCoverage > MaxScreenCoverage // グリッドが表示面積率上限より大きければ子に分割する。自分を描画しないのはリストの使用側でIsLeafで判定する。
		&& Node.Length > PatchLength // パッチサイズ以下の縦横長さであればそれ以上分割しない。上の条件だけだとカメラが近いといくらでも小さく分割してしまうので
		&& Node.LOD > 0) // LOD0のものはそれ以上分割しない
	{
		// LODとchildListIndicesは初期値通り
		FQuadNode ChildNodeBottomRight;
		ChildNodeBottomRight.BottomRight = Node.BottomRight;
		ChildNodeBottomRight.Length = Node.Length * 0.5f;
		ChildNodeBottomRight.LOD = Node.LOD - 1;
		Node.ChildNodeIndices[0] = BuildQuadtreeRecursively(MaxLOD, NumRowColumn, MaxScreenCoverage, PatchLength, CameraPosition, ProjectionScale, ViewProjectionMatrix, ChildNodeBottomRight, OutQuadNodeList);

		FQuadNode ChildNodeBottomLeft;
		ChildNodeBottomLeft.BottomRight = Node.BottomRight + FVector(Node.Length * 0.5f, 0.0f, 0.0f);
		ChildNodeBottomLeft.Length = Node.Length * 0.5f;
		ChildNodeBottomLeft.LOD = Node.LOD - 1;
		Node.ChildNodeIndices[1] = BuildQuadtreeRecursively(MaxLOD, NumRowColumn, MaxScreenCoverage, PatchLength, CameraPosition, ProjectionScale, ViewProjectionMatrix, ChildNodeBottomLeft, OutQuadNodeList);

		FQuadNode ChildNodeTopRight;
		ChildNodeTopRight.BottomRight = Node.BottomRight + FVector(0.0f, Node.Length * 0.5f, 0.0f);
		ChildNodeTopRight.Length = Node.Length * 0.5f;
		ChildNodeTopRight.LOD = Node.LOD - 1;
		Node.ChildNodeIndices[2] = BuildQuadtreeRecursively(MaxLOD, NumRowColumn, MaxScreenCoverage, PatchLength, CameraPosition, ProjectionScale, ViewProjectionMatrix, ChildNodeTopRight, OutQuadNodeList);

		FQuadNode ChildNodeTopLeft;
		ChildNodeTopLeft.BottomRight = Node.BottomRight + FVector(Node.Length * 0.5f, Node.Length * 0.5f, 0.0f);
		ChildNodeTopLeft.Length = Node.Length * 0.5f;
		ChildNodeTopLeft.LOD = Node.LOD - 1;
		Node.ChildNodeIndices[3] = BuildQuadtreeRecursively(MaxLOD, NumRowColumn, MaxScreenCoverage, PatchLength, CameraPosition, ProjectionScale, ViewProjectionMatrix, ChildNodeTopLeft, OutQuadNodeList);

		// すべての子ノードがフラスタムカリング対象だったら、自分も不可視なはずで、カリング計算で漏れたとみるべきなのでカリングする
		if (Node.IsLeaf())
		{
			return INDEX_NONE;
		}
	}

	int32 Index = OutQuadNodeList.Add(Node);
	return Index;
}

int32 SearchLeafContainsPosition2D(const TArray<FQuadNode>& RenderQuadNodeList, const FVector2D& Position2D)
{
	// AllQuadNodeListから木をたどって探してもいいが、ロジックが複雑になるし、現実的にそんなに処理数に大した差はでないのでRenderQuadNodeListからループで探す
	for (int32 i = 0; i < RenderQuadNodeList.Num(); i++)
	{
		if (RenderQuadNodeList[i].ContainsPosition2D(Position2D))
		{
			return i;
		}
	}

	return INDEX_NONE;
}


int32 GetGridMeshIndex(int32 Row, int32 Column, int32 NumColumn)
{
	return Row * (NumColumn + 1) + Column;
}

uint32 CreateInnerMesh(int32 NumRowColumn, TArray<uint32>& OutIndices)
{
	check(NumRowColumn % 2 == 0);

	// 内側の部分はどのメッシュパターンでも同じだが、すべて同じものを作成する。ドローコールでインデックスバッファの不連続アクセスはできないので
	for (int32 Row = 1; Row < NumRowColumn - 1; Row++)
	{
		for (int32 Column = 1; Column < NumRowColumn - 1; Column++)
		{
			// 4隅のグリッドをトライアングル2つに分割する対角線は、メッシュ全体の対角線の方向になっている方が、
			// もし4隅の部分で一方の辺がLOD差がなく一方の辺がLOD差がある場合に、境界のジオメトリを作るのが扱いやすい。
			// よってインデックスが偶数のグリッドと奇数のグリッドで対角線を逆にする。
			// TRIANGLE_STRIPと同じ。
			// NumRowColumnが偶数であることを前提にしている

			if ((Row + Column) % 2 == 0)
			{
				OutIndices.Emplace(GetGridMeshIndex(Row, Column, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(Row + 1, Column + 1, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(Row, Column + 1, NumRowColumn));

				OutIndices.Emplace(GetGridMeshIndex(Row, Column, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(Row + 1, Column, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(Row + 1, Column + 1, NumRowColumn));
			}
			else
			{
				OutIndices.Emplace(GetGridMeshIndex(Row, Column, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(Row + 1, Column, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(Row, Column + 1, NumRowColumn));

				OutIndices.Emplace(GetGridMeshIndex(Row + 1, Column, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(Row + 1, Column + 1, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(Row, Column + 1, NumRowColumn));
			}
		}
	}

	return 6 * (NumRowColumn - 2) * (NumRowColumn - 2);
}

uint32 CreateBoundaryMesh(EAdjacentQuadNodeLODDifference RightAdjLODDiff, EAdjacentQuadNodeLODDifference LeftAdjLODDiff, EAdjacentQuadNodeLODDifference BottomAdjLODDiff, EAdjacentQuadNodeLODDifference TopAdjLODDiff, int32 NumRowColumn, TArray<uint32>& OutIndices, TArray<Quadtree::FQuadMeshParameter>& OutQuadMeshParams)
{
	check(NumRowColumn % 2 == 0);
	uint32 NumIndices = 0;

	// 境界部分は、LODの差に合わせて隣と接する部分のトライアングルの辺を2倍あるいは4倍にする
	// 隣と接する辺の長いトライアングル単位でインデックスを追加していくため、LODの差が1ならグリッドをたどるループのステップを2、2以上なら4単位で行う

	// Right
	{
		if (RightAdjLODDiff == EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST)
		{
			for (int32 Row = 0; Row < NumRowColumn; Row++)
			{
				if (Row % 2 == 0)
				{
					if (Row > 0) // Bottomと重複しないように
					{
						OutIndices.Emplace(GetGridMeshIndex(Row, 0, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(Row + 1, 1, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(Row, 1, NumRowColumn));
						NumIndices += 3;
					}

					OutIndices.Emplace(GetGridMeshIndex(Row, 0, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row + 1, 0, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row + 1, 1, NumRowColumn));
					NumIndices += 3;
				}
				else
				{
					OutIndices.Emplace(GetGridMeshIndex(Row, 0, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row + 1, 0, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row, 1, NumRowColumn));
					NumIndices += 3;

					if (Row < NumRowColumn) // Topと重複しないように
					{
						OutIndices.Emplace(GetGridMeshIndex(Row + 1, 0, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(Row + 1, 1, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(Row, 1, NumRowColumn));
						NumIndices += 3;
					}
				}
			}
		}
		else
		{
			int32 Step = 1 << (int32)RightAdjLODDiff;

			for (int32 Row = 0; Row < NumRowColumn; Row += Step)
			{
				// 接する部分の辺の長いトライアングル
				OutIndices.Emplace(GetGridMeshIndex(Row, 0, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(Row + Step, 0, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(Row + (Step >> 1), 1, NumRowColumn));
				NumIndices += 3;

				// 辺の長いトライアングルの山の下側を埋めるトライアングル
				for (int32 i = 0; i < (Step >> 1); i++)
				{
					if (Row == 0 && i == 0)
					{
						// Bottomと重複しないように4隅のトライアングルの分は作らない
						continue;
					}

					OutIndices.Emplace(GetGridMeshIndex(Row, 0, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row + i + 1, 1, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row + i, 1, NumRowColumn));
					NumIndices += 3;
				}

				// 辺の長いトライアングルの山の上側を埋めるトライアングル
				for (int32 i = (Step >> 1); i < Step; i++)
				{
					if (Row == (NumRowColumn - Step) && i == (Step - 1))
					{
						// Topと重複しないように4隅のトライアングルの分は作らない
						continue;
					}

					OutIndices.Emplace(GetGridMeshIndex(Row + Step, 0, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row + i + 1, 1, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row + i, 1, NumRowColumn));
					NumIndices += 3;
				}
			}
		}
	}

	// Left
	{
		if (LeftAdjLODDiff == EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST)
		{
			for (int32 Row = 0; Row < NumRowColumn; Row++)
			{
				if ((Row + NumRowColumn - 1) % 2 == 0)
				{
					OutIndices.Emplace(GetGridMeshIndex(Row, NumRowColumn - 1, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row + 1, NumRowColumn, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row, NumRowColumn, NumRowColumn));
					NumIndices += 3;

					if (Row < (NumRowColumn - 1)) // Topと重複しないように
					{
						OutIndices.Emplace(GetGridMeshIndex(Row, NumRowColumn - 1, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(Row + 1, NumRowColumn - 1, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(Row + 1, NumRowColumn, NumRowColumn));
						NumIndices += 3;
					}
				}
				else
				{
					if (Row > 0) // Bottomと重複しないように
					{
						OutIndices.Emplace(GetGridMeshIndex(Row, NumRowColumn - 1, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(Row + 1, NumRowColumn - 1, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(Row, NumRowColumn, NumRowColumn));
						NumIndices += 3;
					}

					OutIndices.Emplace(GetGridMeshIndex(Row + 1, NumRowColumn - 1, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row + 1, NumRowColumn, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row, NumRowColumn, NumRowColumn));
					NumIndices += 3;
				}
			}
		}
		else
		{
			int32 Step = 1 << (int32)LeftAdjLODDiff;

			for (int32 Row = 0; Row < NumRowColumn; Row += Step)
			{
				// 接する部分の辺の長いトライアングル
				OutIndices.Emplace(GetGridMeshIndex(Row, NumRowColumn, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(Row + (Step >> 1), NumRowColumn - 1, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(Row + Step, NumRowColumn, NumRowColumn));
				NumIndices += 3;

				// 辺の長いトライアングルの山の下側を埋めるトライアングル
				for (int32 i = 0; i < (Step >> 1); i++)
				{
					if (Row == 0 && i == 0)
					{
						// Bottomと重複しないように4隅のトライアングルの分は作らない
						continue;
					}

					OutIndices.Emplace(GetGridMeshIndex(Row, NumRowColumn, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row + i, NumRowColumn - 1, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row + i + 1, NumRowColumn - 1, NumRowColumn));
					NumIndices += 3;
				}

				// 辺の長いトライアングルの山の上側を埋めるトライアングル
				for (int32 i = (Step >> 1); i < Step; i++)
				{
					if (Row == (NumRowColumn - Step) && i == (Step - 1))
					{
						// Topと重複しないように4隅のトライアングルの分は作らない
						continue;
					}

					OutIndices.Emplace(GetGridMeshIndex(Row + Step, NumRowColumn, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row + i, NumRowColumn - 1, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(Row + i + 1, NumRowColumn - 1, NumRowColumn));
					NumIndices += 3;
				}
			}
		}
	}

	// Bottom
	{
		if (BottomAdjLODDiff == EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST)
		{
			for (int32 Column = 0; Column < NumRowColumn; Column++)
			{
				if (Column % 2 == 0)
				{
					OutIndices.Emplace(GetGridMeshIndex(0, Column, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(1, Column + 1, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(0, Column + 1, NumRowColumn));
					NumIndices += 3;

					if (Column > 0) // Rightと重複しないように
					{
						OutIndices.Emplace(GetGridMeshIndex(0, Column, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(1, Column, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(1, Column + 1, NumRowColumn));
						NumIndices += 3;
					}
				}
				else
				{
					OutIndices.Emplace(GetGridMeshIndex(0, Column, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(1, Column, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(0, Column + 1, NumRowColumn));
					NumIndices += 3;

					if (Column < NumRowColumn - 1) // Leftと重複しないように
					{
						OutIndices.Emplace(GetGridMeshIndex(1, Column, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(1, Column + 1, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(0, Column + 1, NumRowColumn));
						NumIndices += 3;
					}
				}
			}
		}
		else
		{
			int32 Step = 1 << (int32)BottomAdjLODDiff;

			for (int32 Column = 0; Column < NumRowColumn; Column += Step)
			{
				// 接する部分の辺の長いトライアングル
				OutIndices.Emplace(GetGridMeshIndex(0, Column, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(1, Column + (Step >> 1), NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(0, Column + Step, NumRowColumn));
				NumIndices += 3;

				// 辺の長いトライアングルの山の右側を埋めるトライアングル
				for (int32 i = 0; i < (Step >> 1); i++)
				{
					if (Column == 0 && i == 0)
					{
						// Bottomと重複しないように4隅のトライアングルの分は作らない
						continue;
					}

					OutIndices.Emplace(GetGridMeshIndex(0, Column, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(1, Column + i, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(1, Column + i + 1, NumRowColumn));
					NumIndices += 3;
				}

				// 辺の長いトライアングルの山の左側を埋めるトライアングル
				for (int32 i = (Step >> 1); i < Step; i++)
				{
					if (Column == (NumRowColumn - Step) && i == (Step - 1))
					{
						// Topと重複しないように4隅のトライアングルの分は作らない
						continue;
					}

					OutIndices.Emplace(GetGridMeshIndex(0, Column + Step, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(1, Column + i, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(1, Column + i + 1, NumRowColumn));
					NumIndices += 3;
				}
			}
		}
	}

	// Top
	{
		if (TopAdjLODDiff == EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST)
		{
			for (int32 Column = 0; Column < NumRowColumn; Column++)
			{
				// 4隅のグリッドをトライアングル2つに分割する対角線は、メッシュ全体の対角線の方向になっている方が、
				// もし4隅の部分で一方の辺がLOD差がなく一方の辺がLOD差がある場合に、境界のジオメトリを作るのが扱いやすい。
				// よってインデックスが偶数のグリッドと奇数のグリッドで対角線を逆にする。
				// TRIANGLE_STRIPと同じ。
				// NumRowColumnが偶数であることを前提にしている

				if ((NumRowColumn - 1 + Column) % 2 == 0)
				{
					if (Column < NumRowColumn - 1) // Leftと重複しないように
					{
						OutIndices.Emplace(GetGridMeshIndex(NumRowColumn - 1, Column, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(NumRowColumn, Column + 1, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(NumRowColumn - 1, Column + 1, NumRowColumn));
						NumIndices += 3;
					}

					OutIndices.Emplace(GetGridMeshIndex(NumRowColumn - 1, Column, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(NumRowColumn, Column, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(NumRowColumn, Column + 1, NumRowColumn));
					NumIndices += 3;
				}
				else
				{
					if (Column > 0) // Rightと重複しないように
					{
						OutIndices.Emplace(GetGridMeshIndex(NumRowColumn - 1, Column, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(NumRowColumn, Column, NumRowColumn));
						OutIndices.Emplace(GetGridMeshIndex(NumRowColumn - 1, Column + 1, NumRowColumn));
						NumIndices += 3;
					}

					OutIndices.Emplace(GetGridMeshIndex(NumRowColumn, Column, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(NumRowColumn, Column + 1, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(NumRowColumn - 1, Column + 1, NumRowColumn));
					NumIndices += 3;
				}
			}
		}
		else
		{
			int32 Step = 1 << (int32)TopAdjLODDiff;

			for (int32 Column = 0; Column < NumRowColumn; Column += Step)
			{
				// 接する部分の辺の長いトライアングル
				OutIndices.Emplace(GetGridMeshIndex(NumRowColumn, Column, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(NumRowColumn, Column + Step, NumRowColumn));
				OutIndices.Emplace(GetGridMeshIndex(NumRowColumn - 1, Column + (Step >> 1), NumRowColumn));
				NumIndices += 3;

				// 辺の長いトライアングルの山の右側を埋めるトライアングル
				for (int32 i = 0; i < (Step >> 1); i++)
				{
					if (Column == 0 && i == 0)
					{
						// Bottomと重複しないように4隅のトライアングルの分は作らない
						continue;
					}

					OutIndices.Emplace(GetGridMeshIndex(NumRowColumn, Column, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(NumRowColumn - 1, Column + i + 1, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(NumRowColumn - 1, Column + i, NumRowColumn));
					NumIndices += 3;
				}

				// 辺の長いトライアングルの山の左側を埋めるトライアングル
				for (int32 i = (Step >> 1); i < Step; i++)
				{
					if (Column == (NumRowColumn - Step) && i == (Step - 1))
					{
						// Topと重複しないように4隅のトライアングルの分は作らない
						continue;
					}

					OutIndices.Emplace(GetGridMeshIndex(NumRowColumn, Column + Step, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(NumRowColumn - 1, Column + i + 1, NumRowColumn));
					OutIndices.Emplace(GetGridMeshIndex(NumRowColumn - 1, Column + i, NumRowColumn));
					NumIndices += 3;
				}
			}
		}
	}

	return NumIndices;
}
} // namespace

namespace Quadtree
{
bool FQuadNode::IsLeaf() const
{
	return (ChildNodeIndices[0] == INDEX_NONE) && (ChildNodeIndices[1] == INDEX_NONE) && (ChildNodeIndices[2] == INDEX_NONE) && (ChildNodeIndices[3] == INDEX_NONE);
}

bool FQuadNode::ContainsPosition2D(const FVector2D& Position2D) const
{
	return (BottomRight.X <= Position2D.X && Position2D.X <= (BottomRight.X + Length) && BottomRight.Y <= Position2D.Y && Position2D.Y <= (BottomRight.Y + Length));
}

void BuildQuadtree(int32 MaxLOD, int32 NumRowColumn, float MaxScreenCoverage, float PatchLength, const FVector& CameraPosition, const FVector2D& ProjectionScale, const FMatrix& ViewProjectionMatrix, FQuadNode& RootNode, TArray<FQuadNode>& OutAllQuadNodeList, TArray<FQuadNode>& OutRenderQuadNodeList)
{
	BuildQuadtreeRecursively(MaxLOD, NumRowColumn, MaxScreenCoverage, PatchLength, CameraPosition, ProjectionScale, ViewProjectionMatrix, RootNode, OutAllQuadNodeList);

	for (const FQuadNode& Node : OutAllQuadNodeList)
	{
		if (Node.IsLeaf())
		{
			OutRenderQuadNodeList.Add(Node);
		}
	}
}

EAdjacentQuadNodeLODDifference QueryAdjacentNodeType(const FQuadNode& Node, const FVector2D& AdjacentPosition, const TArray<FQuadNode>& RenderQuadNodeList)
{
	int32 AdjNodeIndex = SearchLeafContainsPosition2D(RenderQuadNodeList, AdjacentPosition);

	EAdjacentQuadNodeLODDifference Ret = EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST;
	if (AdjNodeIndex != INDEX_NONE)
	{
		int32 AdjNodeLOD = RenderQuadNodeList[AdjNodeIndex].LOD;
		if (Node.LOD + 2 <= AdjNodeLOD)
		{
			Ret = EAdjacentQuadNodeLODDifference::GREATER_BY_MORE_THAN_2;
		}
		else if (Node.LOD + 1 <= AdjNodeLOD)
		{
			Ret = EAdjacentQuadNodeLODDifference::GREATER_BY_1;
		}
	}

	return Ret;
}

void CreateQuadMeshes(int32 NumRowColumn, TArray<uint32>& OutIndices, TArray<Quadtree::FQuadMeshParameter>& OutQuadMeshParams)
{
	check((uint32)EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST == 0);
	check((uint32)EAdjacentQuadNodeLODDifference::MAX == 3);

	// グリッドをすべて2個のトライアングルで分割した場合のメッシュを81パターン分で確保しておく。正確には境界部分をより大きなトライアングルで
	// 分割するパターンを含むのでもっと少ないが、式が複雑になるので大きめに確保しておく
	OutIndices.Reset(81 * 2 * 3 * NumRowColumn * NumRowColumn);

	// 右隣がLODが自分以下なのと、一段階上なのと、二段階以上なのの3パターン。左隣、下隣、上隣でも3パターンでそれらの組み合わせで3*3*3*3パターン。
	OutQuadMeshParams.Reset(81);

	uint32 IndexOffset = 0;
	for (uint32 RightType = (uint32)EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST; RightType < (uint32)EAdjacentQuadNodeLODDifference::MAX; RightType++)
	{
		for (uint32 LeftType = (uint32)EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST; LeftType < (uint32)EAdjacentQuadNodeLODDifference::MAX; LeftType++)
		{
			for (uint32 BottomType = (uint32)EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST; BottomType < (uint32)EAdjacentQuadNodeLODDifference::MAX; BottomType++)
			{
				for (uint32 TopType = (uint32)EAdjacentQuadNodeLODDifference::LESS_OR_EQUAL_OR_NOT_EXIST; TopType < (uint32)EAdjacentQuadNodeLODDifference::MAX; TopType++)
				{
					uint32 NumInnerMeshIndices = CreateInnerMesh(NumRowColumn, OutIndices);
					uint32 NumBoundaryMeshIndices = CreateBoundaryMesh((EAdjacentQuadNodeLODDifference)RightType, (EAdjacentQuadNodeLODDifference)LeftType, (EAdjacentQuadNodeLODDifference)BottomType, (EAdjacentQuadNodeLODDifference)TopType, NumRowColumn, OutIndices, OutQuadMeshParams);
					// TArrayのインデックスは、RightType * 3^3 + LeftType * 3^2 + BottomType * 3^1 + TopType * 3^0となる
					OutQuadMeshParams.Emplace(IndexOffset, NumInnerMeshIndices + NumBoundaryMeshIndices);
					IndexOffset += NumInnerMeshIndices + NumBoundaryMeshIndices;
				}
			}
		}
	}
}
} // namespace Quadtree

