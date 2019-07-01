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

#ifndef GBUFFER_HLSL
#define GBUFFER_HLSL

#include "PerFrameConstants.hlsl"

// GBuffer and related common utilities and structures
struct GBuffer
{
    // SV_Target0 is reserved for the back buffer when rendering
    float4 normals : SV_Target0;
    float4 albedo  : SV_Target1;
};

// Above values PLUS depth buffer (last element)
Texture2D gGBufferTextures[3] : register(t0);

struct Partition
{
    float intervalBegin;
    float intervalEnd;
    
    // These are given in texture coordinate [0, 1] space
    float3 scale;
    float3 bias;
};

// This version of the structure is useful for atomic operations (asuint(), etc)
// Must match layout of structure above!!
struct PartitionUint
{
    uint intervalBegin;
    uint intervalEnd;
    
    // These are given in texture coordinate [0, 1] space
    uint3 scale;
    uint3 bias;
};

float2 EncodeSphereMap(float3 n)
{
    float oneMinusZ = 1.0f - n.z;
    float p = sqrt(n.x * n.x + n.y * n.y + oneMinusZ * oneMinusZ);
    return n.xy * rcp(p) * 0.5f + 0.5f;
}

float3 DecodeSphereMap(float2 e)
{
    float2 tmp = e - e * e;
    float f = tmp.x + tmp.y;
    float m = sqrt(4.0f * f - 1.0f);
    
    float3 n;
    n.xy = m * (e * 4.0f - 2.0f);
    n.z  = 3.0f - 8.0f * f;
    return n;
}

float3 ComputePositionViewFromZ(float2 positionScreen,
                                float viewSpaceZ)
{
    float2 screenSpaceRay = float2(positionScreen.x / mCameraProj._11,
                                   positionScreen.y / mCameraProj._22);
    
    float3 positionView;
    positionView.z = viewSpaceZ;
    // Solve the two projection equations
    positionView.xy = screenSpaceRay.xy * positionView.z;
    
    return positionView;
}

// data that we can read or derived from the surface shader outputs
struct SurfaceData
{
    float3 positionView;         // View space position
    float3 positionViewDX;       // Screen space derivatives
    float3 positionViewDY;       // of view space position
    float3 normal;               // View space normal
    float4 albedo;
    float3 lightTexCoord;        // Texture coordinates and depth in light space, [0, 1]
    float3 lightTexCoordDX;      // Screen space partial derivatives
    float3 lightTexCoordDY;      // of light space texture coordinates.
};

float3 ProjectIntoLightTexCoord(float3 positionView)
{
    float4 positionLight = mul(float4(positionView, 1.0f), mCameraViewToLightProj);
    float3 texCoord = (positionLight.xyz / positionLight.w) * float3(0.5f, -0.5f, 1.0f) + float3(0.5f, 0.5f, 0.0f);
    return texCoord;
}

SurfaceData ComputeSurfaceDataFromGBuffer(uint2 coords)
{
    // Load the raw data from the GBuffer
    GBuffer rawData;
    rawData.normals = gGBufferTextures[0][coords];
    rawData.albedo  = gGBufferTextures[1][coords];
    float zBuffer   = gGBufferTextures[2][coords].x;
    
    float2 gbufferDim;
    gGBufferTextures[0].GetDimensions(gbufferDim.x, gbufferDim.y);
    
    // Compute screen/clip-space position and neighbour positions
    // NOTE: Mind DX11 viewport transform and pixel center!
    float2 screenPixelOffset = float2(2.0f, -2.0f) / gbufferDim;
    float2 positionScreen = (float2(coords.xy) + 0.5f) * screenPixelOffset.xy + float2(-1.0f, 1.0f);
    float2 positionScreenX = positionScreen + float2(screenPixelOffset.x, 0.0f);
    float2 positionScreenY = positionScreen + float2(0.0f, screenPixelOffset.y);
    
    // Decode into reasonable outputs
    SurfaceData data;

    // Unproject depth buffer Z value into view space
    float viewSpaceZ = mCameraProj._43 / (zBuffer - mCameraProj._33);
    
    // Solve for view-space position and derivatives
    data.positionView   = ComputePositionViewFromZ(positionScreen , viewSpaceZ);
    data.positionViewDX = ComputePositionViewFromZ(positionScreenX, viewSpaceZ + rawData.normals.z) - data.positionView;
    data.positionViewDY = ComputePositionViewFromZ(positionScreenY, viewSpaceZ + rawData.normals.w) - data.positionView;
    
    data.normal = DecodeSphereMap(rawData.normals.xy);
    data.albedo = rawData.albedo;
            
    // Solve for light space position and screen-space derivatives
    float deltaPixels = 2.0f;
    data.lightTexCoord   = (ProjectIntoLightTexCoord(data.positionView));
    data.lightTexCoordDX = (ProjectIntoLightTexCoord(data.positionView + deltaPixels * data.positionViewDX) - data.lightTexCoord) / deltaPixels;
    data.lightTexCoordDY = (ProjectIntoLightTexCoord(data.positionView + deltaPixels * data.positionViewDY) - data.lightTexCoord) / deltaPixels;
    
    return data;
}

#endif // GBUFFER_HLSL
