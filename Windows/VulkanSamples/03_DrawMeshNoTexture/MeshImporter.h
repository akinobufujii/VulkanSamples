//==============================================================================
// Assimp�ł̃��b�V���C���|�[�^�[
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
	// ��`
	//==========================================================================
	// ���b�V���f�[�^
	struct MeshData
	{
		// ���_�t�H�[�}�b�g
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

		std::vector<VertexFormat>	vertices;		// ���_�z��
		std::vector<uint32_t>		indices;		// �C���f�b�N�X�z��
		std::string					textureName;	// �e�N�X�`����
	};

	//==========================================================================
	// ���\�b�h
	//==========================================================================
	// �R���X�g���N�^
	MeshImporter();

	// �f�X�g���N�^
	~MeshImporter();

	// ���b�V���ǂݍ���
	bool loadMesh(const std::string& filename, float scale = 1.0f, bool isFlipY = false );

	// ���b�V�����l��
	const std::vector<MeshData>& getMeshDatum() const
	{
		return m_mashDatum;
	}

private:
	//==========================================================================
	// �t�B�[���h
	//==========================================================================
	std::vector<MeshData> m_mashDatum;	// ���b�V���f�[�^�W����
};

