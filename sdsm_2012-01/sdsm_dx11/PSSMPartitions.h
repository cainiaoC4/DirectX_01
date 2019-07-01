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

#pragma once

#include "Partitions.h"
#include <d3d11.h>
#include <vector>

class PSSMPartitions
{
public:
    PSSMPartitions(ID3D11Device *d3dDevice, int partitions);

    ~PSSMPartitions();

    // Compute PSSM-placed partitions
    // Returns SRV that contains partition data
	// NOTE: Set quantizeResolution to 0 to not do any quantization. Otherwise set to cascade size.
    ID3D11ShaderResourceView* ComputePartitions(
        ID3D11DeviceContext *d3dDeviceContext,
        const D3DXMATRIXA16& cameraViewInv,
        const D3DXMATRIXA16& cameraProj,
        float minZ, float maxZ,
        const D3DXMATRIXA16& lightView,
		const D3DXMATRIXA16& lightProj,
        float lambda,
        const D3DXVECTOR3& lightSpaceBorder,
		unsigned int quantizeResolution = 0);

private:
	// No copying
	PSSMPartitions(const PSSMPartitions&);
	PSSMPartitions& operator=(const PSSMPartitions&);

    int mPartitions;

    ID3D11Buffer* mPartitionBuffer;
    ID3D11ShaderResourceView* mPartitionSRV;

    std::vector<float> mPSSMDistances;
};
