#pragma once

class GameTimer {

public:
	GameTimer();

	float getGameTime()const;
	float getDeltaTime()const;

	void reset();        //message loop ǰ����
	void start();       //unpausedʱ����
	void stop();        //��ͣʱ����
	void tick();       //ÿ֡����

private:

	double mSecondsPercount;
	double mDeltaTime;

	//��ʱ��ʱ�䵥λΪ������count��
	__int64 mBaseTime;
	__int64 mPausedTime;
	__int64 mStopTime;
	__int64 mPrevTime;
	__int64 mCurrTime;

	bool mStopped;
};