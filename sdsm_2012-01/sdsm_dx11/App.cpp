// Copyright 2012 Intel Corporation
// All Rights Reserved
//
// Permission is granted to use, copy, distribute and prepare derivative works of this
// software for any purpose and without fee, provided, that the above copyright notice
// and this statement appear in all copies.  Intel makes no representations about the
// suitability of this software for any purpose.  THIS SOFTWARE IS PROVIDED "AS IS."
// INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, AND ALL LIABILITY,
// INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES, FOR THE USE OF THIS SOFTWARE,
// INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY RIGHTS, AND INCLUDING THE
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  Intel does not
// assume any responsibility for any errors which may appear in this software nor any
// responsibility to update it.

#include "App.h"
#include "Partitions.h"
#include <limits>
#include <sstream>

using std::tr1::shared_ptr;

// NOTE: Must match layout of shader constant buffers

__declspec(align(16))
struct PerFrameConstants
{
    D3DXMATRIX mCameraWorldViewProj;
    D3DXMATRIX mCameraWorldView;
    D3DXMATRIX mCameraViewProj;
    D3DXMATRIX mCameraProj;
    D3DXMATRIX mLightWorldViewProj;
    D3DXMATRIX mCameraViewToLightProj;
    D3DXVECTOR4 mLightDir;
    D3DXVECTOR4 mBlurSizeLightSpace;
    D3DXVECTOR4 mCameraNearFar;
	D3DXVECTOR4 mShadowTextureDim;

    UIConstants mUI;
};

__declspec(align(16))
struct PerPartitionPassConstants
{
    unsigned int mRenderPartition;
    unsigned int mAccumulatePartitionBegin;
    unsigned int mAccumulatePartitionCount;
};

__declspec(align(16))
struct BoxBlurConstants
{
    D3DXVECTOR2 mFilterSize;
    unsigned int mPartition;
    unsigned int mDimension;
};

