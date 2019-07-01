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

#ifndef VISUALIZE_HLSL
#define VISUALIZE_HLSL

#include "Rendering.hlsl"

//--------------------------------------------------------------------------------------
// Error analysis
//--------------------------------------------------------------------------------------
float4 ErrorPS(FullScreenTriangleVSOut input) : SV_Target
{
    // Get some general metadata right from our bound resources...
    uint partitions, stride;
    gPartitions.GetDimensions(partitions, stride);
    
    SurfaceData surface = ComputeSurfaceDataFromGBuffer(uint2(input.positionViewport.xy));
    
    float error = 0.0f;
    
    for (uint partitionIndex = 0; partitionIndex < partitions; ++partitionIndex) {
        Partition partition = gPartitions[partitionIndex];
        
        if (surface.positionView.z < partition.intervalEnd) {
            float2 texCoordDX = surface.lightTexCoordDX.xy * partition.scale.xy;
            float2 texCoordDY = surface.lightTexCoordDY.xy * partition.scale.xy;
            error = ResolutionError(texCoordDX, texCoordDY);
            break;
        }
    }
    
    return error.xxxx;
}


//--------------------------------------------------------------------------------------
// Output normalized view space Z
//--------------------------------------------------------------------------------------
float NormalizedDepthPS(FullScreenTriangleVSOut input) : SV_Target
{
    SurfaceData surface = ComputeSurfaceDataFromGBuffer(uint2(input.positionViewport.xy));
    return (surface.positionView.z - mCameraNearFar.x) / (mCameraNearFar.y - mCameraNearFar.x);
}


//--------------------------------------------------------------------------------------
// Histogram visualization
//--------------------------------------------------------------------------------------
StructuredBuffer<HistogramBin> gHistogramReadOnly : register(t0);

float4 VisualizeHistogramPS(FullScreenTriangleVSOut input) : SV_Target
{
    float4 color = float4(1.0f, 1.0f, 1.0f, 1.0f);
    
    // Get some general metadata right from our bound resources...
    uint bins, partitions, stride;
    gHistogramReadOnly.GetDimensions(bins, stride);
    gPartitions.GetDimensions(partitions, stride);
    
    float nearZ = mCameraNearFar.x;
    float farZ = mCameraNearFar.y;
    float viewSpaceZ = lerp(nearZ, farZ, input.texCoord.x);
        
    // NOTE: This logic only really works for the unwarped bin case, but as a rough visualization
    // it works fine for what we're trying to accomplish even when the bins are warped.
    uint binIndex = uint(floor(ZToNormalizedBin(viewSpaceZ, nearZ, farZ) * float(bins)));
    HistogramBin bin = gHistogramReadOnly[binIndex];
    
    float barHeight = float(bin.count) / 10000.0f;       // TODO: ... yeah
    
    color.xyz = (1.0f - input.texCoord.y) < barHeight ? float3(0.0f, 0.0f, 0.0f) : color.xyz;
    
    if (mUI.visualizePartitions) {
        // Which partition are we in
        uint partition = 0;
        for (uint i = 0; i < (partitions - 1); ++i) {
            [flatten] if (viewSpaceZ >= gPartitions[i].intervalEnd) {
                ++partition;
            }
        }

        color.xyz = lerp(color.xyz, GetPartitionColor(partition), 0.4f);
    }
    
    return color;
}


//--------------------------------------------------------------------------------------
// Light space visualization
// TODO: Unify more with Geometry path?
//--------------------------------------------------------------------------------------
struct VisualizeLightSpaceVSOut
{
    float4 position : SV_Position;
    float2 texCoord : texCoord;
};

VisualizeLightSpaceVSOut VisualizeLightSpaceVS(GeometryVSIn input)
{
    VisualizeLightSpaceVSOut output;

    // TODO: Expand Z range at all for visualization purposes to show the scene a bit better?
    output.position = mul(float4(input.position, 1.0f), mLightWorldViewProj);
    output.texCoord = input.texCoord;
    
    return output;
}

