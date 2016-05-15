//==============================================================================
// Assimpでのメッシュインポーター
//==============================================================================
#include <vector>
#include <string>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/vector2.h>
#include <assimp/vector3.h>

#pragma once
class MeshImporter
{
public:
	//==========================================================================
	// 定義
	//==========================================================================
	// メッシュデータ
	struct MeshData
	{
		// 頂点フォーマット
		struct VertexFormat
		{
			aiVector3D	m_pos;
			aiVector2D	m_uv;
			aiVector3D	m_normal;
			aiColor4D	m_color;

			VertexFormat(const aiVector3D& pos, const aiVector2D& uv, const aiVector3D& normal, const aiColor4D& color)
				: m_pos(pos)
				, m_uv(uv)
				, m_normal(normal)
				, m_color(color)
			{
			}
		};

		std::vector<VertexFormat>	vertices;		// 頂点配列
		std::vector<uint32_t>		indices;		// インデックス配列
		std::string					textureName;	// テクスチャ名
	};

	//==========================================================================
	// メソッド
	//==========================================================================
	// コンストラクタ
	MeshImporter();

	// デストラクタ
	~MeshImporter();

	// メッシュ読み込み
	bool loadMesh(const std::string& filename, float scale = 1.0f, bool isFlipY = false );

	// メッシュ情報獲得
	const std::vector<MeshData>& getMeshDatum() const
	{
		return m_mashDatum;
	}

private:
	//==========================================================================
	// フィールド
	//==========================================================================
	std::vector<MeshData> m_mashDatum;	// メッシュデータ集合体
};

