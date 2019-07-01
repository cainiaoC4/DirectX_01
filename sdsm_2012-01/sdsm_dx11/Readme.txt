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

Date
====
July 2010


Sample Distribution Shadow Maps Demo
====================================

This application demonstrates the basic Sample Distribution Shadow Maps (SDSM)
algorithm. 


User Interaction
================

- As with other DXSDK samples, use WASD with the mouse (click and drag) to fly
  around the scene.
- Press F8 to display the "Expert UI", which exposes additional parameters to
  users familiar with the underlying algorithms.


Requirements
============

For compilation:

1) Visual C++ 2008 SP1 (makes some use of tr1::shared_ptr) or Visual C++ 2010
2) DirectX SDK (June 2010 or newer)

For execution:

1) Windows Vista or Windows 7
2) DirectX 11 compatible video card
3) Appropriate Visual C++ Redistributable Package (2008 SP1/2010)


Known Issues
============
   
1) Framebuffer MSAA is not currently supported. The demo uses deferred
   rendering which requires special handling of MSAA samples, which has not yet
   been implemented. However, shadow map MSAA is supported when EVSM filtering
   is selected.
   
2) Flickering shadows or abrupt transitions on surfaces nearly parallel to the
   light rays are often due to bad shading normals in the art assets. Please
   check the "Face Normals" option in the GUI to demonstrate faces that are
   actually back facing to the light, but have incorrect shading normals.
   
   
Code Overview
=============

main.cpp contains the majority of the DXUT and UI code, while App.[cpp/h]
contains the majority of the application code. In particular, the App
constructor initializes all shaders and resources required by the algorithms,
and the App::Render function (and children) handle rendering a frame.

Rendering generally proceeds as follows (see App:Render):

1) All geometry is rendered to the G-buffer
2) Partitions locations are generated in one of several ways:
  2a) With Adaptive Log Partitioning, the G-buffer data is analyzed for gaps in
      depth and log-based partitions are placed with those gaps
  2b) With Log Partitioning, the G-buffer data is analyzed to find the min/max
      z and then log partitions are placed to cover that single range
  2c) With k-means partitioning, the G-buffer data is analyzed using k-means
      clustering of the depth histogram
  2b) With PSSM partitioning, the partitions are statically placed
3) For each pass:
  3a) For each partition in this pass:
    3ai) Render simple depth shadow map
    3aii) [EVSM Filtering] Convert shadow map to EVSM representation
    3aiii) [EVSM Filtering] Optionally prefilter/blur the EVSM
  3b) Accumulate lighting for all partitions in this pass

The resolution management algorithms that analyze the g-buffer are implemented 
in SDSMPartitions.[cpp/h/hlsl]. The entire algorithms are implemented on the
GPU in the [*]Partitions.hlsl files, with the C++ source code setting up and 
invoking it.

Filtering code specific to Exponential Variance Shadow Maps can be found in 
RenderingEVSM.hlsl. Percentage closer filtering code is in RenderingPCF.hlsl.
The rest of the shaders and rendering utilities are in Rendering.hlsl and 
GBuffer.hlsl. Visualization-related shaders are in Visualize.hlsl.
