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

#pragma once

#include "DXUT.h"
#include "DXUTcamera.h"
#include "SDKMesh.h"
#include "Texture2D.h"
#include "SDSMPartitions.h"
#include "PSSMPartitions.h"
#include <vector>
#include <memory>

enum FilteringScheme {
    EVSM_FILTERING,
    NEIGHBOR_PCF_FILTERING,
};

// NOTE: Must match shader equivalent structure
__declspec(align(16))
struct UIConstants
{
    unsigned int lightingOnly;
    unsigned int faceNormals;
    unsigned int partitionScheme;
    unsigned int visualizePartitions;
    unsigned int visualizeHistogram;
    unsigned int visualizeError;
    unsigned int edgeSoftening;
    float edgeSofteningAmount;
    float maxEdgeSofteningFilter;
    float pssmFactor;
    unsigned int alignLightToFrustum;
    unsigned int cpuPartitionFrustumCulling;
    unsigned int tightPartitionBounds;
    float histogramResolutionPower;
    unsigned int lloydIterations;
    unsigned int usePositiveExponent;
    float positiveExponent;
    unsigned int useNegativeExponent;
    float negativeExponent;
    unsigned int visualizeLightSpace;
	unsigned int quantizePartitions;
};


class App
{
public:
    App(ID3D11Device* d3dDevice,
        unsigned int partitions, unsigned int partitionsPerPass,
        unsigned int shadowTextureDim, unsigned int shadowAASamples,
        bool shadowTransparencyAA,
        FilteringScheme filteringScheme);

    ~App();
    
    void OnD3D11ResizedSwapChain(ID3D11Device* d3dDevice,
                                 const DXGI_SURFACE_DESC* backBufferDesc);

    // NOTE:
    // - lightDirection should be normalized
    void Render(ID3D11DeviceContext* d3dDeviceContext,
                ID3D11RenderTargetView* backBuffer,
                CDXUTSDKMesh& mesh_opaque,
                CDXUTSDKMesh& mesh_alpha,
                ID3D11ShaderResourceView* skybox,
                const D3DXMATRIXA16& worldMatrix,
                const CFirstPersonCamera* viewerCamera,
                const D3DXVECTOR3& lightDirection,
                const D3D11_VIEWPORT* viewport,
                const UIConstants* ui);

    float GetAverageError() const { return mErrorAverageLast; }

    void SaveNextNormalizedDepth() { mSaveNormalizedDepth = true; }

private:
	// No copying
	App(const App&);
	App& operator=(const App&);

    // Notes: 
    // - Most of these functions should all be called after initializing per frame/pass constants, etc.
    //   as the shaders that they invoke bind those constant buffers.

    // Draws geometry into G-buffer (and ambient into back buffer)
    void RenderGeometry(ID3D11DeviceContext* d3dDeviceContext,
                        ID3D11RenderTargetView* backBuffer,
                        CDXUTSDKMesh& mesh_opaque,
                        CDXUTSDKMesh& mesh_alpha,
                        const D3D11_VIEWPORT* viewport,
                        const UIConstants* ui);

    void RenderSkybox(ID3D11DeviceContext* d3dDeviceContext,
                      ID3D11RenderTargetView* backBuffer,
                      ID3D11ShaderResourceView* skybox,
                      const D3D11_VIEWPORT* viewport);

    // Reads back partition solution to the CPU and computes composite light world-view-projection
    // matrices for each partition
    void ReadbackPartitionMatrices(ID3D11DeviceContext* d3dDeviceContext,
                                   ID3D11ShaderResourceView* partitionSRV,
                                   const D3DXMATRIXA16& lightWorldViewProj);

    // Renders linear depth for the current partition (set via pass constants)
    // into the given depth render target
    void RenderShadowDepth(ID3D11DeviceContext* d3dDeviceContext,
                           ID3D11DepthStencilView* shadowDepth,
                           CDXUTSDKMesh& mesh_opaque,
                           CDXUTSDKMesh& mesh_alpha,
                           const D3D11_VIEWPORT* viewport,
                           ID3D11ShaderResourceView* partitionSRV,
                           const UIConstants* ui);

    // Accumulates lighting into the back buffer for the current partition set
    void AccumulateLighting(ID3D11DeviceContext* d3dDeviceContext,
                            ID3D11RenderTargetView* backBuffer,
                            const D3D11_VIEWPORT* viewport,
                            ID3D11ShaderResourceView* shadowSRV,
                            ID3D11SamplerState* shadowSampler,
                            ID3D11ShaderResourceView* partitionSRV);

    // Generate an EVSM from a raw shadow depth map for the current partition
    void ConvertToEVSM(ID3D11DeviceContext* d3dDeviceContext,
                       ID3D11ShaderResourceView* depthInput,
                       ID3D11RenderTargetView* evsmOutput,
                       const D3D11_VIEWPORT* viewport,
                       ID3D11ShaderResourceView* partitionSRV);

    // Draws histogram visualization overlay into the given viewport
    void VisualizeHistogram(ID3D11DeviceContext* d3dDeviceContext,
                            ID3D11RenderTargetView* backBuffer,
                            const D3D11_VIEWPORT* viewport,
                            ID3D11ShaderResourceView* histogramSRV,
                            ID3D11ShaderResourceView* partitionSRV);

    // Draws light space shadow sample visualization
    void VisualizeLightSpace(ID3D11DeviceContext* d3dDeviceContext,
                             ID3D11RenderTargetView* backBuffer,
                             CDXUTSDKMesh& mesh_opaque,
                             CDXUTSDKMesh& mesh_alpha,
                             const D3DXMATRIXA16& lightWorldViewProj,
                             const D3D11_VIEWPORT* viewport,
                             ID3D11ShaderResourceView* partitionSRV,
                             const UIConstants* ui);

