//COM �ӿڶ��Դ�д��ĸI��ǰ׺

//texture ������һ�����飬���Դ��ж༶��������㣬GPU������������ִ���������㡣
//����֧���ض���ʽ�����ݴ洢
// DXGI_FORMAT_R32G32B32_FLOAT    ÿ��Ԫ�ذ���3��32λ������
//DXGI_FORMAT_R16G16B16A16_UNORM    4��16λ����������ȡֵ[0,1]
//DXGI_FORMAT_R32G32_UINT          2��32λ�޷���������
//DXGI_FORMAT_R8G8B8A8_UNORM       4��8λ���� �޷��� ȡֵ[0,1]
//DXGI_FORMAT_R8G8B8A8_SNORM       4��8λ���� �з��� ȡֵ[-1,1]
//DXGI_FORMAT_R8G8B8A8_SINT        4��8λ�з�������  ȡֵ[-128,127]
//DXGI_FORMAT_R8G8B8A8_UINT        4��8λ�޷�������  ȡֵ[0,255]

//����������typeless��ʽ��Ԥ�ȷ����ڴ�ռ䣬Ȼ��������󶨵�����ʱ��ָ��������½�����������
//DXGI_FORMAT_R8G8B8A8_TYPELESS

//���⶯����˸����õ���������һ������������ִ�����л��ƹ�������������Ϊ��̨������ backbuffer��Ӳ���Զ�ά��������������������ʵ�� һ֡�Ļ���ʱ��С����Ļ��ֱˢ��ʱ�䡣
//�����������ֱ�Ϊǰ̨�������ͺ�̨��������ǰ̨�������洢�˵�ǰ��ʾ����Ļ�ϵ�ͼ�����ݣ���������һ֡���ں�̨��������ִ�л��ƣ�����̨���������ƹ�����ɺ�ǰ�����������������ûᷢ����ת��ǰ̨���̨����̨��ǰ̨��Ϊ��һ֡������ǰ����׼��
// ǰ�󻺳����γ�һ����������swap chain��   
//Direct3D �н������� IDXGISwapChain�ӿڱ�ʾ   �ṩ�˵����������ߴ�ķ���IDXGISwapChain::ResizeBuffers  ���ύ����IDXGISwapChain::Present
//ʹ��ǰ��������������Ϊ˫���壬�������������ɶ���������


//��Ȼ�������depth buffer����һ��������ͼ�����ݵ����������ȿ��Ա���Ϊ��һ����������ء�0.0��ʾ��۲�����������壬1.0��ʾ��۲�����Զ�����塣��Ȼ�������ÿ��Ԫ�����̨��������ÿ��Ԫ��һһ��Ӧ��
//Ϊ���ж��������Щ����λ����������ǰ��DX3Dʹ������Ȼ���(depth buffering)��z���棨z-buffering���ļ���
// more... see 


//������Դ��ͼ ������Ա��󶨵���Ⱦ���ߣ�rendering pipeline)�Ĳ�ͬ�׶Σ�stage������ϳ����Ľ�������Ϊ��ȾĿ�꣨��Ⱦ����������ɫ����Դ������ɫ���ж�������в�����
//����������������Ŀ�ĵ�����ʱӦ��ʹ�ð󶨱�־ֵ  
// D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE

//ʵ������Դ���ܱ�ֱ�Ӱ󶨵�һ�����߽׶Σ�ֻ�ܰ�����Դ��������Դ��ͼ�󶨵���ͬ�Ĺ��߽׶Ρ�
//DX ʼ��Ҫ�������ڳ�ʼ��ʱΪ��������ص���Դ��ͼ���������������Ч�ʡ�������ͼʱִ����Ӧ����֤��ӳ�䣬���ٰ�ʱ�����ͼ�顣
//
//��������������ͼΪ ��ȾĿ����ͼ��ID3D10RenderTargetView�� ����ɫ����Դ��ͼ��ID3D10ShaderResourceView��
//��Դ��ͼ ����DX ���ʹ����Դ�����ڴ�����Դʱָ�����������͸�ʽ������Ԫ�ؿ��ܻ���һ�����߽׶�����Ϊ��������������һ�����߽׶�����Ϊ������
//�����ڴ�����Դʱʹ���ض��İ󶨱�־ֵ����û��ʹ��D3D10_BIND_DEPTH_STENCIL ����������Ϊһ�����/ģ�� �������󶨵������ϣ��󶨱�־ֵ�����޷�Ϊ����Դ����ID3D10DepthStencilView��ͼ

//���ز������������ʾ�����طֱ������޵���һ��ֱ����aliasing����ݣ� ЧӦ
//����ݣ�antialiasing�� DX֧��һ�ֳ�Ϊ���ز����Ŀ���ݼ�����ͨ����һ�����ص��ڽ����ؽ��в�������������ص�������ɫ��

