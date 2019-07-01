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

#include "Partitions.h"


static void ComputeAABBFromPoints(
	D3DXVECTOR4 *points,
	UINT numPoints,
	D3DXVECTOR3* outMin,
    D3DXVECTOR3* outMax)
{
    // NOTE: we don't do a perspective divide here since we actually rely on it being an ortho
    // projection in several places, including not doing any proper near plane clipping here.

    // Work out AABB of frustum in light view space
    D3DXVECTOR4 minCorner(points[0]);
    D3DXVECTOR4 maxCorner(points[0]);
    for (unsigned int i = 1; i < numPoints; ++i) {
        D3DXVec4Minimize(&minCorner, &minCorner, points + i);
        D3DXVec4Maximize(&maxCorner, &maxCorner, points + i);
    }

    outMin->x = minCorner.x;
    outMin->y = minCorner.y;
    outMin->z = minCorner.z;
    outMax->x = maxCorner.x;
    outMax->y = maxCorner.y;
    outMax->z = maxCorner.z;
}


void ComputeFrustumExtents(const D3DXMATRIXA16& cameraViewInv,
                           const D3DXMATRIXA16& cameraProj,
                           float nearZ, float farZ,
                           const D3DXMATRIXA16& lightViewProj,
                           D3DXVECTOR3* outMin,
                           D3DXVECTOR3* outMax)
{
    // Extract frustum points
    float scaleXInv = 1.0f / cameraProj._11;
    float scaleYInv = 1.0f / cameraProj._22;

    // Transform frustum corners into light view space
    D3DXMATRIXA16 cameraViewToLightProj = cameraViewInv * lightViewProj;

    D3DXVECTOR3 corners[8];
    // Near corners (in view space)
    float nearX = scaleXInv * nearZ;
    float nearY = scaleYInv * nearZ;
    corners[0] = D3DXVECTOR3(-nearX,  nearY, nearZ);
    corners[1] = D3DXVECTOR3( nearX,  nearY, nearZ);
    corners[2] = D3DXVECTOR3(-nearX, -nearY, nearZ);
    corners[3] = D3DXVECTOR3( nearX, -nearY, nearZ);
    // Far corners (in view space)
    float farX = scaleXInv * farZ;
    float farY = scaleYInv * farZ;
    corners[4] = D3DXVECTOR3(-farX,  farY, farZ);
    corners[5] = D3DXVECTOR3( farX,  farY, farZ);
    corners[6] = D3DXVECTOR3(-farX, -farY, farZ);
    corners[7] = D3DXVECTOR3( farX, -farY, farZ);

	D3DXVECTOR4 cornersLightView[8];
    D3DXVec3TransformArray(cornersLightView, sizeof(D3DXVECTOR4),
                           corners, sizeof(D3DXVECTOR3),
                           &cameraViewToLightProj, 8);

    ComputeAABBFromPoints(cornersLightView, 8, outMin, outMax);
}


void ComputeAABBExtents(const D3DXVECTOR3& aabbMin,
                        const D3DXVECTOR3& aabbMax,
                        const D3DXMATRIXA16& lightViewProj,
                        D3DXVECTOR3* outMin,
                        D3DXVECTOR3* outMax)
{
	D3DXVECTOR3 corners[8];
    corners[0] = D3DXVECTOR3(aabbMin.x, aabbMin.y, aabbMin.z);
    corners[1] = D3DXVECTOR3(aabbMin.x, aabbMin.y, aabbMax.z);
    corners[2] = D3DXVECTOR3(aabbMin.x, aabbMax.y, aabbMin.z);
    corners[3] = D3DXVECTOR3(aabbMin.x, aabbMax.y, aabbMax.z);
    corners[4] = D3DXVECTOR3(aabbMax.x, aabbMin.y, aabbMin.z);
    corners[5] = D3DXVECTOR3(aabbMax.x, aabbMin.y, aabbMax.z);
    corners[6] = D3DXVECTOR3(aabbMax.x, aabbMax.y, aabbMin.z);
    corners[7] = D3DXVECTOR3(aabbMax.x, aabbMax.y, aabbMax.z);

	D3DXVECTOR4 cornersLightView[8];
    D3DXVec3TransformArray(cornersLightView, sizeof(D3DXVECTOR4),
                           corners, sizeof(D3DXVECTOR3),
                           &lightViewProj, 8);

    ComputeAABBFromPoints(cornersLightView, 8, outMin, outMax);
}
