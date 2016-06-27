//==============================================================================
// Assimpでのメッシュインポーター
//==============================================================================
#include "MeshImporter.h"

// コンストラクタ
MeshImporter::MeshImporter()
{
}

// デストラクタ
MeshImporter::~MeshImporter()
{
}

// メッシュ読み込み
bool MeshImporter::loadMesh(const std::string& filename, float scale, bool isFlipY)
{
	Assimp::Importer importer;

	// ファイルからシーン作成
	const aiScene* pScene = importer.ReadFile(filename, aiProcess_FlipWindingOrder | aiProcess_Triangulate | aiProcess_PreTransformVertices | aiProcess_GenSmoothNormals);

	if(pScene)
	{
		m_mashDatum.resize(pScene->mNumMeshes);

		// メッシュデータを初期化して行く
		for(unsigned int i = 0; i < m_mashDatum.size(); ++i)
		{
			const aiMesh* pMesh = pScene->mMeshes[i];

			// ディフューズカラー獲得
			aiColor4D diffuseColor(0.f, 0.f, 0.f, 0.0f);
			pScene->mMaterials[pMesh->mMaterialIndex]->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor);

			// テクスチャパスを獲得
			aiString path;
			pScene->mMaterials[pMesh->mMaterialIndex]->GetTexture(aiTextureType_DIFFUSE, 0, &path);
			m_mashDatum[i].textureName = path.C_Str();

			aiVector3D ZERO_3D(0.0f, 0.0f, 0.0f);

			// 頂点情報格納
			for(unsigned int j = 0; j < pMesh->mNumVertices; ++j)
			{
				aiVector3D* pTexCoord;
				if(pMesh->HasTextureCoords(0))
				{
					pTexCoord = &pMesh->mTextureCoords[0][j];
				}
				else
				{
					pTexCoord = &ZERO_3D;
				}

				// TexcoordのV座標が逆転しているので逆転させる
				pTexCoord->y = 1 - pTexCoord->y;

				aiVector3D vertex = pMesh->mVertices[j] * scale;

				// 上下をフリップさせる
				if(isFlipY)
				{
					vertex.y *= -1;
				}

				m_mashDatum[i].vertices.emplace_back(
					vertex,
					aiVector2D(pTexCoord->x, pTexCoord->y),
					pMesh->mNormals[j],
					diffuseColor);
			}

			// 面情報からインデックス情報を獲得
			for(unsigned int j = 0; j < pMesh->mNumFaces; j++)
			{
				const aiFace& face = pMesh->mFaces[j];

				// 三角化しているはずなので3以外はスルー
				if(face.mNumIndices != 3)
				{
					continue;
				}

				m_mashDatum[i].indices.emplace_back(face.mIndices[0]);
				m_mashDatum[i].indices.emplace_back(face.mIndices[1]);
				m_mashDatum[i].indices.emplace_back(face.mIndices[2]);
			}
		}
	}
	else
	{
		return false;
	}

	return true;
}