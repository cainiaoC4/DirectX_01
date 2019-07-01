#pragma once

#include"d3dUtil.h"
#include"GameTimer.h"
#include<string>
#include<vector>
class D3DApp {

public:
	D3DApp(HINSTANCE hInstance);
	virtual ~D3DApp();

	HINSTANCE getAppInst();              //��ȡӦ�ó���ʵ���������
	HWND getMainWnd();                  //��ȡ���ھ������

	int run();                          //��װӦ�ó�����Ϣѭ��

	virtual void initApp();            //�����ʼ����������Դ����ʼ���������õƹ�ȵ�
	virtual void onResize();           //ʵ���˺�̨����������Ȼ����������Ĺ���
	virtual void updateScene(float dt);  //֡���º���
	virtual void drawScene();            //֡���ƺ���
	virtual LRESULT msgProc(UINT msg, WPARAM wParam, LPARAM lParam);         //��Ӧ�ó��򴰿���Ϣ������

protected:
	void initMainWindow();           //c��ʼ��Ӧ�ó��򴰿�
	void initDirect3D();             //��ʼ��Direct3D

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