//��ʼ������
//1.���һ�� DXGI_SWAP_CHAIN_DESC �ṹ�壬��������Ҫ�����Ľ���������
//2.ʹ��D3D10CreateDeviceAndSwapChain ��������ID3D10Device�ӿں�IDXGISwapChain�ӿ�
//3.Ϊ�������ĺ�̨����������һ����ȾĿ����ͼ
//4.�������/ģ�建�����Լ���ص����/ģ����ͼ
//5.����ȾĿ����ͼ�����/ģ����ͼ�󶨵���Ⱦ���ߵ�����ϲ��׶Σ�ʹ���ǿ��Ա�Direct3Dʹ��
//6.�����ӿ�


#include<d3d10.h>

//typedef struct DXGI_SWAP_CHAIN_DESC {              //���DXGI_SWAP_CHAIN_DESC �������������Ľ���������
//	DXGI_MODE_DESC BufferDesc;                       // ������������Ҫ�����ĺ�̨����������  ��ȡ��߶ȡ����ظ�ʽ�ȵ�
//	DXGI_SAMPLE_DESC SampleDesc;                     //���ز�����������������
//	DXGI_USAGE BufferUsage;                          //��ΪDXGI_USAGE_RENDER_TARGET_OUTPUT ��������Ⱦ����̨��������������������ȾĿ��
//	UINT BufferCount;                                //�������еĺ�̨������������һ��ֻ��һ����̨������ʵ��˫���档
//	HWND OutputWindow;                               //��Ҫ��Ⱦ���Ĵ��ڵľ��
//	BOOL Windowed;                                   //��Ϊtrueʱ�������Դ���ģʽ����falseʱ��������ȫ��ģʽ����
//	DXGI_SWAP_EFFECT SwapEffect;                     //��ΪDXGI_SWAP_EFFECT_DISCARD ���Կ���������ѡ�����Ч����ʾģʽ
//	UINT Flags;                                      //��ѡ�ı�־ֵ��������ΪDXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWICH ,��Ӧ�ó����л���ȫ��ģʽʱ��Dx���Զ�ѡ���뵱ǰ��̨������������ƥ�����ʾģʽ����δʹ�ã����л���ȫ��ʱ��Dx��ʹ�õ�ǰ��������ʾģʽ��
//}DXGI_SWAP_CHAIN_DESC;


//����DX10 �豸��ID3D10Device���ͽ�����(IDXGISwapChain)
//�豸�ͽ��������������º�������

//HRESULT WINAPI D3D10CreateDeviceAndSwapChain(
//
//	IDXGIAdapter*              pAdapter,              //ָ��ҪΪ�ĸ������Կ������豸������Ϊ��ʱ����ʾʹ�����Կ�
//	D3D10_DRIVER_TYPE          DriverType,            //һ������ָ��D3D10_DRIVER_TYPE_HARDWARE  ��ʾʹ��3DӲ�����٣�Ҳ��ָ��D3D10_DRIVER_TYPE_REFERENCE���������豸��DX�Ĵ����ʵ�֣�����
//	HMODULE                    Software,              //����֧�������դ���豸��������Ϊ�գ���Ϊʹ��Ӳ�����١���ʹ�������դ���������Ȱ�װһ�������դ���豸
//	UINT                       Flags,                 //��ѡ���豸������־ֵ����releaseģʽ���ɳʳ���ʱ���ò���ͨ������Ϊ0����debugģʽ����ʱ��Ӧ������ΪD3D10_CREATE_DEVICE_DEBUG �Լ�����Բ㡣
//	UINT                       SDKVersion,            //ʼ������ΪD3D10_SDK_VERSION
//	DXGI_SWAP_CHAIN_DESC*      pSwapChainDesc,        //ָ��DXGI_SWAP_CHAIN_DESC�ṹ���ָ�룬�ýṹ������������Ҫ�����Ľ�����
//	IDXGISwapChain**           ppSwapChain,           //���ش�����Ľ���������
//	ID3D10Device**             ppDevice               //���ش�������豸����
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

//=====================================================�����豸�ͽ�����
	UINT createDeviceFlags = 0;
#if defined(DEBUG)||defined(_DEBUG)
	createDeviceFlags |= D3D10_CREATE_DEVICE_DEBUG;
#endif

	ID3D10Device* md3dDevice;
	IDXGISwapChain* mSwapChain;
	D3D10CreateDeviceAndSwapChain(0, D3D10_DRIVER_TYPE_HARDWARE, 0, createDeviceFlags, D3D10_SDK_VERSION, &sd, &mSwapChain, &md3dDevice);
//=========================================================================================================================================

//==================������ȾĿ����ͼ
	ID3D10RenderTargetView* mRenderTargetView;
	ID3D10Texture2D* backBuffer;
	mSwapChain->GetBuffer(0, __uuidof(ID3D10Texture2D), reinterpret_cast<void**>(&backBuffer));
	md3dDevice->CreateRenderTargetView(backBuffer, 0, &mRenderTargetView);

	ReleaseCOM(backBuffer);
//======================================================

//=================�������/ģ�建����������ͼ
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

//=================����ͼ�󶨵�����ϲ����׶�

	md3dDevice->OMGetRenderTargets(1, &mRenderTargetView, &mDepthStencilView);

//================�����ӿ�
	D3D10_VIEWPORT vp;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	vp.Width = mClientWidth;
	vp.Height = mClientHeight;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 0.0f;
	md3dDevice->RSSetViewports(1, &vp);

}


