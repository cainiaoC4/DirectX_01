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

#ifndef SDSM_PARTITIONS_HLSL
#define SDSM_PARTITIONS_HLSL

#include "GBuffer.hlsl"
#include "Histogram.hlsl"

// The number of partitions to produce
#ifndef PARTITIONS
#define PARTITIONS 4
#endif

// The number of bins in the histogram
#ifndef BINS
#define BINS 1024
#endif

#define SCATTER_BLOCK_DIM 32
#define SCATTER_BLOCK_SIZE (SCATTER_BLOCK_DIM*SCATTER_BLOCK_DIM)

// We use a single constant buffer for simplicity
cbuffer SDSMPartitionsConstants : register(b1)
{
    float4 mLightSpaceBorder;
    float4 mMaxScale;
    float mDilationFactor;
    uint mScatterTileDim;
    uint mReduceTileDim;
}

float ZToBin(float z)
{
    return floor(ZToNormalizedBin(z, mCameraNearFar.x, mCameraNearFar.y) * float(BINS));
}

float BinToZ(float bin)
{
    return NormalizedBinToZ(bin / float(BINS), mCameraNearFar.x, mCameraNearFar.y);
}

StructuredBuffer<HistogramBin> gHistogramReadOnly : register(t5);
RWStructuredBuffer<HistogramBin> gHistogram : register(u5);


// TODO: Maybe make a thread group out of this but this is the trivial mapping case with no shared
// memory usage so the driver really should be able to figure it out.
[numthreads(1, 1, 1)]
void ClearHistogram(uint3 globalThreadId : SV_DispatchThreadID)
{
    gHistogram[globalThreadId.x] = emptyBin();
}

groupshared HistogramBin sLocalHistogram[BINS];

