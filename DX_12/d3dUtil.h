#pragma once

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x600
#endif // !_WIN32_WINNT


#if defined(DEBUG)||defined(_DEBUG)
#ifndef D3D_DEBUG_INFO
#define D3D_DEBUG_INFO
#endif // !D3D_DEBUG_INFO

#endif

#if defined(DEBUG)|defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include<crtdbg.h>
#endif

#include<d3dx10.h>
#include<dxerr.h>
#include<cassert>



#if defined(DEBUG)|defined(DEBUG)

#ifndef HR
#define HR(x)
{
	HRESULT hr = (x);
	if (FAILED(hr)) {
		DXTrace(__FILE__, (DWORD)__LINE__, hr, L#x, true);
	}
}
#endif

#else
#ifndef HR
#define HR(x) (x)
#endif

#endif


#define ReleaseCOM(x){if(x){x->Release();x=0;}}


D3DX10INLINE UINT ARGB2ABGR(UINT argb)
{
	BYTE A = (argb >> 24) & 0xff;
	BYTE R = (argb >> 16) & 0xff;
	BYTE G = (argb >> 8) & 0xff;
	BYTE B = (argb >> 0) & 0xff;
	
	return (A << 24) | (B << 16) | (G << 8) | (R << 0);
}


D3DX10INLINE float RandF()
{
	return (float)(rand()) / (float)RAND_MAX;
}

D3DX10INLINE float RandF(float a, float b)
{
	return a + RandF()*(b - a);
}

D3DX10INLINE D3DXVECTOR3 RandUintVec3()
{
	D3DXVECTOR3 v(RandF(), RandF(), RandF());

	D3DXVec3Normalize(&v, &v);

	return v;
}


template<typename T>
D3DX10INLINE T Min(const T& a, const T& b)
{
	return a < b ? a : b;
}

template<typename T>
D3DX10INLINE T Max(const T& a, const T& b) {
	return a > b ? a : b;
}

template<typename T>
D3DX10INLINE T lerp(const T& a, const T& b,float t) {
	return a + (b - a)*t;
}

template<typename T>
D3DX10INLINE T Clamp(const T& x, const T& low, const T& high) {
	return x < low ? low : (x > high ? high : x);
}


//const float INFINITY = FLT_MAX;
const float PI = 3.14159265358979323f;
const float MATH_EPS = 0.0001f;

const D3DXCOLOR WHITE(1.0f, 1.0f, 1.0f, 1.0f);
const D3DXCOLOR BLACK(0.0f, 0.0f, 0.0f, 1.0f);
const D3DXCOLOR RED(1.0f, 0.0f, 0.0f, 1.0f);
const D3DXCOLOR GREEN(0.0f, 1.0f, 0.0f, 1.0f);
const D3DXCOLOR BLUE(0.0f, 0.0f, 1.0f, 1.0f);
const D3DXCOLOR YELLOW(1.0f, 1.0f, 0.0f, 1.0f);
const D3DXCOLOR CYAN(0.0f, 1.0f, 1.0f, 1.0f);
const D3DXCOLOR MAGENTA(1.0f, 0.0f, 1.0f, 1.0f);

const D3DXCOLOR BEACH_SAND(1.0f, 0.96f, 0.62f, 1.0f);
const D3DXCOLOR LIGHT_YELLOW_GREEN(0.48f, 0.77f, 0.46f, 1.0f);
const D3DXCOLOR DARK_YELLOW_GREEN(0.1f, 0.48f, 0.19f, 1.0f);
const D3DXCOLOR DARKBROWN(0.45f, 0.39f, 0.34f, 1.0f);

