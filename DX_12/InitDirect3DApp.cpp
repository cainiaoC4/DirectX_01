#include"d3dApp.h"
class InitDirect3DApp :public D3DApp
{
public:

	InitDirect3DApp(HINSTANCE hInstance);
	~InitDirect3DApp();

	void initApp();
	void onResize();

	void updateScene(float dt);
	void drawScene();
};


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE PrevInstance, PSTR cmdLine, int showCmd)
{
#if defined(DEBUG)|defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	InitDirect3DApp theApp(hInstance);
	theApp.initApp();

	return theApp.run();

}


InitDirect3DApp::InitDirect3DApp(HINSTANCE hInstance) :D3DApp(hInstance)
{
}

InitDirect3DApp::~InitDirect3DApp()
{
	if (md3dDevice)
		md3dDevice->ClearState();
}

void InitDirect3DApp::initApp()
{
	D3DApp::initApp();
}

void InitDirect3DApp::onResize()
{
	D3DApp::onResize();
}

void InitDirect3DApp::updateScene(float dt)
{
	D3DApp::updateScene(dt);
}

void InitDirect3DApp::drawScene()
{
	D3DApp::drawScene();


	RECT R = { 5,5,0,0 };

	mFont->DrawText(0, mFrameStats.c_str(), -1, &R, DT_NOCLIP, BLACK);

	mSwapChain->Present(0, 0);
}