float4 VisualizeLightSpacePS(VisualizeLightSpaceVSOut input) : SV_Target
{
    float4 albedo = gDiffuseTexture.Sample(gDiffuseSampler, input.texCoord);

    // Map NULL diffuse textures to white
    uint2 textureDim;
    gDiffuseTexture.GetDimensions(textureDim.x, textureDim.y);
    albedo = (textureDim.x == 0U ? float4(1.0f, 1.0f, 1.0f, 1.0f) : albedo);

    return float4(albedo.rgb * 0.1f, albedo.a);
}

float4 VisualizeLightSpaceAlphaTestPS(VisualizeLightSpaceVSOut input) : SV_Target
{
    float4 output = VisualizeLightSpacePS(input);
    clip(output.a - 0.3f);
    return output;
}


float4 VisualizeLightSpaceScatterVS(uint vertexID : SV_VertexID) : SV_Position
{
    // Compute which framebuffer pixel we are (1D->2D)
    uint2 gbufferDim;
    gGBufferTextures[0].GetDimensions(gbufferDim.x, gbufferDim.y);

    uint2 globalCoord;
    globalCoord.y = vertexID / gbufferDim.x;
    globalCoord.x = vertexID - globalCoord.y * gbufferDim.x;

    // Work out light space coordinates
    SurfaceData surface = ComputeSurfaceDataFromGBuffer(globalCoord);

    float2 lightCoords = surface.lightTexCoord.xy;
    // Convert back to NDC
    lightCoords.xy = float2(2.0f, -2.0f) * lightCoords.xy + float2(-1.0f, 1.0f);

    // Ignore samples that come from the background (i.e. make sure they clip)
    if (surface.positionView.z >= mCameraNearFar.y) {
        lightCoords.xy = float2(-10.0f, -10.0f);
    }

    return float4(lightCoords, 0.0f, 1.0f);
}

float4 VisualizeLightSpaceScatterPS() : SV_Target
{
    return float4(1.0f, 1.0f / 255.0f, 0.0f, 1.0f);
}


float4 VisualizeFrustumVS(uint vertexID : SV_VertexID) : SV_Position
{
    // Query number of partitions
    uint partitions, stride;
    gPartitions.GetDimensions(partitions, stride);
    
    // The following is because I'm too lazy to make a vertex/index buffer ;)
    uint corner;
    float cameraZ;

    // First partitions+1 sets of 8 vertices draw partition Z planes
    uint partitionZPlaneVertices = 8 * (partitions + 1);
    if (vertexID < partitionZPlaneVertices) {
        cameraZ = gPartitions[0].intervalBegin;
        uint partition = vertexID / 8;
        if (partition > 0) {
            cameraZ = gPartitions[partition - 1].intervalEnd;
        }
        // Z planes go around the corners
        corner = (((vertexID & 0x7) + 1) >> 1) % 4;
    } else {
        vertexID -= partitionZPlaneVertices;
        // Side lines of frustum go from near->far for each corner
        corner = vertexID >> 1;
        cameraZ = vertexID & 1 ? gPartitions[partitions - 1].intervalEnd : gPartitions[0].intervalBegin;
    }

    float3 vertexViewSpace;
    vertexViewSpace.z = cameraZ;
    vertexViewSpace.x = ((corner + 1) & 2 ? vertexViewSpace.z : -vertexViewSpace.z);
    vertexViewSpace.y = (corner & 2 ? vertexViewSpace.z : -vertexViewSpace.z);
    vertexViewSpace.xy *= 0.999f * rcp(mCameraProj._11_22);

    return mul(float4(vertexViewSpace, 1.0f), mCameraViewToLightProj);
}

float4 VisualizeFrustumPS() : SV_Target
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}


float4 VisualizeLightSpacePartitionsPS(FullScreenTriangleVSOut input) : SV_Target
{
    // Query number of partitions
    uint partitions, stride;
    gPartitions.GetDimensions(partitions, stride);

    float4 contrib = float4(0.0f, 0.0f, 0.0f, 0.0f);

    for (int p = partitions - 1; p >= 0; --p) {
        float2 partitionCoord = input.texCoord * gPartitions[p].scale.xy + gPartitions[p].bias.xy;
        if (all(partitionCoord >= 0.0f) && all(partitionCoord <= 1.0f)) {
            contrib = lerp(contrib, float4(GetPartitionColor(p), 1.0f), 0.4f);
        }
    }

    return contrib;
}

#endif
