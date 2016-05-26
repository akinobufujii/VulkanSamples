//==============================================================================
// Assimp�ł̃��b�V���C���|�[�^�[
//==============================================================================
#include "MeshImporter.h"

// �R���X�g���N�^
MeshImporter::MeshImporter()
{
}

// �f�X�g���N�^
MeshImporter::~MeshImporter()
{
}

// ���b�V���ǂݍ���
bool MeshImporter::loadMesh(const std::string& filename, float scale, bool isFlipY)
{
	Assimp::Importer importer;

	// �t�@�C������V�[���쐬
	const aiScene* pScene = importer.ReadFile(filename, aiProcess_FlipWindingOrder | aiProcess_Triangulate | aiProcess_PreTransformVertices | aiProcess_GenSmoothNormals);

	if(pScene)
	{
		m_mashDatum.resize(pScene->mNumMeshes);

		// ���b�V���f�[�^�����������čs��
		for(unsigned int i = 0; i < m_mashDatum.size(); ++i)
		{
			const aiMesh* pMesh = pScene->mMeshes[i];

			// �f�B�t���[�Y�J���[�l��
			aiColor4D diffuseColor(0.f, 0.f, 0.f, 0.0f);
			pScene->mMaterials[pMesh->mMaterialIndex]->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor);

			// �e�N�X�`���p�X���l��
			aiString path;
			pScene->mMaterials[pMesh->mMaterialIndex]->GetTexture(aiTextureType_DIFFUSE, 0, &path);
			m_mashDatum[i].textureName = path.C_Str();

			aiVector3D ZERO_3D(0.0f, 0.0f, 0.0f);

			// ���_���i�[
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

				// Texcoord��V���W���t�]���Ă���̂ŋt�]������
				pTexCoord->y = 1 - pTexCoord->y;

				aiVector3D vertex = pMesh->mVertices[j] * scale;

				// �㉺���t���b�v������
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

			// �ʏ�񂩂�C���f�b�N�X�����l��
			for(unsigned int j = 0; j < pMesh->mNumFaces; j++)
			{
				const aiFace& face = pMesh->mFaces[j];

				// �O�p�����Ă���͂��Ȃ̂�3�ȊO�̓X���[
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