    // Computes average resolution error using the given partitioning and shadow map
    // NOTE: Only the resolution of the shadow map is used/needed; the data need not be meaningful
    float ComputeResolutionError(ID3D11DeviceContext* d3dDeviceContext,
                                 ID3D11RenderTargetView* backBuffer,
                                 const D3D11_VIEWPORT* viewport,
                                 ID3D11ShaderResourceView* partitionSRV,
                                 ID3D11ShaderResourceView* shadowSRV);

    // Normalizes and saves view space Z (linear depth)
    void SaveNormalizedDepth(ID3D11DeviceContext* d3dDeviceContext,
                             const D3D11_VIEWPORT* viewport,
                             const std::wstring& fileName);

    void BoxBlurPass(ID3D11DeviceContext* d3dDeviceContext,
                     ID3D11ShaderResourceView* input,
                     ID3D11RenderTargetView* output,
                     unsigned int partitionIndex,
                     ID3D11ShaderResourceView* partitionSRV,
                     const D3D11_VIEWPORT* viewport,
                     const D3DXVECTOR2& filterSize,
                     unsigned int dimension);

    void BoxBlur(ID3D11DeviceContext* d3dDeviceContext,
                 std::tr1::shared_ptr<Texture2D> texture,
                 unsigned int textureElement,
                 std::tr1::shared_ptr<Texture2D> temp,
                 unsigned int partitionIndex,
                 ID3D11ShaderResourceView* partitionSRV,
                 const D3DXVECTOR2& filterSize);

    unsigned int mPartitions;
    unsigned int mPartitionsPerPass;
    unsigned int mShadowTextureDim;
    unsigned int mShadowAASamples;
    FilteringScheme mFilteringScheme;

    ID3D11InputLayout* mMeshVertexLayout;

    VertexShader* mGeometryVS;
    PixelShader* mGeometryPS;
    PixelShader* mGeometryAlphaTestPS;

    CDXUTSDKMesh mSkyboxMesh;
    VertexShader* mSkyboxVS;
    PixelShader* mSkyboxPS;

    VertexShader* mShadowVS;
    VertexShader* mShadowAlphaTestVS;
    PixelShader* mShadowAlphaTestPS;

    VertexShader* mFullScreenTriangleVS;
    PixelShader* mLightingPS;

    // Additional EVSM shaders
    PixelShader* mShadowDepthToEVSMPS;
    VertexShader* mBoxBlurVS;
    PixelShader* mBoxBlurPS;

    PixelShader* mVisualizeHistogramPS;

    VertexShader* mVisualizeLightSpaceVS;
    PixelShader* mVisualizeLightSpacePS;
    PixelShader* mVisualizeLightSpaceAlphaTestPS;

    VertexShader* mVisualizeLightSpaceScatterVS;
    PixelShader* mVisualizeLightSpaceScatterPS;

    VertexShader* mVisualizeFrustumVS;
    PixelShader* mVisualizeFrustumPS;

    PixelShader* mVisualizeLightSpacePartitionsPS;

    ID3D11Buffer* mPerFrameConstants;
    ID3D11Buffer* mPerPartitionPassConstants;
    ID3D11Buffer* mBoxBlurConstants;

    ID3D11RasterizerState* mRasterizerState;
    ID3D11RasterizerState* mDoubleSidedRasterizerState;
    ID3D11RasterizerState* mShadowRasterizerState;

    ID3D11DepthStencilState* mGeomeryDepthStencilState;     // Uses complementary Z in flipped direction
    ID3D11DepthStencilState* mShadowDepthStencilState;      // Uses linear Z (ortho) in standard direction

    ID3D11BlendState* mGeometryBlendState;
    ID3D11BlendState* mLightingBlendState;
    ID3D11BlendState* mAlphaBlendState;

    ID3D11SamplerState* mDiffuseSampler;
    ID3D11SamplerState* mEVSMShadowSampler;
    ID3D11SamplerState* mPCFShadowSampler;

    std::vector< std::tr1::shared_ptr<Texture2D> > mGBuffer;
    // Handy cache of list of RT pointers for G-buffer
    std::vector<ID3D11RenderTargetView*> mGBufferRTV;
    // Handy cache of list of SRV pointers for the G-buffer
    std::vector<ID3D11ShaderResourceView*> mGBufferSRV;

    std::tr1::shared_ptr<Depth2D> mDepthBuffer;

    // A second depth buffer used for visualizing light space
    // TODO: Could maybe use one of the shadow maps for this, but depends on resolution
    std::tr1::shared_ptr<Depth2D> mDepthBufferLightSpace;

    // Partitioning implementations
    SDSMPartitions *mSDSMPartitions;
    PSSMPartitions *mPSSMPartitions;

    // Partition world-view-projection matrices when read back to the CPU
    ID3D11Buffer* mPartitionReadback;
    std::vector<D3DXMATRIX> mPartitionWorldViewProj;

    // Basic shadow map (raw depth buffer)
    D3D11_VIEWPORT mShadowViewport;
    std::tr1::shared_ptr<Depth2D> mShadowDepthTexture;
    std::tr1::shared_ptr<Texture2D> mShadowEVSMTexture;
    std::tr1::shared_ptr<Texture2D> mShadowEVSMBlurTexture;

    // Error reporting and optimization
    std::tr1::shared_ptr<Texture2D> mResolutionErrorTexture;
    PixelShader* mErrorPS;
    ID3D11Texture2D* mErrorReadbackTexture;
    float mErrorAverageLast;

    // Output of raw normalized depth
    std::tr1::shared_ptr<Texture2D> mNormalizedDepthTexture;
    PixelShader* mNormalizedDepthPS;
    bool mSaveNormalizedDepth;
};
