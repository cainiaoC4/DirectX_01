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

#ifndef RENDERING_HLSL
#define RENDERING_HLSL

#ifndef SHADOWAA_SAMPLES
#define SHADOWAA_SAMPLES 1
#endif

#ifndef SHADOWTRANSPARENCYAA_ENABLED
#define SHADOWTRANSPARENCYAA_ENABLED 0
#endif

#include "GBuffer.hlsl"
#include "Histogram.hlsl"
#include "FullScreenTriangle.hlsl"

//--------------------------------------------------------------------------------------
// Geometry phase
//--------------------------------------------------------------------------------------
Texture2D gDiffuseTexture : register(t0);
SamplerState gDiffuseSampler : register(s0);

struct GeometryVSIn
{
    float3 position : position;
    float3 normal   : normal;
    float2 texCoord : texCoord;
};

struct GeometryVSOut
{
    float4 position     : SV_Position;
    float3 positionView : positionView;      // View space position
    float3 normal       : normal;
    float2 texCoord     : texCoord;
};

GeometryVSOut GeometryVS(GeometryVSIn input)
{
    GeometryVSOut output;

    output.position     = mul(float4(input.position, 1.0f), mCameraWorldViewProj);
    output.positionView = mul(float4(input.position, 1.0f), mCameraWorldView).xyz;
    output.normal       = mul(float4(input.normal, 0.0f), mCameraWorldView).xyz;
    output.texCoord     = input.texCoord;
    
    return output;
}

float3 ComputeFaceNormal(float3 position)
{
    return cross(ddx_coarse(position), ddy_coarse(position));
}

void GeometryPS(GeometryVSOut input,
                out GBuffer outputGBuffer,
                out float4 outputLit : SV_Target2)
{
    float4 albedo = gDiffuseTexture.Sample(gDiffuseSampler, input.texCoord);
    albedo.rgb = mUI.lightingOnly ? float3(1.0f, 1.0f, 1.0f) : albedo.rgb;
    
    // Map NULL diffuse textures to white
    uint2 textureDim;
    gDiffuseTexture.GetDimensions(textureDim.x, textureDim.y);
    albedo = (textureDim.x == 0U ? float4(1.0f, 1.0f, 1.0f, 1.0f) : albedo);

    // Optionally use face normal instead of shading normal
    float3 faceNormal = ComputeFaceNormal(input.positionView);
    float3 normal = normalize(mUI.faceNormals ? faceNormal : input.normal);
    
    // Effectively encodes the shading and face normals
    // (The latter are used to recover derivatives in deferred passes.)
    outputGBuffer.normals = float4(EncodeSphereMap(normal),
                                   ddx_coarse(input.positionView.z),
                                   ddy_coarse(input.positionView.z));

    outputGBuffer.albedo = albedo;
    
    // Initialize the back buffer
    // TODO: Better ambient
    // TODO: Could do this in a later pass as well
    outputLit.rgb = (mUI.lightingOnly ? 0.0f : 0.05f) * albedo.rgb;
    outputLit.a = albedo.a;
}

void GeometryAlphaTestPS(GeometryVSOut input,
                         out GBuffer outputGBuffer,
                         out float4 outputLit : SV_Target2)
{
    GeometryPS(input, outputGBuffer, outputLit);
    
    // Alpha test
    clip(outputGBuffer.albedo.a - 0.3f);

    // Always use face normal for alpha tested stuff since it's double-sided
    outputGBuffer.normals.xy = EncodeSphereMap(normalize(ComputeFaceNormal(input.positionView)));
}


//--------------------------------------------------------------------------------------
// Shadow map phase
//--------------------------------------------------------------------------------------
cbuffer PerPartitionPassConstants : register(b1)
{
    uint mRenderPartition;
    uint mAccumulatePartitionBegin;
    uint mAccumulatePartitionCount;
};

StructuredBuffer<Partition> gPartitions : register(t4);

float4 ShadowVS(GeometryVSIn input) : SV_Position
{
    float4 position = mul(float4(input.position, 1.0f), mLightWorldViewProj);
    
    // Scale/bias in NDC space
    Partition partition = gPartitions[mRenderPartition];
    position.xy *= partition.scale.xy;
    position.x += (2.0f * partition.bias.x + partition.scale.x - 1.0f);
    position.y -= (2.0f * partition.bias.y + partition.scale.y - 1.0f);
    
    // Push the surface back ever-so-slightly
    // The only reason for this is because of interpolant and position reconstruction
    // precision issues on some GPUs/G-buffer layouts.
    position.z += 0.0001f;
    
    // Clamp to [0, 1] happens in the viewport transform
    // NOTE: Depth clipping is disabled in this pass (directional light)
    position.z = position.z * partition.scale.z + partition.bias.z;
    
    return position;
}


