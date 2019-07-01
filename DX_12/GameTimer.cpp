#include "GameTimer.h"
#include<windows.h>

//QueryPerformanceCounter�������ڻ�ȡ���������ĵ�ǰʱ��ֵ�����ص�ʱ��ֵ��һ��64λ����

//QueryPerformanceFrequency�������ڻ�ȡ���ܼ�ʱ��Ƶ�ʣ�ÿ��ļ���������


GameTimer::GameTimer():mSecondsPercount(0.0),mDeltaTime(-1.0),mBaseTime(0),mPausedTime(0),mPrevTime(0),mCurrTime(0),mStopped(false)
{

	__int64 countsPerSec;
	QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);

	//ÿ�μ���ʱ�䳤�ȵ���Ƶ�ʵĵ���
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

	//��ǰ֡��ʱ����
	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
	mCurrTime = currTime;                                     

	//��������һ֡ʱ��Ĳ�ֵ
	mDeltaTime = (mCurrTime - mPrevTime)*mSecondsPercount;

	//����ǰ֡ʱ������Ϊ��һ֡ʱ����
	mPrevTime = mCurrTime;

	if (mDeltaTime < 0.0)
	{
		mDeltaTime = 0.0;
	}

}


