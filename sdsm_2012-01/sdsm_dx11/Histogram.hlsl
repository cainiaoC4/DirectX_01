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

#ifndef HISTOGRAM_HLSL
#define HISTOGRAM_HLSL

#include "PerFrameConstants.hlsl"

// Uint version of the bounds structure for atomic usage
// NOTE: This version cannot represent negative numbers!
struct BoundsUint
{
    uint3 minCoord;
    uint3 maxCoord;
};

// Reset bounds to [0, maxFloat]
BoundsUint EmptyBoundsUint()
{
    BoundsUint b;
    b.minCoord = uint(0x7F7FFFFF).xxx;      // Float max
    b.maxCoord = uint(0).xxx;               // NOTE: Can't be negative!!
    return b;
}

// Float version of structure for convenient
// NOTE: We still tend to maintain the non-negative semantics of the above for consistency
struct BoundsFloat
{
    float3 minCoord;
    float3 maxCoord;
};

BoundsFloat BoundsUintToFloat(BoundsUint u)
{
    BoundsFloat f;
    f.minCoord.x = asfloat(u.minCoord.x);
    f.minCoord.y = asfloat(u.minCoord.y);
    f.minCoord.z = asfloat(u.minCoord.z);
    f.maxCoord.x = asfloat(u.maxCoord.x);
    f.maxCoord.y = asfloat(u.maxCoord.y);
    f.maxCoord.z = asfloat(u.maxCoord.z);
    return f;
}

BoundsFloat EmptyBoundsFloat()
{
    return BoundsUintToFloat(EmptyBoundsUint());
}

// All data stored in a histogram bin
struct HistogramBin
{
    uint count;
    // Use uint version since we use atomics
    BoundsUint bounds;
};

HistogramBin emptyBin()
{
    HistogramBin b;
    b.count  = 0;
    b.bounds = EmptyBoundsUint();
    return b;
}

float ZToBinWarp(float z)
{
    return pow(z, (1.0f / mUI.histogramResolutionPower));
}

float BinToZWarp(float z)
{
    return pow(z, mUI.histogramResolutionPower);
}

float ZToNormalizedBin(float z, float near, float far)
{
    return ZToBinWarp((z - near) / (far - near));
}

float NormalizedBinToZ(float bin, float near, float far)
{
    return BinToZWarp(bin) * (far - near) + near;
}

#endif // HISTOGRAM_HLSL
