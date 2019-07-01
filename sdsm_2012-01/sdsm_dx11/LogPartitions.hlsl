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

#ifndef LOG_PARTITIONS_HLSL
#define LOG_PARTITIONS_HLSL

#include "SDSMPartitions.hlsl"


// partition index should be in range [0, PARTITIONS] inclusive
float LogPartitionFromRange(uint partition, float minZ, float maxZ)
{
    float z = maxZ;   // Exclusive on this end
    if (partition < PARTITIONS) {
        float ratio = maxZ / minZ;
        float power = float(partition) * (1.0f / float(PARTITIONS));
        z = minZ * pow(ratio, power);
    }
    return z;
}


groupshared uint sMinBin;
groupshared uint sMaxBin;

[numthreads(BINS, 1, 1)]
void ComputeLogPartitionsFromHistogram(uint groupIndex : SV_GroupIndex)
{
    if (groupIndex == 0) {
        sMinBin = BINS;
        sMaxBin = 0;
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    if (gHistogramReadOnly[groupIndex].count > 0) {
        InterlockedMin(sMinBin, groupIndex);
        InterlockedMax(sMaxBin, groupIndex + 1);
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    if (groupIndex < PARTITIONS) {
        sPartitionMaxBin[groupIndex] = ZToBin(LogPartitionFromRange(groupIndex + 1, BinToZ(sMinBin), BinToZ(sMaxBin)));
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    ComputePartitionDataFromHistogram(groupIndex);
}


#define REDUCE_ZBOUNDS_BLOCK_DIM 16
#define REDUCE_ZBOUNDS_BLOCK_SIZE (REDUCE_ZBOUNDS_BLOCK_DIM*REDUCE_ZBOUNDS_BLOCK_DIM)

groupshared float sMinZ[REDUCE_ZBOUNDS_BLOCK_SIZE];
groupshared float sMaxZ[REDUCE_ZBOUNDS_BLOCK_SIZE];

// Clear out all the intervals just in case
[numthreads(PARTITIONS, 1, 1)]
void ClearZBounds(uint groupIndex : SV_GroupIndex)
{
    gPartitionsUint[groupIndex].intervalBegin = 0x7F7FFFFF;       // Float max
    gPartitionsUint[groupIndex].intervalEnd = 0;
}

[numthreads(REDUCE_ZBOUNDS_BLOCK_DIM, REDUCE_ZBOUNDS_BLOCK_DIM, 1)]
void ReduceZBoundsFromGBuffer(uint3 groupId       : SV_GroupID,
                              uint3 groupThreadId : SV_GroupThreadID,
                              uint  groupIndex    : SV_GroupIndex)
{
    uint2 gbufferDim;
    gGBufferTextures[0].GetDimensions(gbufferDim.x, gbufferDim.y);

    // Initialize stack copy of reduction data for this thread
    float minZ = mCameraNearFar.y;
    float maxZ = mCameraNearFar.x;
    
    // Loop over tile and reduce into local memory
    {
        uint2 tileStart = groupId.xy * mReduceTileDim.xx + groupThreadId.xy;
        for (uint tileY = 0; tileY < mReduceTileDim; tileY += REDUCE_ZBOUNDS_BLOCK_DIM) {
            for (uint tileX = 0; tileX < mReduceTileDim; tileX += REDUCE_ZBOUNDS_BLOCK_DIM) {
                uint2 globalCoords = tileStart + uint2(tileX, tileY);
                SurfaceData data = ComputeSurfaceDataFromGBuffer(globalCoords);
                if (data.positionView.z >= mCameraNearFar.x && data.positionView.z < mCameraNearFar.y) {
                    minZ = min(minZ, data.positionView.z);
                    maxZ = max(maxZ, data.positionView.z);
                }
            }
        }
    }
    
    // Copy result to shared memory for reduction
    sMinZ[groupIndex] = minZ;
    sMaxZ[groupIndex] = maxZ;

    GroupMemoryBarrierWithGroupSync();

    // Reduce our local memory data set to one element
    // TODO: Switch to local atomics for last few iterations
    for (uint offset = (REDUCE_ZBOUNDS_BLOCK_SIZE >> 1); offset > 0; offset >>= 1) {
        if (groupIndex < offset) {
            sMinZ[groupIndex] = min(sMinZ[groupIndex], sMinZ[offset + groupIndex]);
            sMaxZ[groupIndex] = max(sMaxZ[groupIndex], sMaxZ[offset + groupIndex]);
        }
        GroupMemoryBarrierWithGroupSync();
    }

    // Now write out the result from this pass to the partition data
    // We'll fill in the intermediate intervals in a subsequent pass
    if (groupIndex == 0) {
        // Just use scatter atomics for now... we can switch to a global reduction if necessary later
        // Note that choosing good tile dimensions to "just" fill the machine should keep this efficient
        InterlockedMin(gPartitionsUint[0].intervalBegin, asuint(sMinZ[0]));
        InterlockedMax(gPartitionsUint[PARTITIONS - 1].intervalEnd, asuint(sMaxZ[0]));
    }
}

[numthreads(PARTITIONS, 1, 1)]
void ComputeLogPartitionsFromZBounds(uint groupIndex : SV_GroupIndex)
{
    // Grab min/max Z from previous reduction
    float minZ = gPartitions[0].intervalBegin;
    float maxZ = gPartitions[PARTITIONS - 1].intervalEnd;

    // Work out partition intervals
    // NOTE: Ensure that it still covers the whole range of the framebuffer (expand first and last)
    // This does not affect the solution at all since we derive the bounds based on the samples, not the
    // partition frusta.
    gPartitions[groupIndex].intervalBegin =
        groupIndex == 0 ? mCameraNearFar.x : LogPartitionFromRange(groupIndex, minZ, maxZ);
    gPartitions[groupIndex].intervalEnd =
        groupIndex == (PARTITIONS - 1) ? mCameraNearFar.y : LogPartitionFromRange(groupIndex + 1, minZ, maxZ);
}

#endif // LOG_PARTITIONS_HLSL