[numthreads(SCATTER_BLOCK_DIM, SCATTER_BLOCK_DIM, 1)]
void ScatterHistogram(uint3 groupId        : SV_GroupID,
                      uint3 groupThreadId  : SV_GroupThreadID,
                      uint  groupIndex     : SV_GroupIndex)
{
    // Initialize local histogram
    {
        for (uint i = groupIndex; i < BINS; i += SCATTER_BLOCK_SIZE) {
            sLocalHistogram[i] = emptyBin();
        }
    }

    GroupMemoryBarrierWithGroupSync();

    // Iterate over the tile
    uint2 tileStart = groupId.xy * mScatterTileDim.xx + groupThreadId.xy;
    for (uint tileY = 0; tileY < mScatterTileDim; tileY += SCATTER_BLOCK_DIM) {
        for (uint tileX = 0; tileX < mScatterTileDim; tileX += SCATTER_BLOCK_DIM) {
            
            // Sample/compute surface data
            uint2 globalCoords = tileStart + uint2(tileX, tileY);
            // TODO: Could branch here to avoid reading off the edge of the GBuffer but it'll return
            // zero and fail the next test anyways, which is fine for now.
            SurfaceData data = ComputeSurfaceDataFromGBuffer(globalCoords);
            
            // Drop samples that fall outside the view frustum (clear color, etc)
            if (data.positionView.z >= mCameraNearFar.x && data.positionView.z < mCameraNearFar.y) {
                // Bin based on view space Z
                int bin = int(ZToBin(data.positionView.z));
                
                uint texCoordX = asuint(data.lightTexCoord.x);
                uint texCoordY = asuint(data.lightTexCoord.y);
                uint lightSpaceZ = asuint(data.lightTexCoord.z);
                
                // Scatter data to the right bin in our local histogram
                InterlockedAdd(sLocalHistogram[bin].count,             1);
                InterlockedMin(sLocalHistogram[bin].bounds.minCoord.x, texCoordX);
                InterlockedMin(sLocalHistogram[bin].bounds.minCoord.y, texCoordY);
                InterlockedMin(sLocalHistogram[bin].bounds.minCoord.z, lightSpaceZ);
                InterlockedMax(sLocalHistogram[bin].bounds.maxCoord.x, texCoordX);
                InterlockedMax(sLocalHistogram[bin].bounds.maxCoord.y, texCoordY);
                InterlockedMax(sLocalHistogram[bin].bounds.maxCoord.z, lightSpaceZ);
            }
        }
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    // Accumulate local histogram into the global one
    {
        for (uint i = groupIndex; i < BINS; i += SCATTER_BLOCK_SIZE) {
            if (sLocalHistogram[i].count > 0) {
                InterlockedAdd(gHistogram[i].count,             sLocalHistogram[i].count            );
                InterlockedMin(gHistogram[i].bounds.minCoord.x, sLocalHistogram[i].bounds.minCoord.x);
                InterlockedMin(gHistogram[i].bounds.minCoord.y, sLocalHistogram[i].bounds.minCoord.y);
                InterlockedMin(gHistogram[i].bounds.minCoord.z, sLocalHistogram[i].bounds.minCoord.z);
                InterlockedMax(gHistogram[i].bounds.maxCoord.x, sLocalHistogram[i].bounds.maxCoord.x);
                InterlockedMax(gHistogram[i].bounds.maxCoord.y, sLocalHistogram[i].bounds.maxCoord.y);
                InterlockedMax(gHistogram[i].bounds.maxCoord.z, sLocalHistogram[i].bounds.maxCoord.z);
            }
        }
    }
}


RWStructuredBuffer<Partition> gPartitions : register(u6);
RWStructuredBuffer<PartitionUint> gPartitionsUint : register(u6);
StructuredBuffer<Partition> gPartitionsReadOnly : register(t6);

groupshared uint sPartitionMaxBin[PARTITIONS];
groupshared BoundsUint sPartitionBoundsUint[PARTITIONS];

// Utility function that uses partitionsBounds to generate zoom regions (scale/bias)
// NOTE: This function can be optimized/simplified but it's only run once per cascade per frame
// and thus isn't performance critical. The math is left separated out for readibility.
void ComputePartitionDataFromBounds(BoundsFloat bounds, out float3 scale, out float3 bias)
{
    float3 minTexCoord = bounds.minCoord;
    float3 maxTexCoord = bounds.maxCoord;
        
    // Ensure our partitions have enough of a border for edge softening (blurring)
    // Also ensure our partition depth ranges stay far enough away from the geometry
    minTexCoord -= mLightSpaceBorder.xyz;
    maxTexCoord += mLightSpaceBorder.xyz;
        
    scale = 1.0f / (maxTexCoord - minTexCoord);
    bias = -minTexCoord * scale;

    // Dilate
    // TODO: This isn't necessarily required... we could instead just clamp mLightSpaceBorder
    // to something fairly small to cover anisotropic kernels, etc. in addition to
    // the blur. Leave it here for now though as the %-based dilation is convenient.
    float oneMinusTwoFactor = 1.0f - 2.0f * mDilationFactor;
    scale *= oneMinusTwoFactor;
    bias = mDilationFactor + oneMinusTwoFactor * bias;
	
	// Clamp to max scale
	float3 targetScale = min(scale, mMaxScale.xyz);

	// Quantize x/y if requested
	// TODO: Save pre-quantized scale/bias for use when frustum culling
	if (mUI.quantizePartitions) {
		// We need to round scale *down* to the nearest power of 2 (i.e. enlarge the partition)
		targetScale.xy = max(1.0f, exp2(floor(log2(targetScale.xy))));
		//targetScale.xy = min(targetScale.x, targetScale.y);
	}

    // Use target scale (but remain centered)
    float3 center = float3(0.5f, 0.5f, 0.5f);
    bias = (targetScale / scale) * (bias - center) + center;
    scale = targetScale;

	// If quantizing, snap bias to the appropriate texel size
	// NOTE: The dilation border above ensures that this doesn't result in missed samples
	if (mUI.quantizePartitions) {
		// Bias happens *after* scale so we can quantize it directly in shadow texture space
		float2 texelsInLightSpace = mShadowTextureDim.xy;
		bias.xy = floor(bias.xy * texelsInLightSpace) / texelsInLightSpace;
	}
	
    // Detect empty partitions
    if (any(maxTexCoord < minTexCoord)) {
        // Set it to a tiny region that shouldn't be overlapped by any geometry
        // NOTE: We could do this a bit more cleanly by just skipping empty partitions
        // entirely (on the CPU...) but this works for now.
        // TODO: Does this interact poorly with blur kernel scaling?
        scale = asfloat(0x7F7FFFFF).xxx;
        bias = scale;
    }
}


// Utility function that uses the sPartitionMaxBin partition locations and
// the histogram to generate partition bounds and zoom regions, which it then
// writes out to the partition UAV.
// NOTE: groupIndex should be [0, BINS)
// TODO: Handle BINS < PARTITIONS??
void ComputePartitionDataFromHistogram(uint groupIndex)
{
    // Initialize
    if (groupIndex < PARTITIONS) {
        sPartitionBoundsUint[groupIndex] = EmptyBoundsUint();
    }

    // Scatter/accumulate bounding volumes into the proper partitions
    // NOTE: This step is likely to have a fair amount of atomic contention, but it should be no worse than
    // to serializing the segmented min/max, and again it's a small enough data set that it's not worth
    // getting clever.
    if (gHistogramReadOnly[groupIndex].count > 0) {
        // Which partition are we in?
        uint partition = 0;
        [unroll] for (uint i = 0; i < (PARTITIONS - 1); ++i) {
            partition += (groupIndex >= sPartitionMaxBin[i] ? 1U : 0U);
        }
    
        InterlockedMin(sPartitionBoundsUint[partition].minCoord.x, gHistogramReadOnly[groupIndex].bounds.minCoord.x);
        InterlockedMin(sPartitionBoundsUint[partition].minCoord.y, gHistogramReadOnly[groupIndex].bounds.minCoord.y);
        InterlockedMin(sPartitionBoundsUint[partition].minCoord.z, gHistogramReadOnly[groupIndex].bounds.minCoord.z);
        InterlockedMax(sPartitionBoundsUint[partition].maxCoord.x, gHistogramReadOnly[groupIndex].bounds.maxCoord.x);
        InterlockedMax(sPartitionBoundsUint[partition].maxCoord.y, gHistogramReadOnly[groupIndex].bounds.maxCoord.y);
        InterlockedMax(sPartitionBoundsUint[partition].maxCoord.z, gHistogramReadOnly[groupIndex].bounds.maxCoord.z);
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    // Convert bounds to scales/biases and write to global memory
    if (groupIndex < PARTITIONS) {
        uint minBin = 0;
        if (groupIndex > 0) {
            minBin = sPartitionMaxBin[groupIndex - 1];
        }
        uint maxBin = sPartitionMaxBin[groupIndex];

        float3 scale, bias;
        ComputePartitionDataFromBounds(BoundsUintToFloat(sPartitionBoundsUint[groupIndex]), scale, bias);

        // NOTE: Ensure that it still covers the whole range of the framebuffer (expand first and last)
        // This does not affect the solution at all since we derive the bounds based on the samples, not the
        // partition frusta.
        gPartitions[groupIndex].intervalBegin =
            groupIndex == 0 ? mCameraNearFar.x : BinToZ(float(minBin));
        gPartitions[groupIndex].intervalEnd =
            groupIndex == (PARTITIONS - 1) ? mCameraNearFar.y : BinToZ(float(maxBin));
        gPartitions[groupIndex].scale = scale;
        gPartitions[groupIndex].bias = bias;
    }
}

#endif // SDSM_PARTITIONS_HLSL
