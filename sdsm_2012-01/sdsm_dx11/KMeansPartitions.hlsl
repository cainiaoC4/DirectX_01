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

#ifndef KMEANS_PARTITIONS_HLSL
#define KMEANS_PARTITIONS_HLSL

#include "SDSMPartitions.hlsl"

// Most of these need double-buffering in local memory
groupshared float sWeightPrefixSum[2][BINS];
groupshared float sZPrefixSum[2][BINS];
groupshared float sCentroids[2][PARTITIONS];

// Binary search
uint WeightUpperBound(uint scanInput, float value)
{
    uint first = 0;
    uint count = BINS;
    while (0 < count) {
        uint halfCount = count / 2;
        uint mid = first + halfCount;
        if (value >= sWeightPrefixSum[scanInput][mid]) {
            first = mid + 1;
            count -= halfCount + 1;
        } else {
            count = halfCount;
        }
    }
    return first;
}

// NOTE: This only runs in one cluster/group, which isn't 100% ideal
// but we're dealing with effectively serial code on a small data set,
// so the key here is that it's running on the GPU at all, and thus we
// don't need a round-trip back to the CPU which entails stalls, pipeline
// flushes, etc.
[numthreads(BINS, 1, 1)]
void ComputeKMeansPartitions(uint groupIndex : SV_GroupIndex)
{
    // Initialize from global memory
    uint scanInput = 0;
    float initCount = float(gHistogramReadOnly[groupIndex].count);
    
    sWeightPrefixSum[scanInput][groupIndex] = initCount;
    sZPrefixSum[scanInput][groupIndex] = initCount * BinToZ(float(groupIndex));
    
    GroupMemoryBarrierWithGroupSync();

    // Scan both count, and weighted bin index
    // Slowish nlog(n) way for now, but we're dealing with so little data
    // that it just needs to run on the GPU, not necessarily be super-parallel.
    for (uint offset = 1; offset < BINS; offset *= 2) {
        uint scanOutput = 1 - scanInput;
        
        float countSum = sWeightPrefixSum[scanInput][groupIndex];
        float binIndexSum = sZPrefixSum[scanInput][groupIndex];
        if (groupIndex >= offset) {
            countSum += sWeightPrefixSum[scanInput][groupIndex - offset];
            binIndexSum += sZPrefixSum[scanInput][groupIndex - offset];
        }
        sWeightPrefixSum[scanOutput][groupIndex] = countSum;
        sZPrefixSum[scanOutput][groupIndex] = binIndexSum;
        
        // Double-buffer
        scanInput = scanOutput;
        GroupMemoryBarrierWithGroupSync();
    }
    
    // Set up initial centroids by partitioning by equal weight
    uint lloydInput = 0;
    if (groupIndex < PARTITIONS) {
        float totalCount = sWeightPrefixSum[scanInput][BINS - 1];
        float weightPerSplit = totalCount / float(PARTITIONS + 1);
        float weightAtSplit = float(groupIndex + 1) * weightPerSplit;
        sCentroids[lloydInput][groupIndex] = BinToZ(float(WeightUpperBound(scanInput, weightAtSplit)));
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    // Lloyd iteration
    for (uint iteration = 0; iteration < mUI.lloydIterations; ++iteration) {
        uint lloydOutput = 1 - lloydInput;
        
        if (groupIndex < PARTITIONS) {
            float leftBin = 0.0f;
            if (groupIndex > 0) {
                leftBin = ZToBin(0.5f * (sCentroids[lloydInput][groupIndex - 1] + sCentroids[lloydInput][groupIndex]));
            }
            float rightBin = float(BINS);       // Exclusive on this end
            if (groupIndex < (PARTITIONS - 1)) {
                rightBin = ZToBin(0.5f * (sCentroids[lloydInput][groupIndex] + sCentroids[lloydInput][groupIndex + 1]));
            }
            
            int leftIndex = int(leftBin);
            int rightIndex = int(rightBin);
            
            // Our prefix sum is inclusive, but the lookup on the right should be exclusive
            --leftIndex;
            --rightIndex;
            
            float weightLeft = 0.0f;
            float weightRight = 0.0f;
            float zLeft = 0.0f;
            float zRight = 0.0f;
            [flatten] if (leftIndex >= 0) {
                weightLeft = sWeightPrefixSum[scanInput][leftIndex];
                zLeft = sZPrefixSum[scanInput][leftIndex];
            }
            [flatten] if (rightIndex >= 0) {
                weightRight = sWeightPrefixSum[scanInput][rightIndex];
                zRight = sZPrefixSum[scanInput][rightIndex];
            }
            
            float weightSum = weightRight - weightLeft;
            float zSum = zRight - zLeft;
            
            sCentroids[lloydOutput][groupIndex] = weightSum > 0.0f
                                                          ? zSum / weightSum
                                                          : sCentroids[lloydInput][groupIndex];
        }
        
        // Double-buffer
        lloydInput = lloydOutput;
        GroupMemoryBarrierWithGroupSync();
    }
    
    // Work out split bin boundaries
    if (groupIndex < PARTITIONS) {
        uint splitBinRight = BINS;   // Exclusive on this end
        if (groupIndex < (PARTITIONS - 1)) {
            splitBinRight = uint(ZToBin(lerp(sCentroids[lloydInput][groupIndex], sCentroids[lloydInput][groupIndex + 1], 0.5f)));
        }
        
        sPartitionMaxBin[groupIndex] = splitBinRight;
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    ComputePartitionDataFromHistogram(groupIndex);
}

#endif // KMEANS_PARTITIONS_HLSL
