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

#ifndef RENDERING_NEIGHBOR_PCF_HLSL
#define RENDERING_NEIGHBOR_PCF_HLSL

#include "BlurUtil.hlsl"

//--------------------------------------------------------------------------------------
// Lookup and filtering
//--------------------------------------------------------------------------------------
SamplerComparisonState gShadowSampler : register(s5);

float2 ComputeReceiverPlaneDepthBias(float3 texCoordDX, float3 texCoordDY)
{    
    float2 biasUV;
    biasUV.x = texCoordDY.y * texCoordDX.z - texCoordDX.y * texCoordDY.z;
    biasUV.y = texCoordDX.x * texCoordDY.z - texCoordDY.x * texCoordDX.z;
    biasUV *= 1.0f / ((texCoordDX.x * texCoordDY.y) - (texCoordDX.y * texCoordDY.x));
    return biasUV;
}

float ShadowContribution(float3 texCoord, float3 texCoordDX, float3 texCoordDY, float depth,
                         Partition partition, uint textureArrayIndex)
{
    // Compute
    float2 receiverPlaneDepthBias = ComputeReceiverPlaneDepthBias(texCoordDX, texCoordDY);

    // NOTE: For now the preamble here is all uniform and could be lifted, but it is
    // dependent on the per-pixel derivatives in a "real" PCF implementation, thus it is left
    // here for simplicity and to be realistic.
    
    // Work out filter metadata for edge softening
    float2 filterSize = mBlurSizeLightSpace.xy * partition.scale.xy;
    
    // Rescale blur kernel into texel space
    float2 dimensions;
    float elements;
    gShadowTexture.GetDimensions(dimensions.x, dimensions.y, elements);
    float2 filterSizeTexels = filterSize * dimensions;
    float2 texelSize = 1.0f / dimensions;
    
    // Always use at least a 1x1 filter
    filterSizeTexels = max(filterSizeTexels, float2(1.0f, 1.0f));
        
    BlurData blurDataX = ComputeBlurData(filterSizeTexels.x);
    BlurData blurDataY = ComputeBlurData(filterSizeTexels.y);
        
    // Static depth biasing to make up for incorrect fractional sampling on the shadow map grid
    float fractionalSamplingError = dot(float2(1.0f, 1.0f) * texelSize, abs(receiverPlaneDepthBias));
    depth -= fractionalSamplingError;
        
    // We loop slightly differently here... we need both to cover *all* samples including the edge ones
    int loopXEnd = blurDataX.interiorSamplesLoopCount + 1;
    int loopYEnd = blurDataY.interiorSamplesLoopCount + 1;
        
    float average = 0.0f;
    
    for (int y = -loopYEnd; y <= loopYEnd; ++y) {
        float weightY = (abs(y) == loopYEnd) ? blurDataY.edgeSampleWeight : blurDataY.interiorSampleWeight;
    
        for (int x = -loopXEnd; x <= loopXEnd; ++x) {
            float weightX = (abs(x) == loopXEnd) ? blurDataX.edgeSampleWeight : blurDataX.interiorSampleWeight;

            // Compute offset and apply planar depth bias
            float2 offset = float2(x, y) * texelSize;
            float planarBiasedDepth = depth + dot(offset, receiverPlaneDepthBias);
            
            average += weightX * weightY *
                gShadowTexture.SampleCmpLevelZero(gShadowSampler, float3(texCoord.xy + offset, textureArrayIndex), planarBiasedDepth);
        }
    }
    
    return average;
}

#endif // RENDERING_NEIGHBOR_PCF_HLSL
