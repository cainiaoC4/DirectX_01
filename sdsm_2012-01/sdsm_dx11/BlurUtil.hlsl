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

#ifndef BLUR_UTIL_HLSL
#define BLUR_UTIL_HLSL

// Utility to work out blur metadata
struct BlurData
{
    uint interiorSamplesLoopCount : interiorSamplesLoopCount;
    float interiorSampleWeight : interiorSampleWeight;
    float edgeSampleWeight : edgeSampleWeight;
};

BlurData ComputeBlurData(float filterSizeTexels)
{
    BlurData output;
    
    // Just draw the diagram to understand... :)
    float sideSamples = 0.5f * filterSizeTexels - 0.5f;
    output.edgeSampleWeight = modf(sideSamples, output.interiorSamplesLoopCount);

    // Normalize weights
    output.interiorSampleWeight = 1.0f / filterSizeTexels;
    output.edgeSampleWeight *= output.interiorSampleWeight;
    
    return output;
}

#endif // BLUR_UTIL_HLSL