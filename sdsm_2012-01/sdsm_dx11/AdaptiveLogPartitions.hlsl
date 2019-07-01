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

#ifndef ADAPTIVE_LOG_PARTITIONS_HLSL
#define ADAPTIVE_LOG_PARTITIONS_HLSL

#include "SDSMPartitions.hlsl"

groupshared uint sMinBin;
groupshared uint sMaxBin;
groupshared uint sEmptyStep[2][BINS];

[numthreads(BINS, 1, 1)]
void ComputeAdaptiveLogPartitions(uint groupIndex : SV_GroupIndex)
{
    // Initialize from global memory
    uint scanInput = 0;
    uint count = gHistogramReadOnly[groupIndex].count;
    sEmptyStep[scanInput][groupIndex] = (count == 0 ? 1 : 0);
    
    GroupMemoryBarrierWithGroupSync();

    bool done = false;
    for (uint offset = 1; offset < BINS; offset *= 2) {
        uint scanOutput = 1 - scanInput;
    
        uint currentStep = sEmptyStep[scanInput][groupIndex];
        uint nextStep = sEmptyStep[scanInput][groupIndex + currentStep];
        sEmptyStep[scanOutput][groupIndex] = currentStep + nextStep;
        
        // Double-buffer
        scanInput = scanOutput;
        GroupMemoryBarrierWithGroupSync();
    }
    
    // Find the minimum and maximum populated bins
    if (groupIndex == 0) {
        sMinBin = sEmptyStep[scanInput][0];
        sMaxBin = 0;
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    if (count > 0) {
        InterlockedMax(sMaxBin, groupIndex + 1);
    }

    GroupMemoryBarrierWithGroupSync();
    
    // Place partitions using adaptive log partitioning scheme
    // TODO: This can probably be slightly more parallelized
    if (groupIndex == 0) {
        float maxZ = BinToZ(sMaxBin);
        uint minBin = sMinBin;
        
        for (uint i = 0; i < PARTITIONS - 1; ++i) {
            // Find the end of this partition
            float minZ = BinToZ(minBin);
            float ratio = maxZ / minZ;
            float power = 1.0f / float(PARTITIONS - i);
            uint nextPartitionBin = ZToBin(minZ * pow(ratio, power));
        
            // Push it out until it has a non-zero count
            nextPartitionBin += sEmptyStep[scanInput][nextPartitionBin];
            
            // Add current bin
            sPartitionMaxBin[i] = nextPartitionBin;
            minBin = nextPartitionBin;
        }
        
        sPartitionMaxBin[PARTITIONS - 1] = BINS;
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    ComputePartitionDataFromHistogram(groupIndex);
}

#endif // ADAPTIVE_LOG_PARTITIONS_HLSL
