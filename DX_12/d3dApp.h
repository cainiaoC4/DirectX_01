#pragma once

#include"d3dUtil.h"
#include"GameTimer.h"
#include<string>
#include<vector>
class D3DApp {

public:
	D3DApp(HINSTANCE hInstance);
	virtual ~D3DApp();

	HINSTANCE getAppInst();              //获取应用程序实例句柄副本
	HWND getMainWnd();                  //获取窗口句柄副本

	int run();                          //封装应用程序消息循环

	virtual void initApp();            //程序初始化，分配资源、初始化对象、设置灯光等等
	virtual void onResize();           //实现了后台缓冲区和深度缓冲区调整的功能
	virtual void updateScene(float dt);  //帧更新函数
	virtual void drawScene();            //帧绘制函数
	virtual LRESULT msgProc(UINT msg, WPARAM wParam, LPARAM lParam);         //主应用程序窗口消息处理函数

protected:
	void initMainWindow();           //c初始化应用程序窗口
	void initDirect3D();             //初始化Direct3D

protected:

	HINSTANCE mhAppInst;
	HWND mhMainWnd;
	bool mAppPaused;
	bool mMinimized;
	bool mMaximized;
	bool mResizing;

	GameTimer mTimer;

	std::wstring mFrameStats;

	ID3D10Device*                  md3dDevice;
	IDXGISwapChain*                mSwapChain;
	ID3D10Texture2D*               mDepthStencilBuffer;
	ID3D10RenderTargetView*        mRenderTargetView;
	ID3D10DepthStencilView*        mDepthStencilView;
	ID3DX10Font*                   mFont;


	std::wstring mMainWndCaption;
	D3D10_DRIVER_TYPE md3dDriverType;
	D3DXCOLOR mClearColor;
	int mClientWidth;
	int mClientHeight;

};