App::App(ID3D11Device *d3dDevice,
         unsigned int partitions, unsigned int partitionsPerPass,
         unsigned int shadowTextureDim, unsigned int shadowAASamples,
         bool shadowTransparencyAA,
         FilteringScheme filteringScheme)
    : mPartitions(partitions)
    , mPartitionsPerPass(partitionsPerPass)
    , mShadowTextureDim(shadowTextureDim)
    , mShadowAASamples(shadowAASamples)
    , mFilteringScheme(filteringScheme)
    , mPartitionWorldViewProj(partitions)
    , mSaveNormalizedDepth(false)
{
    // Set up macros
    std::string shadowAAStr;
    {
        std::ostringstream oss;
        oss << mShadowAASamples;
        shadowAAStr = oss.str();
    }

    // Set up macro for filtering scheme include file
    const char *filteringDefine;
    switch (mFilteringScheme) {
        case NEIGHBOR_PCF_FILTERING: filteringDefine = "NEIGHBOR_PCF_FILTERING"; break;
        default:  filteringDefine = "EVSM_FILTERING"; break;
    };

    D3D10_SHADER_MACRO defines[] = {
        {"SHADOWAA_SAMPLES", shadowAAStr.c_str()},
        {"SHADOWTRANSPARENCYAA_ENABLED", shadowTransparencyAA ? "1" : "0"},
        {filteringDefine, "1"},
        {0, 0}
    };

    // Create shaders
    mGeometryVS = new VertexShader(d3dDevice, L"Rendering.hlsl", "GeometryVS", defines);
    mGeometryPS = new PixelShader(d3dDevice, L"Rendering.hlsl", "GeometryPS", defines);
    mGeometryAlphaTestPS = new PixelShader(d3dDevice, L"Rendering.hlsl", "GeometryAlphaTestPS", defines);

    mShadowVS = new VertexShader(d3dDevice, L"Rendering.hlsl", "ShadowVS", defines);
    mShadowAlphaTestVS = new VertexShader(d3dDevice, L"Rendering.hlsl", "ShadowAlphaTestVS", defines);
    mShadowAlphaTestPS = new PixelShader(d3dDevice, L"Rendering.hlsl", "ShadowAlphaTestPS", defines);

    mFullScreenTriangleVS = new VertexShader(d3dDevice, L"Rendering.hlsl", "FullScreenTriangleVS", defines);
    mLightingPS = new PixelShader(d3dDevice, L"Rendering.hlsl", "LightingPS", defines);
        
    // Additional EVSM shaders - loaded only when needed
    // TODO: Replace with lazy loading of shaders
    mShadowDepthToEVSMPS = 0;
    mBoxBlurVS = 0;
    mBoxBlurPS = 0;
    if (mFilteringScheme == EVSM_FILTERING) {
        mShadowDepthToEVSMPS = new PixelShader(d3dDevice, L"Rendering.hlsl", "ShadowDepthToEVSMPS", defines);

        mBoxBlurVS = new VertexShader(d3dDevice, L"BoxBlur.hlsl", "BoxBlurVS", defines);
        mBoxBlurPS = new PixelShader(d3dDevice, L"BoxBlur.hlsl", "BoxBlurPS", defines);
    }

    mSkyboxVS = new VertexShader(d3dDevice, L"Rendering.hlsl", "SkyboxVS", defines);
    mSkyboxPS = new PixelShader(d3dDevice, L"Rendering.hlsl", "SkyboxPS", defines);

    mVisualizeHistogramPS = new PixelShader(d3dDevice, L"Visualize.hlsl", "VisualizeHistogramPS", defines);

    mVisualizeLightSpaceVS = new VertexShader(d3dDevice, L"Visualize.hlsl", "VisualizeLightSpaceVS", defines);
    mVisualizeLightSpacePS = new PixelShader(d3dDevice, L"Visualize.hlsl", "VisualizeLightSpacePS", defines);
    mVisualizeLightSpaceAlphaTestPS = new PixelShader(d3dDevice, L"Visualize.hlsl", "VisualizeLightSpaceAlphaTestPS", defines);

    mVisualizeLightSpaceScatterVS = new VertexShader(d3dDevice, L"Visualize.hlsl", "VisualizeLightSpaceScatterVS", defines);
    mVisualizeLightSpaceScatterPS = new PixelShader(d3dDevice, L"Visualize.hlsl", "VisualizeLightSpaceScatterPS", defines);

    mVisualizeFrustumVS = new VertexShader(d3dDevice, L"Visualize.hlsl", "VisualizeFrustumVS", defines);
    mVisualizeFrustumPS = new PixelShader(d3dDevice, L"Visualize.hlsl", "VisualizeFrustumPS", defines);

    mVisualizeLightSpacePartitionsPS = new PixelShader(d3dDevice, L"Visualize.hlsl", "VisualizeLightSpacePartitionsPS", defines);

    mErrorPS = new PixelShader(d3dDevice, L"Visualize.hlsl", "ErrorPS", defines);

    mNormalizedDepthPS = new PixelShader(d3dDevice, L"Visualize.hlsl", "NormalizedDepthPS", defines);
    

    // Create input layout
    {
        // We need the vertex shader bytecode for this... rather than try to wire that all through the
        // shader interface, just recompile the vertex shader.
        UINT shaderFlags = D3D10_SHADER_ENABLE_STRICTNESS | D3D10_SHADER_PACK_MATRIX_ROW_MAJOR;
        ID3D10Blob *bytecode = 0;
        HRESULT hr = D3DX11CompileFromFile(L"Rendering.hlsl", defines, 0, "GeometryVS", "vs_5_0", shaderFlags, 0, 0, &bytecode, 0, 0);
        if (FAILED(hr)) {
            assert(false);      // It worked earlier...
        }

        const D3D11_INPUT_ELEMENT_DESC layout[] =
        {
            {"position",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"normal",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"texCoord",  0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        
        d3dDevice->CreateInputLayout( 
            layout, ARRAYSIZE(layout), 
            bytecode->GetBufferPointer(),
            bytecode->GetBufferSize(), 
            &mMeshVertexLayout);

        bytecode->Release();
    }

    // Create standard rasterizer state
    {
        CD3D11_RASTERIZER_DESC desc(D3D11_DEFAULT);
        d3dDevice->CreateRasterizerState(&desc, &mRasterizerState);

        desc.CullMode = D3D11_CULL_NONE;
        d3dDevice->CreateRasterizerState(&desc, &mDoubleSidedRasterizerState);
    }

    // Shadow rasterizer state has no back-face culling and multisampling enabled
    {
        CD3D11_RASTERIZER_DESC desc(D3D11_DEFAULT);
        // Always use double-sided objects for shadowing
        // TODO: No backface culling is non-ideal here, but some of the scenes require it.
        desc.CullMode = D3D11_CULL_NONE;
        desc.MultisampleEnable = true;
        // Include shadows from objects that may be closer to the light than the camera
        // frustum. This disables any clipping based on "depth" in light space.
        desc.DepthClipEnable = false;
        d3dDevice->CreateRasterizerState(&desc, &mShadowRasterizerState);
    }

    // Standard depth stencil state
    {
        CD3D11_DEPTH_STENCIL_DESC desc(D3D11_DEFAULT);
        desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        d3dDevice->CreateDepthStencilState(&desc, &mShadowDepthStencilState);

        // NOTE: Complementary Z => GREATER test
        desc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
        d3dDevice->CreateDepthStencilState(&desc, &mGeomeryDepthStencilState);
    }

    // Create geometry phase blend state
    {
        CD3D11_BLEND_DESC desc(D3D11_DEFAULT);
        d3dDevice->CreateBlendState(&desc, &mGeometryBlendState);
    }

    // Create lighting phase blend state
    {
        CD3D11_BLEND_DESC desc(D3D11_DEFAULT);
        // Additive blending
        desc.RenderTarget[0].BlendEnable = true;
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
        desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        d3dDevice->CreateBlendState(&desc, &mLightingBlendState);
    }

    // Create alpha blend state
    // Create lighting phase blend state
    {
        CD3D11_BLEND_DESC desc(D3D11_DEFAULT);
        // Alpha blending
        desc.RenderTarget[0].BlendEnable = true;
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        d3dDevice->CreateBlendState(&desc, &mAlphaBlendState);
    }

    // Create constant buffers
    {
        CD3D11_BUFFER_DESC desc(
            sizeof(PerFrameConstants),
            D3D11_BIND_CONSTANT_BUFFER,
            D3D11_USAGE_DYNAMIC,
            D3D11_CPU_ACCESS_WRITE);

        d3dDevice->CreateBuffer(&desc, 0, &mPerFrameConstants);
    }
    {
        CD3D11_BUFFER_DESC desc(
            sizeof(PerPartitionPassConstants),
            D3D11_BIND_CONSTANT_BUFFER,
            D3D11_USAGE_DYNAMIC,
            D3D11_CPU_ACCESS_WRITE);

        d3dDevice->CreateBuffer(&desc, 0, &mPerPartitionPassConstants);
    }
    {
        CD3D11_BUFFER_DESC desc(
            sizeof(BoxBlurConstants),
            D3D11_BIND_CONSTANT_BUFFER,
            D3D11_USAGE_DYNAMIC,
            D3D11_CPU_ACCESS_WRITE);

        d3dDevice->CreateBuffer(&desc, 0, &mBoxBlurConstants);
    }

    // Create sampler state
    {
        CD3D11_SAMPLER_DESC desc(D3D11_DEFAULT);
        desc.Filter = D3D11_FILTER_ANISOTROPIC;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.MaxAnisotropy = 16;
        d3dDevice->CreateSamplerState(&desc, &mDiffuseSampler);
    }
    {
        CD3D11_SAMPLER_DESC desc(D3D11_DEFAULT);
        desc.Filter = D3D11_FILTER_ANISOTROPIC;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.MaxAnisotropy = 16;
        d3dDevice->CreateSamplerState(&desc, &mEVSMShadowSampler);
    }
    {
        CD3D11_SAMPLER_DESC desc(D3D11_DEFAULT);
        desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
        d3dDevice->CreateSamplerState(&desc, &mPCFShadowSampler);
    }

    // Create shadow map and viewport
    {
        // One depth buffer (potentially with multiple samples)
        // MSAA is not supported with PCF
        if (mFilteringScheme == EVSM_FILTERING) {
            // One MSAAed depth buffer
            DXGI_SAMPLE_DESC sampleDesc;
            sampleDesc.Count = mShadowAASamples;
            sampleDesc.Quality = 0;
            mShadowDepthTexture = shared_ptr<Depth2D>(new Depth2D(
                d3dDevice, mShadowTextureDim, mShadowTextureDim, D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE, sampleDesc));

            // PartitionsPerPass EVSM textures (full mip chain each)
            mShadowEVSMTexture = shared_ptr<Texture2D>(new Texture2D(
                d3dDevice, mShadowTextureDim, mShadowTextureDim, DXGI_FORMAT_R32G32B32A32_FLOAT,
                D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, 0, mPartitionsPerPass));

            // Temporary texture for blurring (no mip chain needed)
            mShadowEVSMBlurTexture = shared_ptr<Texture2D>(new Texture2D(
                d3dDevice, mShadowTextureDim, mShadowTextureDim, DXGI_FORMAT_R32G32B32A32_FLOAT,
                D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, 1, 1));
        } else {
            // PartitionsPerPass non-MSAAed depth buffers
            mShadowDepthTexture = shared_ptr<Depth2D>(new Depth2D(
                d3dDevice, mShadowTextureDim, mShadowTextureDim, D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE, mPartitionsPerPass));
        }
    
        mShadowViewport.Width = static_cast<float>(mShadowTextureDim);
        mShadowViewport.Height = static_cast<float>(mShadowTextureDim);
        mShadowViewport.MinDepth = 0.0f;
        mShadowViewport.MaxDepth = 1.0f;
        mShadowViewport.TopLeftX = 0.0f;
        mShadowViewport.TopLeftY = 0.0f;
    }

    // Create error readback texture
    {
        CD3D11_TEXTURE2D_DESC desc(
            DXGI_FORMAT_R32_FLOAT, 1, 1, 1, 1, 0,
            D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ);
        d3dDevice->CreateTexture2D(&desc, 0, &mErrorReadbackTexture);        
    }

    // Create partition readback texture, which is used with some UI combinations
    {
        UINT structSize = sizeof(Partition);

        CD3D11_BUFFER_DESC desc(
            structSize * mPartitions,            
            0,
            D3D11_USAGE_STAGING,
            D3D11_CPU_ACCESS_READ,
            D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
            structSize);
        d3dDevice->CreateBuffer(&desc, 0, &mPartitionReadback);
    }

    // Create skybox mesh
    mSkyboxMesh.Create(d3dDevice, L"Media\\Skybox\\Skybox.sdkmesh");

    // Set up partitioning implementations
    mSDSMPartitions = new SDSMPartitions(d3dDevice, mPartitions, 1024, shadowTextureDim);
    mPSSMPartitions = new PSSMPartitions(d3dDevice, mPartitions);
}


App::~App() 
{
    delete mPSSMPartitions;
    delete mSDSMPartitions;
    mSkyboxMesh.Destroy();
    SAFE_RELEASE(mPartitionReadback);
    SAFE_RELEASE(mErrorReadbackTexture);
    SAFE_RELEASE(mPCFShadowSampler);
    SAFE_RELEASE(mEVSMShadowSampler);
    SAFE_RELEASE(mDiffuseSampler);
    SAFE_RELEASE(mBoxBlurConstants);
    SAFE_RELEASE(mPerPartitionPassConstants);
    SAFE_RELEASE(mPerFrameConstants);
    SAFE_RELEASE(mAlphaBlendState);
    SAFE_RELEASE(mLightingBlendState);
    SAFE_RELEASE(mGeometryBlendState);
    SAFE_RELEASE(mShadowDepthStencilState);
    SAFE_RELEASE(mGeomeryDepthStencilState);
    SAFE_RELEASE(mShadowRasterizerState);
    SAFE_RELEASE(mDoubleSidedRasterizerState);
    SAFE_RELEASE(mRasterizerState);
    SAFE_RELEASE(mMeshVertexLayout);
    delete mSkyboxPS;
    delete mSkyboxVS;
    delete mNormalizedDepthPS;
    delete mErrorPS;
    delete mVisualizeLightSpacePartitionsPS;
    delete mVisualizeFrustumPS;
    delete mVisualizeFrustumVS;
    delete mVisualizeLightSpaceScatterPS;
    delete mVisualizeLightSpaceScatterVS;
    delete mVisualizeLightSpaceAlphaTestPS;
    delete mVisualizeLightSpacePS;
    delete mVisualizeLightSpaceVS;
    delete mVisualizeHistogramPS;
    delete mBoxBlurPS;
    delete mBoxBlurVS;
    delete mShadowDepthToEVSMPS;
    delete mLightingPS;
    delete mFullScreenTriangleVS;
    delete mShadowAlphaTestPS;
    delete mShadowAlphaTestVS;
    delete mShadowVS;
    delete mGeometryAlphaTestPS;
    delete mGeometryPS;
    delete mGeometryVS;
}


void App::OnD3D11ResizedSwapChain(ID3D11Device* d3dDevice,
                                  const DXGI_SURFACE_DESC* backBufferDesc)
{
    // Create/recreate GBuffer textures
    mGBuffer.clear();
    mGBufferSRV.clear();

    // standard depth buffer
    mDepthBuffer = shared_ptr<Depth2D>(new Depth2D(
        d3dDevice, backBufferDesc->Width, backBufferDesc->Height));

    // same as above, but for use when rendering light space visualization
    mDepthBufferLightSpace = shared_ptr<Depth2D>(new Depth2D(
        d3dDevice, backBufferDesc->Width, backBufferDesc->Height));
    
    // normals
    mGBuffer.push_back(shared_ptr<Texture2D>(new Texture2D(
        d3dDevice, backBufferDesc->Width, backBufferDesc->Height, DXGI_FORMAT_R16G16B16A16_FLOAT)));
    
    // albedo
    mGBuffer.push_back(shared_ptr<Texture2D>(new Texture2D(
        d3dDevice, backBufferDesc->Width, backBufferDesc->Height, DXGI_FORMAT_R8G8B8A8_UNORM)));

    // Recreate error texture
    // Full mip chain for simple average reductions
    mResolutionErrorTexture = shared_ptr<Texture2D>(new Texture2D(
        d3dDevice, backBufferDesc->Width, backBufferDesc->Height, DXGI_FORMAT_R32_FLOAT,
        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, 0));

    // Recreate normalized depth texture
    mNormalizedDepthTexture = shared_ptr<Texture2D>(new Texture2D(
        d3dDevice, backBufferDesc->Width, backBufferDesc->Height, DXGI_FORMAT_R32_FLOAT,
        D3D11_BIND_RENDER_TARGET));


    // Set up GBuffer resource lists
    mGBufferRTV.resize(mGBuffer.size() + 1, 0);     // Includes back buffer as last RTV
    mGBufferSRV.resize(mGBuffer.size() + 1, 0);     // Includes depth buffer as last SRV
    for (std::size_t i = 0; i < mGBuffer.size(); ++i) {
        mGBufferRTV[i] = mGBuffer[i]->GetRenderTarget();
        mGBufferSRV[i] = mGBuffer[i]->GetShaderResource();
    }
    // Back buffer is the last RTV that we use for rendering - will be set each frame
    mGBufferRTV.back() = 0;
    // Depth buffer is the last SRV that we use for reading
    mGBufferSRV.back() = mDepthBuffer->GetShaderResource();
}


void App::Render(ID3D11DeviceContext* d3dDeviceContext, 
                 ID3D11RenderTargetView* backBuffer,
                 CDXUTSDKMesh& mesh_opaque,
                 CDXUTSDKMesh& mesh_alpha,
                 ID3D11ShaderResourceView* skybox,
                 const D3DXMATRIXA16& worldMatrix,
                 const CFirstPersonCamera* viewerCamera,
                 const D3DXVECTOR3& lightDirection,
                 const D3D11_VIEWPORT* viewport,
                 const UIConstants* ui)
{
    D3DXMATRIXA16 cameraProj = *viewerCamera->GetProjMatrix();
    D3DXMATRIXA16 cameraView = *viewerCamera->GetViewMatrix();

    // NOTE: Complementary Z => swap near/far back
    float cameraNear = viewerCamera->GetFarClip();
    float cameraFar = viewerCamera->GetNearClip();
    
    D3DXMATRIXA16 cameraViewInv;
    D3DXMatrixInverse(&cameraViewInv, 0, &cameraView);

    // We only use the view direction from the camera object
    // We then center the directional light on the camera frustum and set the
    // extents to completely cover it.
    D3DXMATRIXA16 lightProj;
    D3DXMATRIXA16 lightView;
    {
        D3DXVECTOR3 eye = -lightDirection;
        D3DXVECTOR3 at(0.0f, 0.0f, 0.0f);
        D3DXVECTOR3 up(0.0f, 1.0f, 0.0f);

        // If requested, align light-space axes to frustum axes
		// NOTE: Cannot do this when quantizing as it will cause texel jitter
		if (ui->alignLightToFrustum && !ui->quantizePartitions) {
            // This ensures that the top and bottom of the frustum projection in
            // light space will align with one axis of the shadow map, which is a reasonable
            // guess at the best oriented bounding box.
            up = *viewerCamera->GetWorldRight();
        }

        // TODO: Handle up ~= lightDirection
        D3DXMatrixLookAtLH(&lightView, &eye, &at, &up);
    }


    {
		// NOTE: We don't include the projection matrix here, since we want to just get back the
        // raw view-space extents and use that to *build* the bounding projection matrix
		D3DXVECTOR3 min, max;

		if (ui->quantizePartitions) {
			// Get the bounding box of the scene
			// TODO: Could cache this somewhere
			D3DXVECTOR3 aabbMin(FLT_MAX, FLT_MAX, FLT_MAX);
			D3DXVECTOR3 aabbMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
			for (UINT i = 0; i < mesh_opaque.GetNumMeshes(); ++i) {
				D3DXVECTOR3 extents = mesh_opaque.GetMeshBBoxExtents(i);
				D3DXVECTOR3 center = mesh_opaque.GetMeshBBoxCenter(i);
				D3DXVECTOR3 meshMin = center - extents;
				D3DXVECTOR3 meshMax = center + extents;
				D3DXVec3Minimize(&aabbMin, &aabbMin, &meshMin);
				D3DXVec3Maximize(&aabbMax, &aabbMax, &meshMax);
			}
			for (UINT i = 0; i < mesh_alpha.GetNumMeshes(); ++i) {
				D3DXVECTOR3 extents = mesh_alpha.GetMeshBBoxExtents(i);
				D3DXVECTOR3 center = mesh_alpha.GetMeshBBoxCenter(i);
				D3DXVECTOR3 meshMin = center - extents;
				D3DXVECTOR3 meshMax = center + extents;
				D3DXVec3Minimize(&aabbMin, &aabbMin, &meshMin);
				D3DXVec3Maximize(&aabbMax, &aabbMax, &meshMax);
			}
			ComputeAABBExtents(aabbMin, aabbMax, lightView, &min, &max);
		} else {
			ComputeFrustumExtents(cameraViewInv, cameraProj, cameraNear, cameraFar, lightView, &min, &max);
		}

        // First adjust the light matrix to be centered on the extents in x/y and behind everything in z
        D3DXVECTOR3 center = 0.5f * (min + max);
        D3DXMATRIXA16 centerTransform;

        D3DXMatrixTranslation(&centerTransform, -center.x, -center.y, -min.z);
        lightView *= centerTransform;

        // Now create a projection matrix that covers the extents when centered
        D3DXVECTOR3 dimensions = max - min;
        D3DXMatrixOrthoLH(&lightProj, dimensions.x, dimensions.y, 0.0f, dimensions.z);
    }


    // Compute composite matrices
    D3DXMATRIXA16 cameraViewProj = cameraView * cameraProj;
    D3DXMATRIXA16 cameraWorldViewProj = worldMatrix * cameraViewProj;
    D3DXMATRIXA16 lightViewProj = lightView * lightProj;
    D3DXMATRIXA16 lightWorldViewProj = worldMatrix * lightViewProj;
    D3DXMATRIXA16 cameraViewToLightProj = cameraViewInv * lightViewProj;

    // Work out blur size in [0,1] light space and the maximum scale we're willing to accept
    // The partition scale clamping ensures that we don't end up with gigantic filter regions
    // due to blurring... i.e. we have *way* too much resolution somewhere given how soft an
    // edge is.
    const float maxFloat = std::numeric_limits<float>::max();
    D3DXVECTOR2 blurSizeLightSpace(0.0f, 0.0f);
    D3DXVECTOR3 maxPartitionScale(maxFloat, maxFloat, maxFloat);
    if (ui->edgeSoftening) {
        blurSizeLightSpace.x = ui->edgeSofteningAmount * 0.5f * lightProj._11;
        blurSizeLightSpace.y = ui->edgeSofteningAmount * 0.5f * lightProj._22;

        float maxBlurLightSpace = ui->maxEdgeSofteningFilter / static_cast<float>(mShadowTextureDim);
        maxPartitionScale.x = maxBlurLightSpace / blurSizeLightSpace.x;
        maxPartitionScale.y = maxBlurLightSpace / blurSizeLightSpace.y;
    }

    // Work out partition border
    D3DXVECTOR3 partitionBorderLightSpace(blurSizeLightSpace.x, blurSizeLightSpace.y, 1.0f);
    partitionBorderLightSpace.z *= lightProj._33;

    // Fill in frame constants
    {
        D3DXVECTOR3 lightDirectionView;
        D3DXVec3TransformNormal(&lightDirectionView, &lightDirection, &cameraView);
        // Camera transform shouldn't include any scaling, so no need to renormalize

        D3D11_MAPPED_SUBRESOURCE mappedResource;
        d3dDeviceContext->Map(mPerFrameConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        PerFrameConstants* constants = static_cast<PerFrameConstants *>(mappedResource.pData);

        constants->mCameraWorldViewProj = cameraWorldViewProj;
        constants->mCameraWorldView = worldMatrix * cameraView;
        constants->mCameraViewProj = cameraViewProj;
        constants->mCameraProj = cameraProj;
        constants->mLightWorldViewProj = lightWorldViewProj;
        constants->mCameraViewToLightProj = cameraViewToLightProj;
        constants->mLightDir = D3DXVECTOR4(lightDirectionView, 0.0f);
        constants->mBlurSizeLightSpace = D3DXVECTOR4(blurSizeLightSpace.x, blurSizeLightSpace.y, 0.0f, 0.0f);
        constants->mCameraNearFar = D3DXVECTOR4(cameraNear, cameraFar, 0.0f, 0.0f);
		constants->mShadowTextureDim = D3DXVECTOR4(float(mShadowTextureDim), float(mShadowTextureDim), 0.0f, 0.0f);

        constants->mUI = *ui;
        
        d3dDeviceContext->Unmap(mPerFrameConstants, 0);
    }

    // Geometry phase
    if (mesh_opaque.IsLoaded()) {
        mesh_opaque.ComputeInFrustumFlags(cameraWorldViewProj);
    }
    if (mesh_alpha.IsLoaded()) {
        mesh_alpha.ComputeInFrustumFlags(cameraWorldViewProj);
    }
    RenderGeometry(d3dDeviceContext, backBuffer, mesh_opaque, mesh_alpha, viewport, ui);
    
    // Generate histogram if needed for the technique, OR if we need the histogram for visualization purposes
    ID3D11ShaderResourceView* histogramSRV = 0;
    if (ui->partitionScheme == KMEANS_PARTITIONS || 
        ui->partitionScheme == LOG_PARTITIONS ||
        ui->partitionScheme == ADAPTIVE_LOG_PARTITIONS ||
        ui->visualizeHistogram) {
        // Assumes integer viewport dimensions that start at 0, 0
        assert(viewport->Width == std::floor(viewport->Width));
        assert(viewport->Height == std::floor(viewport->Height));
        assert(viewport->TopLeftX == 0.0f);
        assert(viewport->TopLeftY == 0.0f);
        
        histogramSRV = mSDSMPartitions->ComputeHistogram(
            d3dDeviceContext,
            static_cast<unsigned int>(mGBufferSRV.size()), &mGBufferSRV.front(),
            mPerFrameConstants,
            static_cast<int>(viewport->Width), static_cast<int>(viewport->Height));
    }

    // Partitioning
    ID3D11ShaderResourceView *partitionSRV = 0;
    if (ui->partitionScheme == KMEANS_PARTITIONS) {
        // K-means clustering
        partitionSRV = mSDSMPartitions->ComputeKMeansPartitions(
            d3dDeviceContext, mPerFrameConstants, partitionBorderLightSpace, maxPartitionScale);
    } else if (ui->partitionScheme == LOG_PARTITIONS) {
        // Log
        partitionSRV = mSDSMPartitions->ComputeLogPartitionsFromHistogram(
            d3dDeviceContext, mPerFrameConstants, partitionBorderLightSpace, maxPartitionScale);
    } else if (ui->partitionScheme == LOG_PARTITIONS_REDUCE) {
        // Log using reduction (rather than histogram)
        partitionSRV = mSDSMPartitions->ComputeLogPartitionsFromGBuffer(d3dDeviceContext,
            static_cast<unsigned int>(mGBufferSRV.size()), &mGBufferSRV.front(),
            mPerFrameConstants, partitionBorderLightSpace, maxPartitionScale,
            static_cast<int>(viewport->Width), static_cast<int>(viewport->Height));
    } else if (ui->partitionScheme == ADAPTIVE_LOG_PARTITIONS) {
        // Adaptive Log
        partitionSRV = mSDSMPartitions->ComputeAdaptiveLogPartitions(
            d3dDeviceContext, mPerFrameConstants, partitionBorderLightSpace, maxPartitionScale);
    } else {
        // Standard PSSM
        partitionSRV = mPSSMPartitions->ComputePartitions(d3dDeviceContext, cameraViewInv, cameraProj,
            cameraNear, cameraFar, lightView, lightProj, ui->pssmFactor,
            partitionBorderLightSpace, ui->quantizePartitions ? mShadowTextureDim : 0);

        // Optionally apply tight bounds using the SDSM histogram
        if (ui->tightPartitionBounds) {
            partitionSRV = mSDSMPartitions->ComputeCustomPartitions(d3dDeviceContext,
                static_cast<unsigned int>(mGBufferSRV.size()), &mGBufferSRV.front(),
                partitionSRV, mPerFrameConstants, partitionBorderLightSpace, maxPartitionScale,
                static_cast<int>(viewport->Width), static_cast<int>(viewport->Height));
        }
    }

    // Make sure one of our implementations filled this out
    assert(partitionSRV);

    // Set up global shadow filtering stuff
    ID3D11ShaderResourceView* shadowSRV = 0;
    ID3D11SamplerState* shadowSampler = 0;
    if (mFilteringScheme == EVSM_FILTERING) {
        shadowSRV = mShadowEVSMTexture->GetShaderResource();
        shadowSampler = mEVSMShadowSampler;
    } else {
        shadowSRV = mShadowDepthTexture->GetShaderResource();
        shadowSampler = mPCFShadowSampler;
    }

    // Do error analysis if requested
    if (ui->visualizeError) {
        mErrorAverageLast = ComputeResolutionError(d3dDeviceContext, backBuffer, viewport, partitionSRV, shadowSRV);
    } else {
        // No error analysis done this frame
        mErrorAverageLast = -1.0f;
    }

    // Save normalized depth if requested
    if (mSaveNormalizedDepth) {
        // Format date/time for file name
        const unsigned int bufferSize = 256;
        wchar_t time[bufferSize];
        wchar_t date[bufferSize];

        GetDateFormat(LOCALE_USER_DEFAULT, 0, NULL, L"yyyy-MM-dd", date, bufferSize);
        GetTimeFormat(LOCALE_USER_DEFAULT, 0, NULL, L"HH-mm-ss", time, bufferSize);

        std::wostringstream fileName;
        fileName << L"sdsm_depth_" << date << L"_" << time << L".dds";
        SaveNormalizedDepth(d3dDeviceContext, viewport, fileName.str());
        mSaveNormalizedDepth = false;
    }

    // If requested, show the light space visualization
    if (ui->visualizeLightSpace) {
        VisualizeLightSpace(d3dDeviceContext, backBuffer, mesh_opaque, mesh_alpha, lightWorldViewProj, viewport, partitionSRV, ui);

    } else {
        // Otherwise normal camera-space rendering

        // Do light-space frustum culling
        // NOTE: We only cull to the *entire* light space here rather than to each partition
        // since we don't know where the partitions are on the CPU.
        // NOTE: Don't cull to near since shadow castors may be closer to the light than
        // the view frustum is.
        if (ui->cpuPartitionFrustumCulling) {
            // Read back partitioning solution and update per-partition matrices
            ReadbackPartitionMatrices(d3dDeviceContext, partitionSRV, lightWorldViewProj);
        } else {
            // TODO: We should maybe dilate the matrix used for culling here as with the partitions
            // to accomodate blurring and such. That said, this is very unlikely to ever pose a problem.
            if (mesh_opaque.IsLoaded()) {
                mesh_opaque.ComputeInFrustumFlags(lightWorldViewProj, false);
            }
            if (mesh_alpha.IsLoaded()) {
                mesh_alpha.ComputeInFrustumFlags(lightWorldViewProj, false);
            }
        }

        for (unsigned int partitionIndex = 0; partitionIndex < mPartitions; ++partitionIndex) {
            unsigned int passPartition = partitionIndex % mPartitionsPerPass;

            // Fill in partition pass constants
            {
                D3D11_MAPPED_SUBRESOURCE mappedResource;
                d3dDeviceContext->Map(mPerPartitionPassConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
                PerPartitionPassConstants* constants = static_cast<PerPartitionPassConstants *>(mappedResource.pData);

                constants->mRenderPartition = partitionIndex;
                constants->mAccumulatePartitionBegin = partitionIndex - passPartition;
                constants->mAccumulatePartitionCount = std::min(mPartitionsPerPass, mPartitions - constants->mAccumulatePartitionBegin);
            
                d3dDeviceContext->Unmap(mPerPartitionPassConstants, 0);
            }

            // Do frustum culling for this partition if requested
            if (ui->cpuPartitionFrustumCulling) {
                if (mesh_opaque.IsLoaded()) {
                    mesh_opaque.ComputeInFrustumFlags(mPartitionWorldViewProj[partitionIndex], false);
                }
                if (mesh_alpha.IsLoaded()) {
                    mesh_alpha.ComputeInFrustumFlags(mPartitionWorldViewProj[partitionIndex], false);
                }
            }

            // Shadow phase
            if (mFilteringScheme == EVSM_FILTERING) {
                // Generate raw depth map
                RenderShadowDepth(d3dDeviceContext, mShadowDepthTexture->GetDepthStencil(),
                    mesh_opaque, mesh_alpha, &mShadowViewport, partitionSRV, ui);

                // Convert single depth map to an EVSM in the proper array slice
                ConvertToEVSM(d3dDeviceContext,
                    mShadowDepthTexture->GetShaderResource(),
                    mShadowEVSMTexture->GetRenderTarget(passPartition),
                    &mShadowViewport,
                    partitionSRV);

                // Optionally blur the EVSM
                if (ui->edgeSoftening > 0) {
                    BoxBlur(d3dDeviceContext, mShadowEVSMTexture, passPartition, mShadowEVSMBlurTexture,
                        partitionIndex, partitionSRV, blurSizeLightSpace);
                }
            
                // Mip levels are generated right before rendering this pass below...

            } else {
                // Generate raw depth into the proper array slice
                RenderShadowDepth(d3dDeviceContext, mShadowDepthTexture->GetDepthStencil(passPartition),
                    mesh_opaque, mesh_alpha, &mShadowViewport, partitionSRV, ui);
            }

            // Lighting accumulation phase
            // Do this if we've filled all of the partitions for this accumulation pass,
            // or if this is the last partition.
            if (passPartition == (mPartitionsPerPass - 1) || partitionIndex == (mPartitions - 1)) {
                if (mFilteringScheme == EVSM_FILTERING) {
                    // Generate mipmaps (for all partitions in the array)
                    d3dDeviceContext->GenerateMips(shadowSRV);
                }

                AccumulateLighting(d3dDeviceContext, backBuffer, viewport, shadowSRV, shadowSampler, partitionSRV);
            }
        }

        // Render skybox if present
        if (skybox) {
            RenderSkybox(d3dDeviceContext, backBuffer, skybox, viewport);
        }
    }

    // Visualize histogram
    if (ui->visualizeHistogram) {
        // Must have generated the histogram to visualize it
        assert(histogramSRV);

        D3D11_VIEWPORT visualizationViewport;
        visualizationViewport.Width = 1024.0f;
        visualizationViewport.Height = 200.0f;
        visualizationViewport.TopLeftX = std::floor(viewport->TopLeftX + 10.0f);
        visualizationViewport.TopLeftY = std::floor(viewport->TopLeftY + viewport->Height - visualizationViewport.Height - 10.0f);
        visualizationViewport.MinDepth = 0.0f;
        visualizationViewport.MaxDepth = 1.0f;

        VisualizeHistogram(d3dDeviceContext, backBuffer, &visualizationViewport, histogramSRV, partitionSRV);
    }
}


void App::RenderGeometry(ID3D11DeviceContext* d3dDeviceContext,
                         ID3D11RenderTargetView* backBuffer,
                         CDXUTSDKMesh& mesh_opaque,
                         CDXUTSDKMesh& mesh_alpha,
                         const D3D11_VIEWPORT* viewport,
                         const UIConstants* ui)
{
    // Clear GBuffer, back buffer and depth buffer
    float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    d3dDeviceContext->ClearRenderTargetView(mGBuffer[0]->GetRenderTarget(), zeros);                // normals
    d3dDeviceContext->ClearRenderTargetView(mGBuffer[1]->GetRenderTarget(), zeros);                // albedo
    d3dDeviceContext->ClearRenderTargetView(backBuffer, zeros);
    // NOTE: Complementary Z buffer: clear to 0 (far)
    d3dDeviceContext->ClearDepthStencilView(mDepthBuffer->GetDepthStencil(), D3D11_CLEAR_DEPTH, 0.0f, 0);

    d3dDeviceContext->IASetInputLayout(mMeshVertexLayout);

    d3dDeviceContext->VSSetConstantBuffers(0, 1, &mPerFrameConstants);
    d3dDeviceContext->VSSetShader(mGeometryVS->GetShader(), 0, 0);
    
    d3dDeviceContext->RSSetViewports(1, viewport);

    d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
    d3dDeviceContext->PSSetSamplers(0, 1, &mDiffuseSampler);
    // Diffuse texture set per-material by DXUT mesh routines
    
    // Set up render GBuffer render targets
    mGBufferRTV.back() = backBuffer;

    d3dDeviceContext->OMSetDepthStencilState(mGeomeryDepthStencilState, 0);
    d3dDeviceContext->OMSetRenderTargets(static_cast<UINT>(mGBufferRTV.size()), &mGBufferRTV.front(), mDepthBuffer->GetDepthStencil());
    d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);
    
    // Render opaque geometry
    if (mesh_opaque.IsLoaded()) {
        // TODO: Some meshes have incorrect winding orders... use no backface culling for now
        //d3dDeviceContext->RSSetState(mRasterizerState);
        d3dDeviceContext->RSSetState(mDoubleSidedRasterizerState);
        d3dDeviceContext->PSSetShader(mGeometryPS->GetShader(), 0, 0);
        mesh_opaque.Render(d3dDeviceContext, 0);
    }

    // Render alpha tested geometry
    if (mesh_alpha.IsLoaded()) {
        d3dDeviceContext->RSSetState(mDoubleSidedRasterizerState);
        d3dDeviceContext->PSSetShader(mGeometryAlphaTestPS->GetShader(), 0, 0);
        mesh_alpha.Render(d3dDeviceContext, 0);
    }

    // Cleanup (aka make the runtime happy)
    d3dDeviceContext->OMSetRenderTargets(0, 0, 0);
}


void App::RenderSkybox(ID3D11DeviceContext* d3dDeviceContext,
                       ID3D11RenderTargetView* backBuffer,
                       ID3D11ShaderResourceView* skybox,
                       const D3D11_VIEWPORT* viewport)
{
    D3D11_VIEWPORT skyboxViewport(*viewport);
    // NOTE: Complementary Z
    skyboxViewport.MinDepth = 0.0f;
    skyboxViewport.MaxDepth = 0.0f;

    d3dDeviceContext->IASetInputLayout(mMeshVertexLayout);

    d3dDeviceContext->VSSetConstantBuffers(0, 1, &mPerFrameConstants);
    d3dDeviceContext->VSSetShader(mSkyboxVS->GetShader(), 0, 0);

    d3dDeviceContext->RSSetState(mDoubleSidedRasterizerState);
    d3dDeviceContext->RSSetViewports(1, &skyboxViewport);

    d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
    d3dDeviceContext->PSSetSamplers(0, 1, &mDiffuseSampler);
    d3dDeviceContext->PSSetShader(mSkyboxPS->GetShader(), 0, 0);

    // Set skybox texture
    d3dDeviceContext->PSSetShaderResources(0, 1, &skybox);

    d3dDeviceContext->OMSetDepthStencilState(mGeomeryDepthStencilState, 0);
    d3dDeviceContext->OMSetRenderTargets(1, &backBuffer, mDepthBuffer->GetDepthStencil());
    d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);
    
    mSkyboxMesh.Render(d3dDeviceContext);

    // Cleanup (aka make the runtime happy)
    d3dDeviceContext->OMSetRenderTargets(0, 0, 0);
    ID3D11ShaderResourceView* nullViews[1] = {0};
    d3dDeviceContext->VSSetShaderResources(0, 1, nullViews);
}



void App::ReadbackPartitionMatrices(ID3D11DeviceContext* d3dDeviceContext,
                                    ID3D11ShaderResourceView* partitionSRV,
                                    const D3DXMATRIXA16& lightWorldViewProj)
{
    // Read back
    ID3D11Resource* partitionResource;
    partitionSRV->GetResource(&partitionResource);
    d3dDeviceContext->CopyResource(mPartitionReadback, partitionResource);
    partitionResource->Release();

    // Map; block for now :S
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    d3dDeviceContext->Map(mPartitionReadback, D3D11CalcSubresource(0, 0, 0), D3D11_MAP_READ,
        0, &mappedResource);
    const Partition* data = static_cast<const Partition*>(mappedResource.pData);
    
    // For each partition, work out the composite matrix
    for (unsigned int i = 0; i < mPartitions; ++i) {
        // See vertex shader math
        const Partition& partition = data[i];
        D3DXMATRIXA16 scale, bias;
        D3DXMatrixScaling(&scale, partition.scale.x, partition.scale.y, partition.scale.z);
        D3DXMatrixTranslation(&bias,
            (2.0f * partition.bias.x + partition.scale.x - 1.0f),
           -(2.0f * partition.bias.y + partition.scale.y - 1.0f),
           partition.bias.z);
        mPartitionWorldViewProj[i] = lightWorldViewProj * scale * bias;
    }

    d3dDeviceContext->Unmap(mPartitionReadback, D3D11CalcSubresource(0, 0, 0));
}


void App::RenderShadowDepth(ID3D11DeviceContext* d3dDeviceContext,
                            ID3D11DepthStencilView* shadowDepth,
                            CDXUTSDKMesh& mesh_opaque,
                            CDXUTSDKMesh& mesh_alpha,
                            const D3D11_VIEWPORT* viewport,
                            ID3D11ShaderResourceView* partitionSRV,
                            const UIConstants* ui)
{
    // Clear shadow depth buffer
    d3dDeviceContext->ClearDepthStencilView(shadowDepth, D3D11_CLEAR_DEPTH, 1.0f, 0);

    d3dDeviceContext->IASetInputLayout(mMeshVertexLayout);

    d3dDeviceContext->VSSetConstantBuffers(0, 1, &mPerFrameConstants);
    d3dDeviceContext->VSSetConstantBuffers(1, 1, &mPerPartitionPassConstants);
    d3dDeviceContext->VSSetShaderResources(4, 1, &partitionSRV);

    d3dDeviceContext->RSSetState(mShadowRasterizerState);
    d3dDeviceContext->RSSetViewports(1, viewport);

    d3dDeviceContext->OMSetDepthStencilState(mShadowDepthStencilState, 0);
    d3dDeviceContext->OMSetRenderTargets(0, 0, shadowDepth);
    d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);

    // Render opaque geometry
    if (mesh_opaque.IsLoaded()) {
        d3dDeviceContext->VSSetShader(mShadowVS->GetShader(), 0, 0);
        d3dDeviceContext->PSSetShader(0, 0, 0);
        mesh_opaque.Render(d3dDeviceContext);
    }

    // Render alpha tested geometry
    if (mesh_alpha.IsLoaded()) {
        d3dDeviceContext->VSSetShader(mShadowAlphaTestVS->GetShader(), 0, 0);
        d3dDeviceContext->PSSetShader(mShadowAlphaTestPS->GetShader(), 0, 0);
        mesh_alpha.Render(d3dDeviceContext, 0);
    }

    // Cleanup (aka make the runtime happy)
    d3dDeviceContext->OMSetRenderTargets(0, 0, 0);        
    ID3D11ShaderResourceView* nullViews[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    d3dDeviceContext->VSSetShaderResources(0, 8, nullViews);
}


void App::ConvertToEVSM(ID3D11DeviceContext* d3dDeviceContext,
                        ID3D11ShaderResourceView* depthInput,
                        ID3D11RenderTargetView* evsmOutput,
                        const D3D11_VIEWPORT* viewport,
                        ID3D11ShaderResourceView* partitionSRV)
{
    // NOTE: This is a 1:1 streaming conversion and we write all pixels, so no need to clear the
    // target EVSM here.

    d3dDeviceContext->IASetInputLayout(0);
    d3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    d3dDeviceContext->IASetVertexBuffers(0, 0, 0, 0, 0);

    d3dDeviceContext->VSSetShader(mFullScreenTriangleVS->GetShader(), 0, 0);

    d3dDeviceContext->RSSetState(mRasterizerState);
    d3dDeviceContext->RSSetViewports(1, viewport);

    d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
    d3dDeviceContext->PSSetConstantBuffers(1, 1, &mPerPartitionPassConstants);
    d3dDeviceContext->PSSetShaderResources(4, 1, &partitionSRV);
    d3dDeviceContext->PSSetShaderResources(0, 1, &depthInput);
    d3dDeviceContext->PSSetShader(mShadowDepthToEVSMPS->GetShader(), 0, 0);

    d3dDeviceContext->OMSetRenderTargets(1, &evsmOutput, 0);
    d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);

    // Full-screen triangle
    d3dDeviceContext->Draw(3, 0);

    // Cleanup (aka make the runtime happy)
    d3dDeviceContext->OMSetRenderTargets(0, 0, 0);
    ID3D11ShaderResourceView* nullViews[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    d3dDeviceContext->PSSetShaderResources(0, 8, nullViews);
}


void App::AccumulateLighting(ID3D11DeviceContext* d3dDeviceContext,
                             ID3D11RenderTargetView* backBuffer,
                             const D3D11_VIEWPORT* viewport,
                             ID3D11ShaderResourceView* shadowSRV,
                             ID3D11SamplerState* shadowSampler,
                             ID3D11ShaderResourceView* partitionSRV)
{
    d3dDeviceContext->IASetInputLayout(0);
    d3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    d3dDeviceContext->IASetVertexBuffers(0, 0, 0, 0, 0);

    d3dDeviceContext->VSSetShader(mFullScreenTriangleVS->GetShader(), 0, 0);

    d3dDeviceContext->RSSetState(mRasterizerState);
    d3dDeviceContext->RSSetViewports(1, viewport);

    d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
    d3dDeviceContext->PSSetConstantBuffers(1, 1, &mPerPartitionPassConstants);
    d3dDeviceContext->PSSetShaderResources(0, static_cast<UINT>(mGBufferSRV.size()), &mGBufferSRV.front());
    d3dDeviceContext->PSSetShaderResources(4, 1, &partitionSRV);
    d3dDeviceContext->PSSetShaderResources(5, 1, &shadowSRV);
    d3dDeviceContext->PSSetSamplers(5, 1, &shadowSampler);
    d3dDeviceContext->PSSetShader(mLightingPS->GetShader(), 0, 0);

    // Additively blend into back buffer
    d3dDeviceContext->OMSetRenderTargets(1, &backBuffer, 0);
    d3dDeviceContext->OMSetBlendState(mLightingBlendState, 0, 0xFFFFFFFF);

    // Full-screen triangle
    d3dDeviceContext->Draw(3, 0);

    // Cleanup (aka make the runtime happy)
    d3dDeviceContext->OMSetRenderTargets(0, 0, 0);
    ID3D11ShaderResourceView* nullViews[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    d3dDeviceContext->PSSetShaderResources(0, 8, nullViews);
}


void App::VisualizeHistogram(ID3D11DeviceContext* d3dDeviceContext,
                             ID3D11RenderTargetView* backBuffer,
                             const D3D11_VIEWPORT* viewport,
                             ID3D11ShaderResourceView* histogramSRV,
                             ID3D11ShaderResourceView* partitionSRV)
{
    d3dDeviceContext->IASetInputLayout(0);
    d3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    d3dDeviceContext->IASetVertexBuffers(0, 0, 0, 0, 0);

    d3dDeviceContext->VSSetShader(mFullScreenTriangleVS->GetShader(), 0, 0);

    d3dDeviceContext->RSSetState(mRasterizerState);
    d3dDeviceContext->RSSetViewports(1, viewport);

    d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
    d3dDeviceContext->PSSetShaderResources(0, 1, &histogramSRV);
    d3dDeviceContext->PSSetShaderResources(4, 1, &partitionSRV);
    d3dDeviceContext->PSSetShader(mVisualizeHistogramPS->GetShader(), 0, 0);

    d3dDeviceContext->OMSetRenderTargets(1, &backBuffer, 0);
    d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);

    // Full-screen triangle
    d3dDeviceContext->Draw(3, 0);

    // Cleanup (aka make the runtime happy)
    d3dDeviceContext->OMSetRenderTargets(0, 0, 0);
    ID3D11ShaderResourceView* nullViews[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    d3dDeviceContext->PSSetShaderResources(0, 8, nullViews);
}


void App::VisualizeLightSpace(ID3D11DeviceContext* d3dDeviceContext,
                              ID3D11RenderTargetView* backBuffer,
                              CDXUTSDKMesh& mesh_opaque,
                              CDXUTSDKMesh& mesh_alpha,
                              const D3DXMATRIXA16& lightWorldViewProj,
                              const D3D11_VIEWPORT* viewport,
                              ID3D11ShaderResourceView* partitionSRV,
                              const UIConstants* ui)
{
    // Render light space geometry into back buffer

    // Modify viewport to maintain 1:1 aspect and center in view.
    D3D11_VIEWPORT viewportCentered = *viewport;
    float aspectDifference = viewportCentered.Width - viewportCentered.Height;
    if (aspectDifference > 0.0f) {
        viewportCentered.TopLeftX += 0.5f * aspectDifference;
        viewportCentered.Width = viewportCentered.Height;
    } else if (aspectDifference < 0.0f) {
        viewportCentered.TopLeftY -= 0.5f * aspectDifference;
        viewportCentered.Height = viewportCentered.Width;
    }

    // Clear back buffer and visualize depth buffer
    float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    d3dDeviceContext->ClearRenderTargetView(backBuffer, zeros);
    d3dDeviceContext->ClearDepthStencilView(mDepthBufferLightSpace->GetDepthStencil(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    d3dDeviceContext->IASetInputLayout(mMeshVertexLayout);

    d3dDeviceContext->VSSetConstantBuffers(0, 1, &mPerFrameConstants);
    d3dDeviceContext->VSSetShader(mVisualizeLightSpaceVS->GetShader(), 0, 0);
    
    d3dDeviceContext->RSSetViewports(1, &viewportCentered);

    d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
    d3dDeviceContext->PSSetSamplers(0, 1, &mDiffuseSampler);
    // Diffuse texture set per-material by DXUT mesh routines
    
    d3dDeviceContext->OMSetDepthStencilState(mShadowDepthStencilState, 0);
    d3dDeviceContext->OMSetRenderTargets(1, &backBuffer, mDepthBufferLightSpace->GetDepthStencil());
    d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);
    
    // Render opaque geometry
    if (mesh_opaque.IsLoaded()) {
        mesh_opaque.ComputeInFrustumFlags(lightWorldViewProj, false);
        
        //d3dDeviceContext->RSSetState(mRasterizerState);
        d3dDeviceContext->RSSetState(mDoubleSidedRasterizerState);
        d3dDeviceContext->PSSetShader(mVisualizeLightSpacePS->GetShader(), 0, 0);
        mesh_opaque.Render(d3dDeviceContext, 0);
    }

    // Render alpha tested geometry
    if (mesh_alpha.IsLoaded()) {
        mesh_alpha.ComputeInFrustumFlags(lightWorldViewProj, false);

        d3dDeviceContext->RSSetState(mDoubleSidedRasterizerState);
        d3dDeviceContext->PSSetShader(mVisualizeLightSpaceAlphaTestPS->GetShader(), 0, 0);
        mesh_alpha.Render(d3dDeviceContext, 0);
    }


    // Now scatter the required light space samples from the G-buffer
    d3dDeviceContext->IASetInputLayout(0);
    d3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

    d3dDeviceContext->VSSetShader(mVisualizeLightSpaceScatterVS->GetShader(), 0, 0);
    d3dDeviceContext->VSSetShaderResources(0, static_cast<UINT>(mGBufferSRV.size()), &mGBufferSRV.front());
    
    d3dDeviceContext->PSSetShader(mVisualizeLightSpaceScatterPS->GetShader(), 0, 0);

    // No longer need depth buffer
    d3dDeviceContext->OMSetRenderTargets(1, &backBuffer, 0);
    // Conveniently uses additive blending
    d3dDeviceContext->OMSetBlendState(mLightingBlendState, 0, 0xFFFFFFFF);

    // Draw one point for every framebuffer pixel
    unsigned int samples = static_cast<unsigned int>(viewport->Width) * static_cast<unsigned int>(viewport->Height);
    d3dDeviceContext->Draw(samples, 0);
    

    // Draw camera frustum
    d3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    d3dDeviceContext->VSSetConstantBuffers(0, 1, &mPerFrameConstants);
    d3dDeviceContext->VSSetShaderResources(4, 1, &partitionSRV);

    d3dDeviceContext->VSSetShader(mVisualizeFrustumVS->GetShader(), 0, 0);    

    d3dDeviceContext->PSSetShader(mVisualizeFrustumPS->GetShader(), 0, 0);
    d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);

    // 8 vertices for each Z-plane (partitions + 1 splits) and 8 for the side frustum lines
    d3dDeviceContext->Draw(8 * (mPartitions + 2), 0);


    // If requested, visualize partitions
    if (ui->visualizePartitions) {
        d3dDeviceContext->IASetInputLayout(0);
        d3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        d3dDeviceContext->IASetVertexBuffers(0, 0, 0, 0, 0);

        d3dDeviceContext->VSSetShader(mFullScreenTriangleVS->GetShader(), 0, 0);
        
        d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
        d3dDeviceContext->PSSetShaderResources(4, 1, &partitionSRV);
        d3dDeviceContext->PSSetShader(mVisualizeLightSpacePartitionsPS->GetShader(), 0, 0);

        d3dDeviceContext->OMSetBlendState(mAlphaBlendState, 0, 0xFFFFFFFF);

        // Full screen triangle
        d3dDeviceContext->Draw(3, 0);
    }


    // Cleanup (aka make the runtime happy)
    {
        d3dDeviceContext->OMSetRenderTargets(0, 0, 0);
        ID3D11ShaderResourceView* nullViews[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        d3dDeviceContext->VSSetShaderResources(0, 8, nullViews);
        d3dDeviceContext->PSSetShaderResources(0, 8, nullViews);
    }

}


float App::ComputeResolutionError(ID3D11DeviceContext* d3dDeviceContext,
                                  ID3D11RenderTargetView* backBuffer,
                                  const D3D11_VIEWPORT* viewport,
                                  ID3D11ShaderResourceView* partitionSRV,
                                  ID3D11ShaderResourceView* shadowSRV)
{
    // Clear error texture
    ID3D11RenderTargetView* errorTexture = mResolutionErrorTexture->GetRenderTarget();
    const float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    d3dDeviceContext->ClearRenderTargetView(errorTexture, clearColor);

    d3dDeviceContext->IASetInputLayout(0);
    d3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    d3dDeviceContext->IASetVertexBuffers(0, 0, 0, 0, 0);

    d3dDeviceContext->VSSetShader(mFullScreenTriangleVS->GetShader(), 0, 0);

    d3dDeviceContext->RSSetState(mRasterizerState);
    d3dDeviceContext->RSSetViewports(1, viewport);

    d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
    d3dDeviceContext->PSSetShaderResources(0, static_cast<UINT>(mGBufferSRV.size()), &mGBufferSRV.front());
    d3dDeviceContext->PSSetShaderResources(4, 1, &partitionSRV);
    d3dDeviceContext->PSSetShaderResources(5, 1, &shadowSRV);
    d3dDeviceContext->PSSetShader(mErrorPS->GetShader(), 0, 0);

    d3dDeviceContext->OMSetRenderTargets(1, &errorTexture, 0);
    d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);

    // Full-screen triangle
    d3dDeviceContext->Draw(3, 0);

    // Cleanup (aka make the runtime happy)
    {
        d3dDeviceContext->OMSetRenderTargets(0, 0, 0);
        ID3D11ShaderResourceView* nullViews[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        d3dDeviceContext->PSSetShaderResources(0, 8, nullViews);
    }

    // Generate mipmaps (average error across screen)
    d3dDeviceContext->GenerateMips(mResolutionErrorTexture->GetShaderResource());

    // Read back 1x1 mipmap level
    D3D11_TEXTURE2D_DESC desc;
    mResolutionErrorTexture->GetTexture()->GetDesc(&desc);
    int lastMipLevel = desc.MipLevels - 1;

    d3dDeviceContext->CopySubresourceRegion(
        mErrorReadbackTexture, D3D11CalcSubresource(0, 0, 0), 0, 0, 0,
        mResolutionErrorTexture->GetTexture(), D3D11CalcSubresource(lastMipLevel, 0, lastMipLevel), 0);

    // Do the bad thing and just stall here to read it back... works well enough for our purposes
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    d3dDeviceContext->Map(mErrorReadbackTexture, D3D11CalcSubresource(0, 0, 0), D3D11_MAP_READ,
        0, &mappedResource);
    const float* data = static_cast<const float*>(mappedResource.pData);
    float errorAverage = data[0];
    d3dDeviceContext->Unmap(mErrorReadbackTexture, D3D11CalcSubresource(0, 0, 0));
   
    return errorAverage;
}


void App::SaveNormalizedDepth(ID3D11DeviceContext* d3dDeviceContext,
                              const D3D11_VIEWPORT* viewport,
                              const std::wstring& fileName)
{
    ID3D11RenderTargetView* depthTexture = mNormalizedDepthTexture->GetRenderTarget();

    d3dDeviceContext->IASetInputLayout(0);
    d3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    d3dDeviceContext->IASetVertexBuffers(0, 0, 0, 0, 0);

    d3dDeviceContext->VSSetShader(mFullScreenTriangleVS->GetShader(), 0, 0);

    d3dDeviceContext->RSSetState(mRasterizerState);
    d3dDeviceContext->RSSetViewports(1, viewport);

    d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
    d3dDeviceContext->PSSetShaderResources(0, static_cast<UINT>(mGBufferSRV.size()), &mGBufferSRV.front());
    d3dDeviceContext->PSSetShader(mNormalizedDepthPS->GetShader(), 0, 0);

    d3dDeviceContext->OMSetRenderTargets(1, &depthTexture, 0);
    d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);

    // Full-screen triangle
    d3dDeviceContext->Draw(3, 0);

    // Cleanup (aka make the runtime happy)
    {
        d3dDeviceContext->OMSetRenderTargets(0, 0, 0);
        ID3D11ShaderResourceView* nullViews[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        d3dDeviceContext->PSSetShaderResources(0, 8, nullViews);
    }

    D3DX11SaveTextureToFile(d3dDeviceContext, mNormalizedDepthTexture->GetTexture(), D3DX11_IFF_DDS, fileName.c_str());
}


void App::BoxBlurPass(ID3D11DeviceContext* d3dDeviceContext,
                      ID3D11ShaderResourceView* input,
                      ID3D11RenderTargetView* output,
                      unsigned int partitionIndex,
                      ID3D11ShaderResourceView* partitionSRV,
                      const D3D11_VIEWPORT* viewport,
                      const D3DXVECTOR2& filterSize,
                      unsigned int dimension)
{
    // Fill in constants
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        d3dDeviceContext->Map(mBoxBlurConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        BoxBlurConstants* constants = static_cast<BoxBlurConstants *>(mappedResource.pData);

        constants->mFilterSize = filterSize;
        constants->mPartition = partitionIndex;
        constants->mDimension = dimension;
        
        d3dDeviceContext->Unmap(mBoxBlurConstants, 0);
    }
 
    d3dDeviceContext->IASetInputLayout(0);
    d3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    d3dDeviceContext->IASetVertexBuffers(0, 0, 0, 0, 0);

    d3dDeviceContext->VSSetConstantBuffers(0, 1, &mBoxBlurConstants);
    d3dDeviceContext->VSSetShaderResources(0, 1, &input);
    d3dDeviceContext->VSSetShaderResources(1, 1, &partitionSRV);
    d3dDeviceContext->VSSetShader(mBoxBlurVS->GetShader(), 0, 0);

    d3dDeviceContext->RSSetState(mRasterizerState);
    d3dDeviceContext->RSSetViewports(1, viewport);

    d3dDeviceContext->PSSetConstantBuffers(0, 1, &mBoxBlurConstants);
    d3dDeviceContext->PSSetShaderResources(0, 1, &input);
    d3dDeviceContext->PSSetShader(mBoxBlurPS->GetShader(), 0, 0);

    d3dDeviceContext->OMSetRenderTargets(1, &output, 0);
    d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);

    // Full-screen triangle
    d3dDeviceContext->Draw(3, 0);

    // Cleanup (aka make the runtime happy)
    d3dDeviceContext->OMSetRenderTargets(0, 0, 0);
    ID3D11ShaderResourceView* nullViews[2] = {0, 0};
    d3dDeviceContext->VSSetShaderResources(0, 2, nullViews);
    d3dDeviceContext->PSSetShaderResources(0, 2, nullViews);
}


void App::BoxBlur(ID3D11DeviceContext* d3dDeviceContext,
                  std::tr1::shared_ptr<Texture2D> texture,
                  unsigned int textureElement,
                  std::tr1::shared_ptr<Texture2D> temp,
                  unsigned int partitionIndex,
                  ID3D11ShaderResourceView* partitionSRV,
                  const D3DXVECTOR2& filterSize)
{
    // Get some info about the texture
    D3D11_TEXTURE2D_DESC textureDesc;
    texture->GetTexture()->GetDesc(&textureDesc);

    // Viewport
    D3D11_VIEWPORT viewport;
    viewport.Width = static_cast<float>(textureDesc.Width);
    viewport.Height = static_cast<float>(textureDesc.Height);
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    // Horizontal pass
    BoxBlurPass(d3dDeviceContext, texture->GetShaderResource(textureElement), temp->GetRenderTarget(0),
        partitionIndex, partitionSRV, &viewport, filterSize, 0);

    // Vertical pass
    BoxBlurPass(d3dDeviceContext, temp->GetShaderResource(0), texture->GetRenderTarget(textureElement),
        partitionIndex, partitionSRV, &viewport, filterSize, 1);
}
