//COM 接口都以大写字母I作前缀

//texture 不仅是一个数组，可以带有多级渐进纹理层，GPU可以在纹理上执行特殊运算。
//纹理支持特定格式的数据存储
// DXGI_FORMAT_R32G32B32_FLOAT    每个元素包含3个32位浮点数
//DXGI_FORMAT_R16G16B16A16_UNORM    4个16位分量，分量取值[0,1]
//DXGI_FORMAT_R32G32_UINT          2个32位无符号整型数
//DXGI_FORMAT_R8G8B8A8_UNORM       4个8位分量 无符号 取值[0,1]
//DXGI_FORMAT_R8G8B8A8_SNORM       4个8位分量 有符号 取值[-1,1]
//DXGI_FORMAT_R8G8B8A8_SINT        4个8位有符号整型  取值[-128,127]
//DXGI_FORMAT_R8G8B8A8_UINT        4个8位无符号整型  取值[0,255]

//另有弱类型typeless格式，预先分配内存空间，然后在纹理绑定到管线时再指定如何重新解释数据内容
//DXGI_FORMAT_R8G8B8A8_TYPELESS

//避免动画闪烁，最好的做法是在一个离屏纹理中执行所有绘制工作，这个纹理称为后台缓冲区 backbuffer。硬件自动维护两个内置纹理缓冲区来实现 一帧的绘制时间小于屏幕垂直刷新时间。
//两个缓冲区分别为前台缓冲区和后台缓冲区。前台缓冲区存储了当前显示在屏幕上的图像数据，动画的下一帧会在后台缓冲区中执行绘制，当后台缓冲区绘制工作完成后，前后两个缓冲区的作用会发生翻转，前台变后台，后台变前台，为下一帧绘制提前做好准备
// 前后缓冲区形成一个交换链（swap chain）   
//Direct3D 中交换链由 IDXGISwapChain接口表示   提供了调整缓冲区尺寸的方法IDXGISwapChain::ResizeBuffers  及提交方法IDXGISwapChain::Present
//使用前后两个缓冲区称为双缓冲，缓冲区的数量可多于两个。


//深度缓冲区（depth buffer）是一个不包含图像数据的纹理对象，深度可以被认为是一种特殊的像素。0.0表示离观察者最近的物体，1.0表示离观察者最远的物体。深度缓冲区的每个元素与后台缓冲区的每个元素一一对应。
//为了判定物体的哪些像素位于其他物体前，DX3D使用了深度缓存(depth buffering)或z缓存（z-buffering）的技术
// more... see 


//纹理资源视图 纹理可以被绑定到渲染管线（rendering pipeline)的不同阶段（stage）。如较常见的将纹理作为渲染目标（渲染到纹理）或着色器资源（在着色器中对纹理进行采样）
//当创建用于这两种目的的纹理时应该使用绑定标志值  
// D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE

//实际上资源不能被直接绑定到一个管线阶段，只能把与资源关联的资源视图绑定到不同的管线阶段。
//DX 始终要求我们在初始化时为纹理创建相关的资源视图，有助于提高运行效率。创建视图时执行相应的验证和映射，减少绑定时的类型检查。
//
//所创建的两种视图为 渲染目标视图（ID3D10RenderTargetView） 和着色器资源视图（ID3D10ShaderResourceView）
//资源视图 告诉DX 如何使用资源，若在创建资源时指定的是弱类型格式，纹理元素可能会在一个管线阶段中视为浮点数，而在另一个管线阶段中视为整数。
//必须在创建资源时使用特定的绑定标志值，若没有使用D3D10_BIND_DEPTH_STENCIL （将纹理作为一个深度/模板 缓冲区绑定到管线上）绑定标志值，则无法为该资源创建ID3D10DepthStencilView视图

//多重采样，计算机显示器像素分辨率有限导致一条直线有aliasing（锯齿） 效应
//抗锯齿（antialiasing） DX支持一种称为多重采样的抗锯齿技术，通过对一个像素的邻接像素进行采样计算出该像素的最终颜色。

//初始化过程
//1.填充一个 DXGI_SWAP_CHAIN_DESC 结构体，描述了所要创建的交换链特性
//2.使用D3D10CreateDeviceAndSwapChain 函数创建ID3D10Device接口和IDXGISwapChain接口
//3.为交换链的后台缓冲区创建一个渲染目标视图
//4.创建深度/模板缓冲区以及相关的深度/模板视图
//5.将渲染目标视图和深度/模板视图绑定到渲染管线的输出合并阶段，使它们可以被Direct3D使用
//6.设置视口


#include<d3d10.h>

