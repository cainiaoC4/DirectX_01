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

#ifndef CUSTOM_PARTITIONS_HLSL
#define CUSTOM_PARTITIONS_HLSL

#include "SDSMPartitions.hlsl"

// Store reduction results
RWStructuredBuffer<BoundsUint> gPartitionBoundsUint : register(u7);
StructuredBuffer<BoundsFloat> gPartitionBoundsReadOnly : register(t7);

// TODO: This actually needs to be modified to ensure that it fits in local memory for any number of partitions!
// Needs to be based on max local memory size and max partition count.
// Currently set to 128 threads which works with up to 10 partitions (given 32KB local memory)
#define REDUCE_BOUNDS_BLOCK_X 16
#define REDUCE_BOUNDS_BLOCK_Y 8
#define REDUCE_BOUNDS_BLOCK_SIZE (REDUCE_BOUNDS_BLOCK_X*REDUCE_BOUNDS_BLOCK_Y)

// Store these as raw float arrays for reduction efficiency:
// Grouped by PARTITIONS
// Then grouped by SV_GroupIndex
#define REDUCE_BOUNDS_SHARED_MEMORY_ARRAY_SIZE (PARTITIONS * REDUCE_BOUNDS_BLOCK_SIZE)
groupshared float3 sBoundsMin[REDUCE_BOUNDS_SHARED_MEMORY_ARRAY_SIZE];
groupshared float3 sBoundsMax[REDUCE_BOUNDS_SHARED_MEMORY_ARRAY_SIZE];

[numthreads(PARTITIONS, 1, 1)]
void ClearPartitionBounds(uint  groupIndex : SV_GroupIndex)
{
    gPartitionBoundsUint[groupIndex] = EmptyBoundsUint();
}

[numthreads(REDUCE_BOUNDS_BLOCK_X, REDUCE_BOUNDS_BLOCK_Y, 1)]
void ReduceBoundsFromGBuffer(uint3 groupId       : SV_GroupID,
                             uint3 groupThreadId : SV_GroupThreadID,
                             uint  groupIndex    : SV_GroupIndex)
{
    uint2 gbufferDim;
    gGBufferTextures[0].GetDimensions(gbufferDim.x, gbufferDim.y);

    // Initialize stack copy of partition data for this thread
    BoundsFloat boundsReduce[PARTITIONS];
    {
        [unroll] for (uint partition = 0; partition < PARTITIONS; ++partition) {
            boundsReduce[partition] = EmptyBoundsFloat();
        }
    }
    
    // Loop over tile and reduce into local memory
    float nearZ = gPartitionsReadOnly[0].intervalBegin;
    float farZ = gPartitionsReadOnly[PARTITIONS - 1].intervalEnd;
    {
        uint2 tileStart = groupId.xy * mReduceTileDim.xx + groupThreadId.xy;
        for (uint tileY = 0; tileY < mReduceTileDim; tileY += REDUCE_BOUNDS_BLOCK_Y) {
            for (uint tileX = 0; tileX < mReduceTileDim; tileX += REDUCE_BOUNDS_BLOCK_X) {
                // Sample/compute surface data
                uint2 globalCoords = tileStart + uint2(tileX, tileY);
                SurfaceData data = ComputeSurfaceDataFromGBuffer(globalCoords);
                
                // Drop samples that fall outside the view frustum (clear color, etc)
                if (data.positionView.z >= nearZ && data.positionView.z < farZ) {
                    uint partition = 0;
                    [unroll] for (uint i = 0; i < (PARTITIONS - 1); ++i) {
                        [flatten] if (data.positionView.z >= gPartitionsReadOnly[i].intervalEnd) {
                            ++partition;
                        }
                    }

                    // Update relevant partition data for this thread
                    // This avoids the need for atomics since we're the only thread accessing this data
                    boundsReduce[partition].minCoord = min(boundsReduce[partition].minCoord, data.lightTexCoord.xyz);
                    boundsReduce[partition].maxCoord = max(boundsReduce[partition].maxCoord, data.lightTexCoord.xyz);
                }
            }
        }
    }
    
    
    // Copy result to shared memory for reduction
    {
        [unroll] for (uint partition = 0; partition < PARTITIONS; ++partition) {
            uint index = (groupIndex * PARTITIONS + partition);
            sBoundsMin[index] = boundsReduce[partition].minCoord;
            sBoundsMax[index] = boundsReduce[partition].maxCoord;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    // Now reduce our local memory data set to one element
    for (uint offset = (REDUCE_BOUNDS_SHARED_MEMORY_ARRAY_SIZE >> 1); offset >= PARTITIONS; offset >>= 1) {
        for (uint i = groupIndex; i < offset; i += REDUCE_BOUNDS_BLOCK_SIZE) {
            sBoundsMin[i] = min(sBoundsMin[i], sBoundsMin[offset + i]);
            sBoundsMax[i] = max(sBoundsMax[i], sBoundsMax[offset + i]);
        }
        GroupMemoryBarrierWithGroupSync();
    }

    // Now write out the result from this pass
    if (groupIndex < PARTITIONS) {
        InterlockedMin(gPartitionBoundsUint[groupIndex].minCoord.x, asuint(sBoundsMin[groupIndex].x));
        InterlockedMin(gPartitionBoundsUint[groupIndex].minCoord.y, asuint(sBoundsMin[groupIndex].y));
        InterlockedMin(gPartitionBoundsUint[groupIndex].minCoord.z, asuint(sBoundsMin[groupIndex].z));
        InterlockedMax(gPartitionBoundsUint[groupIndex].maxCoord.x, asuint(sBoundsMax[groupIndex].x));
        InterlockedMax(gPartitionBoundsUint[groupIndex].maxCoord.y, asuint(sBoundsMax[groupIndex].y));
        InterlockedMax(gPartitionBoundsUint[groupIndex].maxCoord.z, asuint(sBoundsMax[groupIndex].z));
    }
}

[numthreads(PARTITIONS, 1, 1)]
void ComputeCustomPartitions(uint groupIndex : SV_GroupIndex)
{
    // Compute scale/bias and update bounds in partition UAV
    float3 scale, bias;
    ComputePartitionDataFromBounds(gPartitionBoundsReadOnly[groupIndex], scale, bias);
    
    gPartitions[groupIndex].scale = scale;
    gPartitions[groupIndex].bias = bias;
}

#endif // CUSTOM_PARTITIONS_HLSL
