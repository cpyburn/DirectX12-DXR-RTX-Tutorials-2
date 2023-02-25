DirectX Raytracing Tutorials
============
This repository contain tutorials demonstrating how to use DirectX Raytracing.

Requirements:
- A GPU that supports DXR (Such as NVIDIA's Volta or Turing hardware)
- Windows 10 RS5 (version 1809) or newer
- [Windows 10 SDK version 1809 (10.0.17763.0)](https://developer.microsoft.com/en-us/windows/downloads/sdk-archive)
- Visual Studio 2022

# DXR Tutorial 10

## Per-Instance Constant Buffer

## 10.0 Overview
In the previous tutorial we learned how to bind a constant-buffer to the ray-tracing pipeline. The same
constant-buffer was shared between the instances.

DXR provides a mechanism to bind different resources to different instances of the same geometry. As
you recall, we bind the resources into a root-table that is part of the shader-table entry. By creating an
shader-table entry per instance and understanding how shader-table indexing works, we can use
different resource for each instance.

Before we get to it, let’s get the simple things out of the way.

## 10.1 Closest-Hit Shader
Since we are using a different resource for each instance, we do not need to use InstanceID()
anymore. We also change the constant-buffer to store a single set of vertex colors.
The code can be found in ’10-Shaders.hlsl’:

```c++
// 10.1.a
cbuffer PerFrame : register(b0)
{
    float3 A;
    float3 B;
    float3 C;
}

```
```c++
// 10.1.b
[shader("closesthit")]
void chs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    float3 barycentrics = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);
    payload.color = A * barycentrics.x + B * barycentrics.y + C * barycentrics.z;
}
```

There is no need to change the hit-program creation. The root-signature stays the same – a single root-
descriptor for the CBV.

## 10.2 Constant-Buffers
We now need to create 3 constant-buffer – one per instance. You can see it in createConstantBuffers().
```c++
// 10.2.a 01-CreateWindow.h
ID3D12ResourcePtr mpConstantBuffer[3];
```

```c++
// 10.
for(uint32_t i = 0 ; i < 3 ; i++)
{
    const uint32_t bufferSize = sizeof(vec4) * 3;
    mpConstantBuffer[i] = createBuffer(mpDevice, bufferSize, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
    uint8_t* pData;
    d3d_call(mpConstantBuffer[i]->Map(0, nullptr, (void**)&pData));
    memcpy(pData, &bufferData[i * 3], sizeof(bufferData));
    mpConstantBuffer[i]->Unmap(0, nullptr);
}
```

## 10.3 Shader-Table Indexing
To understand the changes required to support per-instance resources, we need to understand how the
ray-tracing pipeline decides which shader-table record to use when executing programs. The indexing
computation is different depending on the type of the program but they share several common
elements specified.
* D3D12_DISPATCH_RAYS_DESC contains StartAddress and StrideInBytes fields per shader
type.
* RayContributionToHitGroupIndex – One of the parameters of the HLSL’s TraceRay()
function. The maximum allowed value is 15.
* MultiplierForGeometryContributionToShaderIndex – One of the parameters of the HLSL’s
TraceRay() function. The maximum allowed value is 15.
* MissShaderIndex – One of the parameters of the HLSL’s TraceRay() function.
## Ray-Generation Program
This is an easy one. It’s the entry pointed by the StartAddress.
## Miss Program
The entry is (MissStartAddress + MissShaderIndex * MissStrideInBytes).
## Hit Program
The entry index is:

And the entry address is (HitStartAddress + entryIndex * HitStrideInBytes)

There are 2 new elements here:
InstanceContributionToHitGroupIndex– This value is specified when creating the TLAS, as part of
D3D12_RAYTRACING_INSTANCE_DESC.
GeometryIndex – As you might recall, when creating a bottom-level acceleration structure we can
specify multiple geometries by passing multiple D3D12_RAYTRACING_GEOMETRY_DESC. The GeometryIndex
is the index of the geometry inside the bottom-level acceleration structure. In our case, we have a single
geometry so this value is always 0.
This indexing scheme allows some flexibility in the way the shader-table records are laid out. See the
DXR specification for examples.
In our case, we will go with the following simple layout:

This only requires us to change the InstanceContributionToHitGroupIndex field of
D3D12_RAYTRACING_INSTANCE_DESC structure when creating the TLAS.

entryIndex =
InstanceContributionToHitGroupIndex +
GeometryIndex * MultiplierForGeometryContributionToShaderIndex +
RayContributionToHitGroupIndex)

RayGen Miss Hit
Instance 0
Hit
Instance 1
Hit
Instance 2

We have no real use for either MultiplierForGeometryContributionToShaderIndex or
RayContributionToHitGroupIndex. We will set both to zero.
You can see the change in line 375 (createTopLevelAS()). `i` is the instance index, in the range [0,3).

Shader Table
The first thing we need to change in the shader-table size. We now need 5 entries. This affect the
shader-table size we calculate (line 604):

Finally, we need to initialize the 3 hit-program entries (lines 797-804):

This code is very similar to the code from tutorial 9. We calculate the address of the hit entry, then set
the program identifier and the constant-buffer address of the current instance.

The last part of the code we need to change is in onFrameRender(). The Hit-Group table contains 3
entries, so we set raytraceDesc.HitGroupTable.SizeInBytes to mShaderTableEntrySize * 3.

In our case, those changes result in the same image as in tutorial 9. However, now that we understand
shader-table indexing we can start covering more advanced usages. We’ll get to that in the next tutorial.



