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

#include "Shader.h"
#include "Buffer.h"
#include "Texture2D.h"
#include "Partitions.h"
#include <d3d11.h>

// NOTE: Must match layout of shader constant buffer
__declspec(align(16))
struct SDSMPartitionsConstants
{
    D3DXVECTOR4 mLightSpaceBorder;
    D3DXVECTOR4 mMaxScale;
    float mDilationFactor;
    unsigned int mScatterTileDim;
    unsigned int mReduceTileDim;
};

struct BoundsFloat
{
    D3DXVECTOR3 minCoord;
    D3DXVECTOR3 maxCoord;
};

struct HistogramBin
{
    unsigned int count;
    BoundsFloat bounds;
};

class SDSMPartitions
{
public:
    // NOTE: tileDim must be a multiple of 32
    SDSMPartitions(ID3D11Device *d3dDevice, int partitions, int bins, int shadowTextureDim);

    ~SDSMPartitions();

    // Clear and recompute histogram from the given G-buffer
    // Returns SRV that contains histogram data (useful for visualization, etc)
    ID3D11ShaderResourceView* ComputeHistogram(
        ID3D11DeviceContext *d3dDeviceContext,
        unsigned int gbufferTexturesNum, ID3D11ShaderResourceView** gbufferTextures,
        ID3D11Buffer* perFrameConstants,
        int screenWidth, int screenHeight);
    
    // Compute k-means-placed partitions from the current histogram data
    // Returns SRV that contains partition data
    ID3D11ShaderResourceView* ComputeKMeansPartitions(
        ID3D11DeviceContext *d3dDeviceContext,
        ID3D11Buffer* perFrameConstants,
        const D3DXVECTOR3& lightSpaceBorder,
        const D3DXVECTOR3& maxScale);

    // Compute Log partitions
    // Returns SRV that contains partition data
    ID3D11ShaderResourceView* ComputeLogPartitionsFromHistogram(
        ID3D11DeviceContext *d3dDeviceContext,
        ID3D11Buffer* perFrameConstants,
        const D3DXVECTOR3& lightSpaceBorder,
        const D3DXVECTOR3& maxScale);

    // Compute Log partitions by direct reduction of GBuffer (no histogram needed)
    ID3D11ShaderResourceView* ComputeLogPartitionsFromGBuffer(
        ID3D11DeviceContext *d3dDeviceContext,
        unsigned int gbufferTexturesNum, ID3D11ShaderResourceView** gbufferTextures,
        ID3D11Buffer* perFrameConstants,
        const D3DXVECTOR3& lightSpaceBorder,
        const D3DXVECTOR3& maxScale,
        int screenWidth, int screenHeight);

    // Compute Adaptive Log partitions from the current histogram data
    // Returns SRV that contains partition data
    ID3D11ShaderResourceView* ComputeAdaptiveLogPartitions(
        ID3D11DeviceContext *d3dDeviceContext,
        ID3D11Buffer* perFrameConstants,
        const D3DXVECTOR3& lightSpaceBorder,
        const D3DXVECTOR3& maxScale);

    // Compute custom-placed partitions (given in the partitionsSRV passed in)
    // using tight bounding volumes derived from the current histogram data
    // Returns a new SRV that contains the computed partition data
    ID3D11ShaderResourceView* ComputeCustomPartitions(
        ID3D11DeviceContext *d3dDeviceContext,
        unsigned int gbufferTexturesNum, ID3D11ShaderResourceView** gbufferTextures,
        ID3D11ShaderResourceView* customPartitionsSRV,
        ID3D11Buffer* perFrameConstants,
        const D3DXVECTOR3& lightSpaceBorder,
        const D3DXVECTOR3& maxScale,
        int screenWidth, int screenHeight);

private:
	// No copying
	SDSMPartitions(const SDSMPartitions&);
	SDSMPartitions& operator=(const SDSMPartitions&);

    void UpdateShaderConstants(ID3D11DeviceContext *d3dDeviceContext) const;

    // Derives partition bounds using reduction from the G-buffer and the partitions intervals
    // that have already been set up in the buffer.
    // NOTE: Assumes that constant buffers have already been set up!
    void ReducePartitionBounds(
        ID3D11DeviceContext *d3dDeviceContext,
        unsigned int gbufferTexturesNum, ID3D11ShaderResourceView** gbufferTextures,
        ID3D11Buffer* perFrameConstants,
        int screenWidth, int screenHeight);
    
    int mBins;
    int mPartitions;

    // We have to maintain some constants for the current histogram between
    // function calls, so it makes sense to just store them here.
    SDSMPartitionsConstants mCurrentConstants;

    StructuredBuffer<HistogramBin> mHistogram;
    StructuredBuffer<Partition> mPartitionBuffer;
    StructuredBuffer<BoundsFloat> mPartitionBounds;

    ID3D11Buffer *mSDSMPartitionsConstants;

    ComputeShader* mClearHistogramCS;
    ComputeShader* mScatterHistogramCS;
    ComputeShader* mClearZBoundsCS;
    ComputeShader* mReduceZBoundsFromGBufferCS;

    ComputeShader* mSDSMPartitionsCS;
    ComputeShader* mLogPartitionsFromHistogramCS;
    ComputeShader* mLogPartitionsFromZBoundsCS;
    ComputeShader* mAdaptiveLogPartitionsCS;
    ComputeShader* mCustomPartitionsCS;

    ComputeShader* mClearPartitionBoundsCS;
    ComputeShader* mReduceBoundsFromGBufferCS;
};
