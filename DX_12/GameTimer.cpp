#include "GameTimer.h"
#include<windows.h>

//QueryPerformanceCounter函数用于获取计数测量的当前时间值。返回的时间值是一个64位整数

//QueryPerformanceFrequency函数用于获取性能计时器频率（每秒的计数次数）


GameTimer::GameTimer():mSecondsPercount(0.0),mDeltaTime(-1.0),mBaseTime(0),mPausedTime(0),mPrevTime(0),mCurrTime(0),mStopped(false)
{

	__int64 countsPerSec;
	QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);

	//每次计数时间长度等于频率的倒数
	mSecondsPercount = 1.0 / (double)countsPerSec; 
}

float GameTimer::getGameTime() const
{
	if (mStopped)
	{
		return (float)((mStopTime - mBaseTime)*mSecondsPercount);
	}
	else
	{
		return (float)(((mCurrTime - mPausedTime) - mBaseTime)*mSecondsPercount);                 
	}
}

float GameTimer::getDeltaTime() const
{
	return (float)mDeltaTime;
}

void GameTimer::reset()
{
	__int64 currTime;

	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

	mBaseTime = currTime;
	mPrevTime = currTime;
	mStopTime = 0;
	mStopped = false;
}

void GameTimer::start()
{
	__int64 startTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&startTime);

	// Accumulate the time elapsed between stop and start pairs.
	//
	//                     |<-------d------->|
	// ----*---------------*-----------------*------------> time
	//  mBaseTime       mStopTime        startTime     

	if (mStopped)
	{
		mPausedTime += (startTime - mStopTime);

		mPrevTime = startTime;
		mStopTime = 0;
		mStopped = false;
	}
}

void GameTimer::stop()
{
	if (!mStopped)
	{
		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

		mStopTime = currTime;
		mStopped = true;
	}
}

void GameTimer::tick()
{
	if (mStopped)
	{
		mDeltaTime = 0.0;
		return;
	}

	//当前帧的时间数
	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
	mCurrTime = currTime;                                     

	//计算与上一帧时间的差值
	mDeltaTime = (mCurrTime - mPrevTime)*mSecondsPercount;

	//将当前帧时间数作为上一帧时间数
	mPrevTime = mCurrTime;

	if (mDeltaTime < 0.0)
	{
		mDeltaTime = 0.0;
	}

}


