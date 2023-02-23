DirectX Raytracing Tutorials
============
This repository contain tutorials demonstrating how to use DirectX Raytracing.

Requirements:
- A GPU that supports DXR (Such as NVIDIA's Volta or Turing hardware)
- Windows 10 RS5 (version 1809) or newer
- [Windows 10 SDK version 1809 (10.0.17763.0)](https://developer.microsoft.com/en-us/windows/downloads/sdk-archive)
- Visual Studio 2022

# DXR Tutorial 08
## Instancing
## Overview
Now that we know how to invoke the ray-tracing pipeline, we can get into more advanced usage. We will start with something simple – instancing.
Instancing has 2 inputs:
1.	Number of instances to render.
2.	The transformation matrix for each instance.
In the rasterization API, we set those inputs by 
(1)	Passing (InstanceCount > 1) to DrawInstanced() or DrawIndexedInstanced().
(2)	Using SV_InstanceID to control the transformation of each instance.

In DXR both inputs are set during the creation of the top-level acceleration-structure (TLAS).
Acceleration Structures Revisited
Recall that we have 2 types of acceleration structures – top-level and bottom-level.
The bottom-level acceleration structure is the one that holds the geometric data – vertex and index buffers, strides, and vertex count. Conceptually, we can think of it as a mesh in local space.
The top-level acceleration structure then references the bottom-level acceleration structures we created. For each reference, we can optionally specify a local→world transformation matrix. Instancing is achieved by referencing the same bottom-level acceleration structure multiple times with different matrices.
Code Walkthrough
We are going to modify our application to render 3 instances of the triangle.
There is no need to change the creation of the bottom-level acceleration structure. We only need to make a small change to the TLAS creation code - createTopLevelAS().

The first thing we need is to change the call to GetRaytracingAccelerationStructurePrebuildInfo(). We need to request the information for 3 instances – specified by the NumDescs field of D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS. Also, since this struct is reused when building the acceleration structure, this change only happens in one place.
Then, we need to change the size of the D3D12_RAYTRACING_INSTANCE_DESC buffer. We need size for 3 descriptors.

Next, let’s create the transformation matrices.



Now we can go ahead and initialize the D3D12_RAYTRACING_INSTANCE_DESC buffer.
It’s very similar to the code we had before, but there are some things to note.
First, we set a different InstanceID per instance. It doesn’t have to be in sequential order (i.e. `i`). We can set it to whatever arbitrary value we want. The ray-tracing pipeline doesn’t use this value. It will be communicated to the hit-shader via the InstanceID() intrinsic.
Next, note that we are using the same InstanceContributionToHitGroupIndex. This means that we will use the same shader-table record for all instances. That’s fine – we do not have any per-instance data in the hit-records.
Finally, we need to transpose our transformation matrix. This is an implementation detail – our math library uses column-major matrices while DRXT expects Transform in row-major format.
And we’re good to go. No other changes are required, we can run the application and see this image.
 

InstanceID()
Actually, if you run the tutorial code you’ll see a different image then the one above. That’s because we also made a small change to the closest-hit shader (08-Shaders.hlsl). At the beginning of the shader, you can see the following line


This value will receive the value we specified when we created the TLAS (D3D12_RAYTRACING_INSTANCE_DESC::InstanceID).
Based on this value we change the color interpolation order. The result is 3 different looking triangles.
 

