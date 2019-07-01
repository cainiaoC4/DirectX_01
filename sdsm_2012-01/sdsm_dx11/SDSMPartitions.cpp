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

#include "SDSMPartitions.h"
#include <d3dx11.h>
#include <assert.h>
#include <limits>
#include <sstream>


static void UnbindResources(ID3D11DeviceContext *d3dDeviceContext)
{
    ID3D11ShaderResourceView *dummySRV[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    d3dDeviceContext->CSSetShaderResources(0, 8, dummySRV);
    ID3D11UnorderedAccessView *dummyUAV[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    d3dDeviceContext->CSSetUnorderedAccessViews(0, 8, dummyUAV, 0);
}


SDSMPartitions::SDSMPartitions(ID3D11Device *d3dDevice, int partitions, int bins, int shadowTextureDim)
    : mBins(bins), mPartitions(partitions)
    , mHistogram(d3dDevice, bins)
    , mPartitionBuffer(d3dDevice, partitions)
    , mPartitionBounds(d3dDevice, partitions)
{	
    // Set up shader defines
    std::string binStr;
    {
        std::ostringstream oss;
        oss << mBins;
        binStr = oss.str();
    }
    std::string partitionsStr;
    {
        std::ostringstream oss;
        oss << mPartitions;
        partitionsStr = oss.str();
    }

    D3D10_SHADER_MACRO defines[] = {
        {"BINS", binStr.c_str()},
        {"PARTITIONS", partitionsStr.c_str()},
        {0, 0}
    };
    
    // Create compute shaders
    mClearHistogramCS = new ComputeShader(d3dDevice, L"SDSMPartitions.hlsl", "ClearHistogram", defines);
    mScatterHistogramCS = new ComputeShader(d3dDevice, L"SDSMPartitions.hlsl", "ScatterHistogram", defines);

    mClearZBoundsCS = new ComputeShader(d3dDevice, L"LogPartitions.hlsl", "ClearZBounds", defines);
    mReduceZBoundsFromGBufferCS = new ComputeShader(d3dDevice, L"LogPartitions.hlsl", "ReduceZBoundsFromGBuffer", defines);

    mSDSMPartitionsCS = new ComputeShader(d3dDevice, L"KMeansPartitions.hlsl", "ComputeKMeansPartitions", defines);
    mLogPartitionsFromHistogramCS = new ComputeShader(d3dDevice, L"LogPartitions.hlsl", "ComputeLogPartitionsFromHistogram", defines);
    mLogPartitionsFromZBoundsCS = new ComputeShader(d3dDevice, L"LogPartitions.hlsl", "ComputeLogPartitionsFromZBounds", defines);
    mAdaptiveLogPartitionsCS = new ComputeShader(d3dDevice, L"AdaptiveLogPartitions.hlsl", "ComputeAdaptiveLogPartitions", defines);        
    mCustomPartitionsCS = new ComputeShader(d3dDevice, L"CustomPartitions.hlsl", "ComputeCustomPartitions", defines);

    mClearPartitionBoundsCS = new ComputeShader(d3dDevice, L"CustomPartitions.hlsl", "ClearPartitionBounds", defines);
    mReduceBoundsFromGBufferCS = new ComputeShader(d3dDevice, L"CustomPartitions.hlsl", "ReduceBoundsFromGBuffer", defines);
    
    // Setup constant buffers
    {
        CD3D11_BUFFER_DESC desc(
            sizeof(SDSMPartitionsConstants),
            D3D11_BIND_CONSTANT_BUFFER,
            D3D11_USAGE_DYNAMIC,
            D3D11_CPU_ACCESS_WRITE);

        d3dDevice->CreateBuffer(&desc, 0, &mSDSMPartitionsConstants);
    }

    // Set default constants
    mCurrentConstants.mScatterTileDim = 64;

    // TODO: This needs tweaking per resolution/GPU... we want to set it up so that there are approximately
    // as many tiles on the screen as there are cores on the GPU.
    mCurrentConstants.mReduceTileDim = 128;

    // ~10 texel dilation on partition borders to cover standard filtering (mipmapping/aniso) and any quantization.
    // Blur kernels are handled separately so this should stay small or can even be removed in some scenes.
    mCurrentConstants.mDilationFactor = 10.0f / float(shadowTextureDim);
}


SDSMPartitions::~SDSMPartitions()
{
    mSDSMPartitionsConstants->Release();
    
    delete mClearPartitionBoundsCS;
    delete mReduceBoundsFromGBufferCS;

    delete mCustomPartitionsCS;
    delete mLogPartitionsFromHistogramCS;
    delete mLogPartitionsFromZBoundsCS;
    delete mAdaptiveLogPartitionsCS;
    delete mSDSMPartitionsCS;

    delete mClearZBoundsCS;
    delete mReduceZBoundsFromGBufferCS;

    delete mClearHistogramCS;
    delete mScatterHistogramCS;
}


ID3D11ShaderResourceView* SDSMPartitions::ComputeHistogram(
    ID3D11DeviceContext *d3dDeviceContext,
    unsigned int gbufferTexturesNum, ID3D11ShaderResourceView** gbufferTextures,
    ID3D11Buffer* perFrameConstants,
    int screenWidth, int screenHeight)
{
    UpdateShaderConstants(d3dDeviceContext);

    // Setup constant buffers (these are global for all shaders)
    d3dDeviceContext->CSSetConstantBuffers(0, 1, &perFrameConstants);
    d3dDeviceContext->CSSetConstantBuffers(1, 1, &mSDSMPartitionsConstants);

    // Initialize histogram
    ID3D11UnorderedAccessView* histogramUAV = mHistogram.GetUnorderedAccess();    
	d3dDeviceContext->CSSetShader(mClearHistogramCS->GetShader(), 0, 0);
	d3dDeviceContext->CSSetUnorderedAccessViews(5, 1, &histogramUAV, 0);
	d3dDeviceContext->Dispatch(mBins, 1, 1);

	// Scatter
    int dispatchWidth = (screenWidth + mCurrentConstants.mScatterTileDim - 1) / mCurrentConstants.mScatterTileDim;
    int dispatchHeight = (screenHeight + mCurrentConstants.mScatterTileDim - 1) / mCurrentConstants.mScatterTileDim;

    d3dDeviceContext->CSSetShader(mScatterHistogramCS->GetShader(), 0, 0);
    d3dDeviceContext->CSSetShaderResources(0, gbufferTexturesNum, gbufferTextures);
	d3dDeviceContext->CSSetUnorderedAccessViews(5, 1, &histogramUAV, 0);
	d3dDeviceContext->Dispatch(dispatchWidth, dispatchHeight, 1);

    UnbindResources(d3dDeviceContext);

    return mHistogram.GetShaderResource();
}


ID3D11ShaderResourceView* SDSMPartitions::ComputeKMeansPartitions(
    ID3D11DeviceContext *d3dDeviceContext,
    ID3D11Buffer* perFrameConstants,
    const D3DXVECTOR3& lightSpaceBorder,
    const D3DXVECTOR3& maxScale)
{
    // Update relevant constant data
    mCurrentConstants.mLightSpaceBorder = D3DXVECTOR4(lightSpaceBorder, 0.0f);
    mCurrentConstants.mMaxScale = D3DXVECTOR4(maxScale, 0.0f);
    UpdateShaderConstants(d3dDeviceContext);

    // Setup constant buffers (these are global for all shaders)
    d3dDeviceContext->CSSetConstantBuffers(0, 1, &perFrameConstants);
    d3dDeviceContext->CSSetConstantBuffers(1, 1, &mSDSMPartitionsConstants);
    
    // Generate partitions
    ID3D11ShaderResourceView* histogramSRV = mHistogram.GetShaderResource();
    ID3D11UnorderedAccessView* partitionUAV = mPartitionBuffer.GetUnorderedAccess();
    d3dDeviceContext->CSSetShader(mSDSMPartitionsCS->GetShader(), 0, 0);
    d3dDeviceContext->CSSetShaderResources(5, 1, &histogramSRV);
	d3dDeviceContext->CSSetUnorderedAccessViews(6, 1, &partitionUAV, 0);
	d3dDeviceContext->Dispatch(1, 1, 1);

    UnbindResources(d3dDeviceContext);

    return mPartitionBuffer.GetShaderResource();
}


ID3D11ShaderResourceView* SDSMPartitions::ComputeLogPartitionsFromHistogram(
    ID3D11DeviceContext *d3dDeviceContext,
    ID3D11Buffer* perFrameConstants,
    const D3DXVECTOR3& lightSpaceBorder,
    const D3DXVECTOR3& maxScale)
{
    // Update relevant constant data
    mCurrentConstants.mLightSpaceBorder = D3DXVECTOR4(lightSpaceBorder, 0.0f);
    mCurrentConstants.mMaxScale = D3DXVECTOR4(maxScale, 0.0f);
    UpdateShaderConstants(d3dDeviceContext);

    // Setup constant buffers (these are global for all shaders)
    d3dDeviceContext->CSSetConstantBuffers(0, 1, &perFrameConstants);
    d3dDeviceContext->CSSetConstantBuffers(1, 1, &mSDSMPartitionsConstants);
    
    // Generate partitions
    ID3D11ShaderResourceView* histogramSRV = mHistogram.GetShaderResource();
    ID3D11UnorderedAccessView* partitionUAV = mPartitionBuffer.GetUnorderedAccess();
    d3dDeviceContext->CSSetShader(mLogPartitionsFromHistogramCS->GetShader(), 0, 0);
    d3dDeviceContext->CSSetShaderResources(5, 1, &histogramSRV);
	d3dDeviceContext->CSSetUnorderedAccessViews(6, 1, &partitionUAV, 0);
	d3dDeviceContext->Dispatch(1, 1, 1);

    UnbindResources(d3dDeviceContext);

    return mPartitionBuffer.GetShaderResource();
}


ID3D11ShaderResourceView* SDSMPartitions::ComputeLogPartitionsFromGBuffer(
    ID3D11DeviceContext *d3dDeviceContext,
    unsigned int gbufferTexturesNum, ID3D11ShaderResourceView** gbufferTextures,
    ID3D11Buffer* perFrameConstants,
    const D3DXVECTOR3& lightSpaceBorder,
    const D3DXVECTOR3& maxScale,
    int screenWidth, int screenHeight)
{
    // Update relevant constant data
    mCurrentConstants.mLightSpaceBorder = D3DXVECTOR4(lightSpaceBorder, 0.0f);
    mCurrentConstants.mMaxScale = D3DXVECTOR4(maxScale, 0.0f);
    UpdateShaderConstants(d3dDeviceContext);

    // Setup constant buffers (these are global for all shaders)
    d3dDeviceContext->CSSetConstantBuffers(0, 1, &perFrameConstants);
    d3dDeviceContext->CSSetConstantBuffers(1, 1, &mSDSMPartitionsConstants);

    // Clear out Z-bounds result for reduction
    ID3D11UnorderedAccessView* partitionUAV = mPartitionBuffer.GetUnorderedAccess();    
	d3dDeviceContext->CSSetShader(mClearZBoundsCS->GetShader(), 0, 0);
	d3dDeviceContext->CSSetUnorderedAccessViews(6, 1, &partitionUAV, 0);
	d3dDeviceContext->Dispatch(1, 1, 1);

    // Reduce Z-bounds
    int dispatchWidth = (screenWidth + mCurrentConstants.mReduceTileDim - 1) / mCurrentConstants.mReduceTileDim;
    int dispatchHeight = (screenHeight + mCurrentConstants.mReduceTileDim - 1) / mCurrentConstants.mReduceTileDim;

    d3dDeviceContext->CSSetShader(mReduceZBoundsFromGBufferCS->GetShader(), 0, 0);
    d3dDeviceContext->CSSetShaderResources(0, gbufferTexturesNum, gbufferTextures);
	d3dDeviceContext->CSSetUnorderedAccessViews(6, 1, &partitionUAV, 0);
	d3dDeviceContext->Dispatch(dispatchWidth, dispatchHeight, 1);
    
    // Generate partition intervals from Z-bounds
    d3dDeviceContext->CSSetShader(mLogPartitionsFromZBoundsCS->GetShader(), 0, 0);
	d3dDeviceContext->CSSetUnorderedAccessViews(6, 1, &partitionUAV, 0);
	d3dDeviceContext->Dispatch(1, 1, 1);
    
    ReducePartitionBounds(d3dDeviceContext, gbufferTexturesNum, gbufferTextures,
        perFrameConstants, screenWidth, screenHeight);

    return mPartitionBuffer.GetShaderResource();
}


ID3D11ShaderResourceView* SDSMPartitions::ComputeAdaptiveLogPartitions(
    ID3D11DeviceContext *d3dDeviceContext,
    ID3D11Buffer* perFrameConstants,
    const D3DXVECTOR3& lightSpaceBorder,
    const D3DXVECTOR3& maxScale)
{
    // Update relevant constant data
    mCurrentConstants.mLightSpaceBorder = D3DXVECTOR4(lightSpaceBorder, 0.0f);
    mCurrentConstants.mMaxScale = D3DXVECTOR4(maxScale, 0.0f);
    UpdateShaderConstants(d3dDeviceContext);

    // Setup constant buffers (these are global for all shaders)
    d3dDeviceContext->CSSetConstantBuffers(0, 1, &perFrameConstants);
    d3dDeviceContext->CSSetConstantBuffers(1, 1, &mSDSMPartitionsConstants);
    
    // Generate partitions
    ID3D11ShaderResourceView* histogramSRV = mHistogram.GetShaderResource();
    ID3D11UnorderedAccessView* partitionUAV = mPartitionBuffer.GetUnorderedAccess();
    d3dDeviceContext->CSSetShader(mAdaptiveLogPartitionsCS->GetShader(), 0, 0);
    d3dDeviceContext->CSSetShaderResources(5, 1, &histogramSRV);
	d3dDeviceContext->CSSetUnorderedAccessViews(6, 1, &partitionUAV, 0);
	d3dDeviceContext->Dispatch(1, 1, 1);

    UnbindResources(d3dDeviceContext);

    return mPartitionBuffer.GetShaderResource();
}


ID3D11ShaderResourceView* SDSMPartitions::ComputeCustomPartitions(
    ID3D11DeviceContext *d3dDeviceContext,
    unsigned int gbufferTexturesNum, ID3D11ShaderResourceView** gbufferTextures,
    ID3D11ShaderResourceView* customPartitionsSRV,
    ID3D11Buffer* perFrameConstants,
    const D3DXVECTOR3& lightSpaceBorder,
    const D3DXVECTOR3& maxScale,
    int screenWidth, int screenHeight)
{
    // Update relevant constant data
    mCurrentConstants.mLightSpaceBorder = D3DXVECTOR4(lightSpaceBorder, 0.0f);
    mCurrentConstants.mMaxScale = D3DXVECTOR4(maxScale, 0.0f);
    UpdateShaderConstants(d3dDeviceContext);

    // Copy custom partition data into our UAV for updating bounds
    ID3D11Resource* customPartitions;
    customPartitionsSRV->GetResource(&customPartitions);
    d3dDeviceContext->CopyResource(mPartitionBuffer.GetBuffer(), customPartitions);
    customPartitions->Release();

    ReducePartitionBounds(d3dDeviceContext, gbufferTexturesNum, gbufferTextures,
        perFrameConstants, screenWidth, screenHeight);

    return mPartitionBuffer.GetShaderResource();;
}


void SDSMPartitions::ReducePartitionBounds(
    ID3D11DeviceContext *d3dDeviceContext,
    unsigned int gbufferTexturesNum, ID3D11ShaderResourceView** gbufferTextures,
    ID3D11Buffer* perFrameConstants,
    int screenWidth, int screenHeight)
{
    // Setup constant buffers (these are global for all shaders)
    d3dDeviceContext->CSSetConstantBuffers(0, 1, &perFrameConstants);
    d3dDeviceContext->CSSetConstantBuffers(1, 1, &mSDSMPartitionsConstants);

    int dispatchWidth = (screenWidth + mCurrentConstants.mReduceTileDim - 1) / mCurrentConstants.mReduceTileDim;
    int dispatchHeight = (screenHeight + mCurrentConstants.mReduceTileDim - 1) / mCurrentConstants.mReduceTileDim;

    // Clear partition bounds buffer
    ID3D11UnorderedAccessView* partitionBoundsUAV = mPartitionBounds.GetUnorderedAccess();   
	d3dDeviceContext->CSSetShader(mClearPartitionBoundsCS->GetShader(), 0, 0);
	d3dDeviceContext->CSSetUnorderedAccessViews(7, 1, &partitionBoundsUAV, 0);
	d3dDeviceContext->Dispatch(1, 1, 1);
    
    UnbindResources(d3dDeviceContext);

    // Reduce bounds
    ID3D11ShaderResourceView* partitionSRV = mPartitionBuffer.GetShaderResource();
    d3dDeviceContext->CSSetShader(mReduceBoundsFromGBufferCS->GetShader(), 0, 0);
    d3dDeviceContext->CSSetShaderResources(0, gbufferTexturesNum, gbufferTextures);
    d3dDeviceContext->CSSetShaderResources(6, 1, &partitionSRV);
	d3dDeviceContext->CSSetUnorderedAccessViews(7, 1, &partitionBoundsUAV, 0);
	d3dDeviceContext->Dispatch(dispatchWidth, dispatchHeight, 1);

    UnbindResources(d3dDeviceContext);

    // Update partitions with new bounds
    ID3D11ShaderResourceView* partitionBoundsSRV = mPartitionBounds.GetShaderResource();
    ID3D11UnorderedAccessView* partitionUAV = mPartitionBuffer.GetUnorderedAccess();
    d3dDeviceContext->CSSetShader(mCustomPartitionsCS->GetShader(), 0, 0);
    d3dDeviceContext->CSSetShaderResources(7, 1, &partitionBoundsSRV);
    d3dDeviceContext->CSSetUnorderedAccessViews(6, 1, &partitionUAV, 0);
	d3dDeviceContext->Dispatch(1, 1, 1);

    UnbindResources(d3dDeviceContext);
}


void SDSMPartitions::UpdateShaderConstants(ID3D11DeviceContext *d3dDeviceContext) const
{
    // TODO: Lazy w/ dirty bit?
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    d3dDeviceContext->Map(mSDSMPartitionsConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    SDSMPartitionsConstants *data = static_cast<SDSMPartitionsConstants *>(mappedResource.pData);
    *data = mCurrentConstants;
    d3dDeviceContext->Unmap(mSDSMPartitionsConstants, 0);
}
