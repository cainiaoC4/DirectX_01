#pragma once

class GameTimer {

public:
	GameTimer();

	float getGameTime()const;
	float getDeltaTime()const;

	void reset();        //message loop 前调用
	void start();       //unpaused时调用
	void stop();        //暂停时调用
	void tick();       //每帧调用

private:

	double mSecondsPercount;
	double mDeltaTime;

	//计时器时间单位为计数（count）
	__int64 mBaseTime;
	__int64 mPausedTime;
	__int64 mStopTime;
	__int64 mPrevTime;
	__int64 mCurrTime;

	bool mStopped;
};