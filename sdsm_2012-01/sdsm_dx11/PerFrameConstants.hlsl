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

#ifndef PER_FRAME_CONSTANTS_HLSL
#define PER_FRAME_CONSTANTS_HLSL

struct UIConstants
{
    uint lightingOnly;
    uint faceNormals;
    uint partitionScheme;
    uint visualizePartitions;
    uint visualizeHistogram;
    uint visualizeError;
    uint edgeSoftening;
    float edgeSofteningAmount;
    float maxEdgeSofteningFilter;
    float pssmFactor;
    uint alignLightToFrustum;
    uint cpuPartitionFrustumCulling;
    uint tightPartitionBounds;
    float histogramResolutionPower;
    uint lloydIterations;
    uint usePositiveExponent;
    float positiveExponent;
    uint useNegativeExponent;
    float negativeExponent;
    uint visualizeLightSpace;
	uint quantizePartitions;
};

cbuffer PerFrameConstants : register(b0)
{
    float4x4 mCameraWorldViewProj;
    float4x4 mCameraWorldView;
    float4x4 mCameraViewProj;
    float4x4 mCameraProj;
    float4x4 mLightWorldViewProj;
    float4x4 mCameraViewToLightProj;
    float4 mLightDir;
    float4 mBlurSizeLightSpace;
    float4 mCameraNearFar;
	float4 mShadowTextureDim;
    
    UIConstants mUI;
};

#endif // PER_FRAME_CONSTANTS_HLSL