//--------------------------------------------------------------------------------------
// When using alpha test, need to pass through texture coordinates and sample
// texture in the pixel shader.
//--------------------------------------------------------------------------------------
struct ShadowAlphaTestVSOut
{
    float4 position : SV_Position;
    // Read the alpha texture at sample frequency to get "transparency AA"
#if SHADOWTRANSPARENCYAA_ENABLED
    sample float2 texCoord : texCoord;
#else
    float2 texCoord : texCoord;
#endif
};

ShadowAlphaTestVSOut ShadowAlphaTestVS(GeometryVSIn input)
{
    ShadowAlphaTestVSOut output;
    output.position = ShadowVS(input);
    output.texCoord = input.texCoord;
    return output;
}

void ShadowAlphaTestPS(ShadowAlphaTestVSOut input)
{
    float4 albedo = gDiffuseTexture.Sample(gDiffuseSampler, input.texCoord);
    clip(albedo.a - 0.3f);
}


//--------------------------------------------------------------------------------------
// Lighting phase
//--------------------------------------------------------------------------------------
Texture2DArray gShadowTexture : register(t5);

// Include proper filtering implementation
// TODO: Replace this with just a define with the include file name?
#if NEIGHBOR_PCF_FILTERING
    #include "RenderingNeighborPCF.hlsl"
#else // EVSM_FILTERING
    #include "RenderingEVSM.hlsl"
#endif

float3 GetPartitionColor(uint p)
{  
    uint perm = (p % 7) + 1;
    return float3(
        perm & 1 ? 1.0f : 0.0f,
        perm & 2 ? 1.0f : 0.0f,
        perm & 4 ? 1.0f : 0.0f);
}

// Compute resolution error metric
float ResolutionError(float2 dx, float2 dy)
{    
    float2 shadowMapDim = mShadowTextureDim.xy;    
    float2 fwidth = max(abs(dx * shadowMapDim), abs(dy * shadowMapDim));
    float lod = log2(sqrt(fwidth.x * fwidth.y));
    
    return max(-lod, 0.0f);
}

float4 LightingPS(FullScreenTriangleVSOut input) : SV_Target
{
    SurfaceData surface = ComputeSurfaceDataFromGBuffer(uint2(input.positionViewport.xy));
    
    float3 lit = float3(0.0f, 0.0f, 0.0f);
    
    // Early out if we don't fall into any of the relevant partitions
    bool inAnyPartition = (surface.positionView.z >= gPartitions[mAccumulatePartitionBegin].intervalBegin &&
        surface.positionView.z < gPartitions[mAccumulatePartitionBegin + mAccumulatePartitionCount - 1].intervalEnd);
    
    // Also early out if we're a back face
    float nDotL = saturate(dot(-mLightDir.xyz, surface.normal));
    
    if (inAnyPartition && (nDotL > 0.0f || mUI.visualizeError)) {
    
        for (uint i = 0; i < mAccumulatePartitionCount; ++i) {
            uint partitionIndex = mAccumulatePartitionBegin + i;
            Partition partition = gPartitions[partitionIndex];
            
            if (surface.positionView.z < partition.intervalEnd) {
                float3 texCoord = surface.lightTexCoord.xyz * partition.scale.xyz + partition.bias.xyz;
                float3 texCoordDX = surface.lightTexCoordDX.xyz * partition.scale.xyz;
                float3 texCoordDY = surface.lightTexCoordDY.xyz * partition.scale.xyz;
                float depth = clamp(texCoord.z, 0.0f, 1.0f);
            
                float shadowContrib = ShadowContribution(texCoord, texCoordDX, texCoordDY, depth, partition, i);
                float3 contrib = shadowContrib * (nDotL * surface.albedo.xyz);
                
                if (mUI.visualizePartitions) {
                    contrib = lerp(contrib, GetPartitionColor(partitionIndex), 0.25f);
                }
                
                // Visualize error - WIP...
                if (mUI.visualizeError) {
                    float error = ResolutionError(texCoordDX.xy, texCoordDY.xy);
                    contrib = float3(error / 10.0f, 0, 0);
                }
                
                // Accumulate lighting
                lit += contrib;
                break;
            }
        }
        
    } else {
        discard;
    }
    
    return float4(lit, 1.0f);
}


//--------------------------------------------------------------------------------------
// Skybox
//--------------------------------------------------------------------------------------
TextureCube gSkyboxTexture : register(t0);

struct SkyboxVSOut
{
    float4 position : SV_Position;
    float3 texCoord : texCoord;
};

SkyboxVSOut SkyboxVS(GeometryVSIn input)
{
    SkyboxVSOut output;
    
    // NOTE: Don't translate skybox, and make sure depth == 0 (complementary Z)
    output.position = mul(float4(input.position, 0.0f), mCameraViewProj).xyww;
    output.position.z = 0.0f;
    output.texCoord = input.position;
    
    return output;
}

float4 SkyboxPS(SkyboxVSOut input) : SV_Target0
{
    return gSkyboxTexture.Sample(gDiffuseSampler, input.texCoord);
}

#endif // RENDERING_HLSL