//==============================================================================
// FPS�J�E���^�[
//==============================================================================
#pragma once

#include <chrono>

class FPSCounter
{
public:
	//==========================================================================
	// ���\�b�h
	//==========================================================================
	// �R���X�g���N�^
	FPSCounter()
		: m_frameCounter(0)
		, m_lastFPS(0)
		, m_fpsTimer(0.f)
		, m_startTime()
	{
	}

	// �f�X�g���N�^
	~FPSCounter(){}

	// FPS�J�E���g�J�n
	void beginCount()
	{
		m_startTime = std::chrono::high_resolution_clock::now();
	}

	// FPS�J�E���g�I��
	void endCount()
	{
		m_fpsTimer += std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - m_startTime).count();
		m_frameCounter++;
		if(m_fpsTimer > std::milli::den)
		{
			m_lastFPS = m_frameCounter;
			m_fpsTimer = 0.0f;
			m_frameCounter = 0.0f;
		}
	}

	// �Ō�ɋL�^����FPS���l��
	unsigned int getLastFPS() const
	{
		return m_lastFPS;
	}

private:
	//==========================================================================
	// �t�B�[���h
	//==========================================================================
	unsigned int							m_frameCounter;	// �t���[���J�E���^
	unsigned int							m_lastFPS;		// �Ō�ɋL�^����FPS
	float									m_fpsTimer;		// FPS�v�Z�^�C�}�[
	std::chrono::steady_clock::time_point	m_startTime;	// �J�n���ԋL�^�p
};