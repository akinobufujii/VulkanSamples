//==============================================================================
// FPSカウンター
//==============================================================================
#pragma once

#include <chrono>

class FPSCounter
{
public:
	//==========================================================================
	// メソッド
	//==========================================================================
	// コンストラクタ
	FPSCounter()
		: m_frameCounter(0)
		, m_lastFPS(0)
		, m_fpsTimer(0.f)
		, m_startTime()
	{
	}

	// デストラクタ
	~FPSCounter(){}

	// FPSカウント開始
	void beginCount()
	{
		m_startTime = std::chrono::high_resolution_clock::now();
	}

	// FPSカウント終了
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

	// 最後に記録したFPSを獲得
	unsigned int getLastFPS() const
	{
		return m_lastFPS;
	}

private:
	//==========================================================================
	// フィールド
	//==========================================================================
	unsigned int							m_frameCounter;	// フレームカウンタ
	unsigned int							m_lastFPS;		// 最後に記録したFPS
	float									m_fpsTimer;		// FPS計算タイマー
	std::chrono::steady_clock::time_point	m_startTime;	// 開始時間記録用
};