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

#ifndef RENDERING_EVSM_HLSL
#define RENDERING_EVSM_HLSL

//--------------------------------------------------------------------------------------
// Convert to EVSM
//--------------------------------------------------------------------------------------
Texture2DMS<float, SHADOWAA_SAMPLES> gShadowDepthTextureMS : register(t0);

float2 GetEVSMExponents(Partition partition)
{
    float2 lightSpaceExponents = float2(mUI.positiveExponent, mUI.negativeExponent);
    
    // Make sure exponents say consistent in light space regardless of partition
    // scaling. This prevents the exponentials from ever getting too rediculous
    // and maintains consistency across partitions.
    // Clamp to maximum range of fp32 to prevent overflow/underflow
    return min(lightSpaceExponents / partition.scale.zz,
               float2(42.0f, 42.0f));
}

// Input depth should be in [0, 1]
float2 WarpDepth(float depth, float2 exponents)
{
    // Rescale depth into [-1, 1]
    depth = 2.0f * depth - 1.0f;
    float pos =  exp( exponents.x * depth);
    float neg = -exp(-exponents.y * depth);
    return float2(pos, neg);
}

float4 ShadowDepthToEVSMPS(FullScreenTriangleVSOut input) : SV_Target
{
    float sampleWeight = 1.0f / float(SHADOWAA_SAMPLES);
    int2 coords = int2(input.positionViewport.xy);
    
    float2 exponents = GetEVSMExponents(gPartitions[mRenderPartition]);
    
    // Simple average (box filter) for now
    float4 average = float4(0.0f, 0.0f, 0.0f, 0.0f);
    
    // Sample indices to Load() must be literal, so force unroll
    [unroll] for (int i = 0; i < SHADOWAA_SAMPLES; ++i) {
        // Convert to EVSM representation
        // NOTE: D3D documentation appears to be wrong on the syntax of Load from Texture2DMS...
        float depth = gShadowDepthTextureMS.Load(coords, i);
        
        float2 warpedDepth = WarpDepth(depth, exponents);
        average += sampleWeight * float4(warpedDepth.xy, warpedDepth.xy * warpedDepth.xy);
    }
    
    return average;
}


//--------------------------------------------------------------------------------------
// Lookup and filtering
//--------------------------------------------------------------------------------------
SamplerState gShadowSampler : register(s5);

float ChebyshevUpperBound(float2 moments, float mean, float minVariance)
{
    // Compute variance
    float variance = moments.y - (moments.x * moments.x);
    variance = max(variance, minVariance);
    
    // Compute probabilistic upper bound
    float d = mean - moments.x;
    float pMax = variance / (variance + (d * d));
    
    // One-tailed Chebyshev
    return (mean <= moments.x ? 1.0f : pMax);
}

float ShadowContribution(float3 texCoord, float3 texCoordDX, float3 texCoordDY, float depth,
                         Partition partition, uint textureArrayIndex)
{
    float2 exponents = GetEVSMExponents(partition);
    float2 warpedDepth = WarpDepth(depth, exponents);
    
    float4 occluder = gShadowTexture.SampleGrad(gShadowSampler, float3(texCoord.xy, textureArrayIndex), texCoordDX.xy, texCoordDY.xy);
    
    // Derivative of warping at depth
    // TODO: Parameterize min depth stddev
    float2 depthScale = 0.0001f * exponents * warpedDepth;
    float2 minVariance = depthScale * depthScale;
    
    float posContrib = ChebyshevUpperBound(occluder.xz, warpedDepth.x, minVariance.x);
    float negContrib = ChebyshevUpperBound(occluder.yw, warpedDepth.y, minVariance.y);
    
    float shadowContrib = mUI.usePositiveExponent > 0 ? posContrib : 1.0f;
    shadowContrib = mUI.useNegativeExponent > 0 ? min(shadowContrib, negContrib) : shadowContrib;
    
    return shadowContrib;
}

#endif // RENDERING_EVSM_HLSL