//typedef struct DXGI_SWAP_CHAIN_DESC {              //填充DXGI_SWAP_CHAIN_DESC 以描述将创建的交换链特性
//	DXGI_MODE_DESC BufferDesc;                       // 描述了我们所要创建的后台缓冲区属性  宽度、高度、像素格式等等
//	DXGI_SAMPLE_DESC SampleDesc;                     //多重采样数量和质量级别
//	DXGI_USAGE BufferUsage;                          //设为DXGI_USAGE_RENDER_TARGET_OUTPUT 讲场景渲染到后台缓冲区，即将它用作渲染目标
//	UINT BufferCount;                                //交换链中的后台缓冲区数量，一般只用一个后台缓冲区实现双缓存。
//	HWND OutputWindow;                               //将要渲染到的窗口的句柄
//	BOOL Windowed;                                   //设为true时，程序以窗口模式允许，false时，程序以全屏模式运行
//	DXGI_SWAP_EFFECT SwapEffect;                     //设为DXGI_SWAP_EFFECT_DISCARD 让显卡驱动程序选择最高效的显示模式
//	UINT Flags;                                      //可选的标志值，若设置为DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWICH ,则当应用程序切换到全屏模式时，Dx会自动选择与当前后台缓冲区设置最匹配的显示模式，若未使用，则当切换到全屏时，Dx会使用当前的桌面显示模式。
//}DXGI_SWAP_CHAIN_DESC;


//创建DX10 设备（ID3D10Device）和交换链(IDXGISwapChain)
//设备和交换链可以用如下函数创建

//HRESULT WINAPI D3D10CreateDeviceAndSwapChain(
//
//	IDXGIAdapter*              pAdapter,              //指定要为哪个物理显卡创建设备对象，设为空时，表示使用主显卡
//	D3D10_DRIVER_TYPE          DriverType,            //一般总是指定D3D10_DRIVER_TYPE_HARDWARE  表示使用3D硬件加速，也可指定D3D10_DRIVER_TYPE_REFERENCE创建引用设备（DX的纯软件实现，慢）
//	HMODULE                    Software,              //用于支持软件光栅化设备，总是设为空，因为使用硬件加速。若使用软件光栅化，必须先安装一个软件光栅化设备
//	UINT                       Flags,                 //可选的设备创建标志值。以release模式生成呈程序时，该参数通常设置为0，以debug模式生成时，应该设置为D3D10_CREATE_DEVICE_DEBUG 以激活调试层。
//	UINT                       SDKVersion,            //始终设置为D3D10_SDK_VERSION
//	DXGI_SWAP_CHAIN_DESC*      pSwapChainDesc,        //指向DXGI_SWAP_CHAIN_DESC结构体的指针，该结构体用于描述将要创建的交换链
//	IDXGISwapChain**           ppSwapChain,           //返回创建后的交换链对象
//	ID3D10Device**             ppDevice               //返回创建后的设备对象
//);



int main()
{
	int mClientWidth = 800;
	int mClientHeight = 600;

	DXGI_SWAP_CHAIN_DESC sd;

	sd.BufferDesc.Width = mClientWidth;
	sd.BufferDesc.Height = mClientHeight;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;


	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = 1;
	sd.OutputWindow = mhMainWnd;
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	sd.Flags = 0;

//=====================================================创建设备和交换链
	UINT createDeviceFlags = 0;
#if defined(DEBUG)||defined(_DEBUG)
	createDeviceFlags |= D3D10_CREATE_DEVICE_DEBUG;
#endif

	ID3D10Device* md3dDevice;
	IDXGISwapChain* mSwapChain;
	D3D10CreateDeviceAndSwapChain(0, D3D10_DRIVER_TYPE_HARDWARE, 0, createDeviceFlags, D3D10_SDK_VERSION, &sd, &mSwapChain, &md3dDevice);
//=========================================================================================================================================

//==================创建渲染目标视图
	ID3D10RenderTargetView* mRenderTargetView;
	ID3D10Texture2D* backBuffer;
	mSwapChain->GetBuffer(0, __uuidof(ID3D10Texture2D), reinterpret_cast<void**>(&backBuffer));
	md3dDevice->CreateRenderTargetView(backBuffer, 0, &mRenderTargetView);

	ReleaseCOM(backBuffer);
//======================================================

//=================创建深度/模板缓冲区及其视图
	D3D10_TEXTURE2D_DESC depthStencilDesc;
	depthStencilDesc.Width = mClientWidth;
	depthStencilDesc.Height = mClientHeight;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.ArraySize = 1;
	depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.SampleDesc.Quality = 0;
	depthStencilDesc.Usage = D3D10_USAGE_DEFAULT;
	depthStencilDesc.BindFlags = D3D10_BIND_DEPTH_STENCIL;
	depthStencilDesc.CPUAccessFlags = 0;
	depthStencilDesc.MiscFlags = 0;
	
	ID3D10Texture2D* mDepthStencilBuffer;
	ID3D10DepthStencilView* mDepthStencilView;

	HR(md3dDevice->CreateTexture2D(&depthStencilDesc, 0, &mDepthStencilBuffer));

	HR(md3dDevice->CreateDepthStencilView(mDepthStencilBuffer, 0, &mDepthStencilView));
//============================================

//=================将视图绑定到输出合并器阶段

	md3dDevice->OMGetRenderTargets(1, &mRenderTargetView, &mDepthStencilView);

//================设置视口
	D3D10_VIEWPORT vp;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	vp.Width = mClientWidth;
	vp.Height = mClientHeight;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 0.0f;
	md3dDevice->RSSetViewports(1, &vp);

}


