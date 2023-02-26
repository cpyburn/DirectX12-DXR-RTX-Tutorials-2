DirectX Raytracing Tutorials
============
This repository contain tutorials demonstrating how to use DirectX Raytracing.

Requirements:
- A GPU that supports DXR (Such as NVIDIA's Volta or Turing hardware)
- Windows 10 RS5 (version 1809) or newer
- [Windows 10 SDK version 1809 (10.0.17763.0)](https://developer.microsoft.com/en-us/windows/downloads/sdk-archive)
- Visual Studio 2022

# DXR Tutorial 14

## 14.0 Refit

## Overview
So far, we’ve only handled static meshes. We created the top-level acceleration structure once and
assumed that the scene stays static.

In this tutorial, we will learn how to handle dynamic objects by making the outer triangles rotate.

## Rebuild vs Refit
We can animate objects by manipulating their transformation matrix used when creating the TLAS
(D3D12_RAYTRACING_INSTANCE_DESC::Transform).
There are 2 options to update the TLAS:
- Rebuild – Creates the TLAS from scratch. Doesn’t use any information from previous builds.
- Refit – Update an existing TLAS.

According to the spec, there are different pros and cons for each option. The refit operation is usually
faster than rebuild, but traversing a TLAS that supports updates might be slower. As we’ll see in a
second, it’s straightforward to switch between the 2 options. This makes it very simple to benchmark
both options.

We already know how to build (and therefore rebuild) a TLAS. This tutorial will focus on refit.

## Refitting a TLAS
The code for refitting a TLAS is almost identical to the code creating a TLAS. We need to go through the
same steps – allocating scratch, result, and instance-desc buffers, initializing the instance descriptors,
and calling BuildRaytracingAccelerationStructure()).

There are 3 differences in the arguments we pass to BuildRaytracingAccelerationStructure():
1. We need to create the TLAS with the
D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE flag. We also need to
pass this flag to GetRaytracingAccelerationStructurePrebuildInfo().
2. When refitting, we need to set the
D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE of
D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC.
3. When refitting, we need to set a source TLAS buffer into the
SourceAccelerationStructureData field of
D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC.

Conceptually, these are the only differences. That’s good news, as it means we already have most of
what we need to support animation.

## 14.1 Code Walkthrough
First, let’s change the code that creates the TLAS. We renamed it and changed the signature.
```c++
// 14.1.a
void buildTopLevelAS(ID3D12Device5Ptr pDevice, ID3D12GraphicsCommandList4Ptr pCmdList, ID3D12ResourcePtr pBottomLevelAS[2], uint64_t& tlasSize, float rotation, bool update, Tutorial01::AccelerationStructureBuffers& buffers)
```

The last 3 arguments are new:
* rotation – Rotation in radians relative to the Y axis. We will apply this rotation to the 2 outer
triangles.
* update – True if this is a refit operation, otherwise false. Remember that we must create the
TLAS once before we can update it.
* buffers – Up to now we’ve only stored the result buffer. To avoid reallocating the scratch and
instance-desc buffers every frame, we will store them as members.

First, we query for the required buffer sizes for a TLAS that supports updating by passing the
D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE flag.
```c++
// 14.1.b
inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
```

If this is an update operation, we need to insert a UAV barrier for the TLAS buffer. In this tutorial, we
request for an update after the TLAS was used in a DispatchRay() call, which reads from the buffer. We
are going to write into the buffer, so a UAV barrier is required to ensure we do not overwrite data that is
currently in use.
```c++
// 14.1.c
if (update)
{
    // If this a request for an update, then the TLAS was already used in a DispatchRay() call. We need a UAV barrier to make sure the read operation ends before updating the buffer
    D3D12_RESOURCE_BARRIER uavBarrier = {};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = buffers.pResult;
    pCmdList->ResourceBarrier(1, &uavBarrier);
}
else
{
    // Create the buffers
    buffers.pScratch = createBuffer(pDevice, info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
    buffers.pResult = createBuffer(pDevice, info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, kDefaultHeapProps);
    // The instance desc should be inside a buffer, create and map the buffer
    // 8.0.b
    buffers.pInstanceDesc = createBuffer(pDevice, sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * 3, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
    tlasSize = info.ResultDataMaxSizeInBytes;
}
```

If it’s not an update operation, then we will allocate the buffers required for the TLAS creation.

Next, when creating the instance descriptors, you can see we apply rotation to the outer triangles.
```c++
// 14.1.d
mat4 rotationMat = eulerAngleY(rotation);
transformation[1] = translate(mat4(), vec3(-2, 0, 0)) * rotationMat;
transformation[2] = translate(mat4(), vec3(2, 0, 0)) * rotationMat;
```

Finally, if this is an update operation, we set the source buffer and the perform-update flag into the
D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC struct used when calling
BuildRaytracingAccelerationStructure().
```c++
// 14.1.e If this is an update operation, set the source buffer and the perform_update flag
if (update)
{
    asDesc.Inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
    asDesc.SourceAccelerationStructureData = buffers.pResult->GetGPUVirtualAddress();
}
```

**NOTE:** There’s a limitation with the current implementation where the source buffer must also be the
result buffer.

Finally, we can record a build command. Notice that we use the ALLOW_UPDATE flag, pass update and use
the source buffer we computed before.

Now it’s time to use this function.

## 14.2 Load Time TLAS Creation
The only thing that changed in createAccelerationStructures() is the fact that we now call the
function by its new name and request a `create` operation (see line 465).
```c++
// 14.2.a
buildTopLevelAS(mpDevice, mpCmdList, mpBottomLevelAS, mTlasSize, mRotation, true, mpTopLevelAS);
```

Add the member variables
```c++
// 14.2.b
float mRotation = 0;
```

## 14.3 Render-Time TLAS Refit
We added 4 lines of code to the beginning of onFrameRender() and we call buildTopLevelAS() and request an update operation and update the rotation.
```c++
// 14.3.a Refit the top-level acceleration structure and set update to false
buildTopLevelAS(mpDevice, mpCmdList, mpBottomLevelAS, mTlasSize, mRotation, false, mpTopLevelAS);
mRotation += 0.005f;
```
Move AccelartionStructures to the .h file and reference it by adding Tutorial01:: to all the references in .cpp
```c++
// 14.3.b bottom-level acceleration structure
struct AccelerationStructureBuffers
{
    ID3D12ResourcePtr pScratch;
    ID3D12ResourcePtr pResult;
    ID3D12ResourcePtr pInstanceDesc;    // Used only for top-level AS
};
```
Update the mpTopLevelAS in the .h file to a object type of AccelerationStructureBuffers
```c++
// 14.3.c
AccelerationStructureBuffers mpTopLevelAS;
```
Finally update the createShaderResources()
```c++
// 14.3.d
srvDesc.RaytracingAccelerationStructure.Location = mpTopLevelAS.pResult->GetGPUVirtualAddress();
```

And we’re done. No shader changes are required. Launch the application and you should see the 2 outer
triangles rotate.


