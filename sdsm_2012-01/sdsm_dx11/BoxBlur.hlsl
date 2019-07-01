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

#ifndef BOX_BLUR_HLSL
#define BOX_BLUR_HLSL

#include "GBuffer.hlsl"
#include "BlurUtil.hlsl"

cbuffer BoxBlurConstants : register(b0)
{
    float2 mFilterSize;     // Filter size in light view space [0,1] (unaffected by partitioning)
    uint mPartition;        // Which partition this is (for scaling filter regions)
    uint mDimension;        // Which dimension to blur in (0 = horiz, 1 = vert)
}

Texture2DArray gInputTexture : register(t0);
StructuredBuffer<Partition> gPartitions : register(t1);

struct BoxBlurVSOut
{
    float4 positionViewport : SV_Position;
    float4 positionClip     : positionClip;
    
    // These are constant over every vertex (no interpolation necessary)
    nointerpolation BlurData blurData;
};

BoxBlurVSOut BoxBlurVS(uint vertexID : SV_VertexID)
{
    BoxBlurVSOut output;

    // Parametrically work out vertex location for full screen triangle
    float2 grid = float2((vertexID << 1) & 2, vertexID & 2);
    output.positionClip = float4(grid * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    output.positionViewport = output.positionClip;
    
    // Scale filter size into this partition
    float2 filterSize = mFilterSize * gPartitions[mPartition].scale.xy;
        
    // Rescale blur kernel into texel space
    float2 dimensions;
    float elements;
    gInputTexture.GetDimensions(dimensions.x, dimensions.y, elements);
    float2 filterSizeTexels = filterSize * float2(dimensions);

    // If neither dimension needs blurring, we can skip both passes
    // To do this, we set up a triangle that gets clipped
    // We could do this by writing a NaN or something for position, but since
    // this section of the shader is actually fully determined by uniforms, it will
    // branch the same way for all three vertices. Thus we can simply set the triangle
    // outside the viewport and let clipping handle it.
    // NOTE: Don't bother putting the rest in the else branch or the compiler complains
    // about potentially uninitialized outputs... whatever.
    if (all(filterSizeTexels <= 1.0f)) {
        output.positionViewport = float4(-10.0f, -10.0f, -10.0f, 1.0f);
    }

    // Rest of the math need only be for the relevant dimension
    float filterSizeDim = (mDimension == 0 ? filterSizeTexels.x : filterSizeTexels.y);
    filterSizeDim = max(filterSizeDim, 1.0f);
    output.blurData = ComputeBlurData(filterSizeDim);
    
    return output;
}


float4 BoxBlurPS(BoxBlurVSOut input) : SV_Target
{
    uint2 sampleOffsetMask = (uint2(0, 1) == mDimension.xx ? uint2(1, 1) : uint2(0, 0));

    // Center sample
    uint2 coords = int2(input.positionViewport.xy);
    float4 average = input.blurData.interiorSampleWeight * gInputTexture.Load(uint4(coords, 0, 0));
    
    // Other interior samples
    for (uint i = 1; i <= input.blurData.interiorSamplesLoopCount; ++i) {
        uint2 offset = i * sampleOffsetMask;
        // Left sample
        average += input.blurData.interiorSampleWeight * gInputTexture.Load(uint4(coords - offset, 0, 0));
        // Right sample
        average += input.blurData.interiorSampleWeight * gInputTexture.Load(uint4(coords + offset, 0, 0));
    }
    
    // Edge samples
    uint2 offset = (input.blurData.interiorSamplesLoopCount + 1) * sampleOffsetMask;
    // Left sample
    average += input.blurData.edgeSampleWeight * gInputTexture.Load(uint4(coords - offset, 0, 0));
    // Right sample
    average += input.blurData.edgeSampleWeight * gInputTexture.Load(uint4(coords + offset, 0, 0));
    
    return average;
}

#endif // BOX_BLUR_HLSL