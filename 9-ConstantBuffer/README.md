DirectX Raytracing Tutorials
============
This repository contain tutorials demonstrating how to use DirectX Raytracing.

Requirements:
- A GPU that supports DXR (Such as NVIDIA's Volta or Turing hardware)
- Windows 10 RS5 (version 1809) or newer
- [Windows 10 SDK version 1809 (10.0.17763.0)](https://developer.microsoft.com/en-us/windows/downloads/sdk-archive)
- Visual Studio 2022

# DXR Tutorial 09
## Constant Buffers

## 9.0 Overview
In the previous tutorial we computed the hit-point colors based on constants defined in the shader. In
this tutorial we will learn how to use constant-buffers with DXRT and use one to get the vertex colors
from.

We already learned everything we need to know for working with constant-buffers. This tutorial is more
of an exercise - feel free to try adding constant-buffer support all by yourself.

Let’s dive directly into the code.
```c++
// 9.0 
void createConstantBuffer();
ID3D12ResourcePtr mpConstantBuffer;
```

## 9.1 Closest-Hit Shader
Let’s start with modifying the shader. The changes are straightforward – we start by adding a constant-
buffer definition.
```c++
// 9.1.a
cbuffer PerFrame : register(b0)
{
    float3 A[3];
    float3 B[3];
    float3 C[3];
}
```

As you can see, we have 3 sets of vertex colors, one per triangle. We then use the instanceID to fetch
the colors from the buffer and compute the result.
```c++
// 9.1.b
[shader("closesthit")]
void chs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    uint instanceID = InstanceID();
    float3 barycentrics = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);
    payload.color = A[instanceID] * barycentrics.x + B[instanceID] * barycentrics.y + C[instanceID] * barycentrics.z;
}
```

## 9.2 Modifying the RTPSO Creation
Up until now, we created the hit-program with an empty root-signature. Now that the closest-hit shader
requires a constant-buffer, we need a different root-signature.

We will create a root-signature with a single entry – a root-descriptor for a Constant-Buffer View (CBV).
If you’ll take a look at createRtPipelineState(), you’ll see that we added 2 new sub-objects:
* A LocalRootSignature for the hit-program created by calling createHitRootDesc().
* An ExportAssociation that associates the hit-program local root-signature to the hit-group.
```c++
//  9.2.a 2 for hit-program root-signature (root-signature and the subobject association)
//  9.2.b 2 for miss-shader root-signature (signature and association)
```
```c++
// 9.2.c create method for HitRootDesc
RootSignatureDesc createHitRootDesc()
{
    RootSignatureDesc desc;
    desc.rootParams.resize(1);
    desc.rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    desc.rootParams[0].Descriptor.RegisterSpace = 0;
    desc.rootParams[0].Descriptor.ShaderRegister = 0;

    desc.desc.NumParameters = 1;
    desc.desc.pParameters = desc.rootParams.data();
    desc.desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

    return desc;
}
```
Create the hit group root sig and associate it
```c++
// 9.2.d Create the hit root-signature and association
LocalRootSignature hitRootSignature(mpDevice, createHitRootDesc().desc);
subobjects[index] = hitRootSignature.subobject; // 4 Hit Root Sig

uint32_t hitRootIndex = index++; // 4
ExportAssociation hitRootAssociation(&kClosestHitShader, 1, &(subobjects[hitRootIndex]));
subobjects[index++] = hitRootAssociation.subobject; // 5 Associate Hit Root Sig to Hit Group
```

We also removed the hit-group name from the empty root-signature association.
```c++
// 9.2.e Create the miss root-signature and association
D3D12_ROOT_SIGNATURE_DESC emptyDesc = {};
emptyDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
LocalRootSignature missRootSignature(mpDevice, emptyDesc);
subobjects[index] = missRootSignature.subobject; // 6 Miss Root Sig

uint32_t missRootIndex = index++; // 6
ExportAssociation missRootAssociation(&kMissShader, 1, &(subobjects[missRootIndex]));
subobjects[index++] = missRootAssociation.subobject; // 7 Associate Miss Root Sig to Miss Shader
```
Don't forget to make the array a size of 12
```c++
// 9.2.f
std::array<D3D12_STATE_SUBOBJECT, 12> subobjects;
```

## 9.3 Creating the Constant-Buffer
Creating the constant-buffer is done the same way as for rasterization. This happens in
createConstantBuffer()
```c++
// 9.3 create constant buffer
void Tutorial01::createConstantBuffer()
{
    // The shader declares the CB with 9 float3. However, due to HLSL packing rules, we create the CB with 9 float4 (each float3 needs to start on a 16-byte boundary)
    vec4 bufferData[] =
    {
        // A
        vec4(1.0f, 0.0f, 0.0f, 1.0f),
        vec4(0.0f, 1.0f, 0.0f, 1.0f),
        vec4(0.0f, 0.0f, 1.0f, 1.0f),

        // B
        vec4(1.0f, 1.0f, 0.0f, 1.0f),
        vec4(0.0f, 1.0f, 1.0f, 1.0f),
        vec4(1.0f, 0.0f, 1.0f, 1.0f),

        // C
        vec4(1.0f, 0.0f, 1.0f, 1.0f),
        vec4(1.0f, 1.0f, 0.0f, 1.0f),
        vec4(0.0f, 1.0f, 1.0f, 1.0f),
    };

    mpConstantBuffer = createBuffer(mpDevice, sizeof(bufferData), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
    uint8_t* pData;
    d3d_call(mpConstantBuffer->Map(0, nullptr, (void**)&pData));
    memcpy(pData, bufferData, sizeof(bufferData));
    mpConstantBuffer->Unmap(0, nullptr);
}
```
If you look at the onLoad() function, you’ll notice that we create the constant-buffer before we create the shader-table. That’s because we need the constant-buffer GPU address in hand when initializing the
shader-table.
## 9.4 onLoad
```c++
createConstantBuffer(); // Tutorial 09. Yes, we need to do it before creating the shader-table
```

## 9.5 The Shader Table
There are potentially 2 modifications we need to make in createShaderTable(). The first one is obvious
* We need to set the CBV into the root-table. 
* The other is subtler – we need to modify the shader-table record size.

Remember that all shader-table records share the same size. The size we chose was based on the largest
required root-table size. Up until now, the ray-generation shader required the largest table. Its root-
signature had a single descriptor-table, which is 8 bytes.

Now the closest-hit shader uses a root-descriptor, but luckily for us a root-descriptor is 8 bytes, so our
shader-table record size can stay the same.

Finally, we need to set the constant-buffer address into the root-table
```c++
// 9.5 The Shader Table
// Entry 2 - hit program. Program ID and one constant-buffer as root descriptor    
uint8_t* pHitEntry = pData + mShaderTableEntrySize * 2; // +2 skips the ray-gen and miss entries
memcpy(pHitEntry, pRtsoProps->GetShaderIdentifier(kHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
uint8_t* pCbDesc = pHitEntry + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;  // Adding `progIdSize` gets us to the location of the constant-buffer entry
assert(((uint64_t)pCbDesc % 8) == 0); // Root descriptor must be stored at an 8-byte aligned address
*(D3D12_GPU_VIRTUAL_ADDRESS*)pCbDesc = mpConstantBuffer->GetGPUVirtualAddress();
```

The first line skips the ray-gen and miss program entries. We then skip the program identifier to get the
address of the root-descriptor. We then set the constant-buffer GPU virtual address.

The spec requires root-descriptors to be aligned on an 8-byte address. The assertion in the code makes
sure that this is the case.

And we’re done!
![image](https://user-images.githubusercontent.com/17934438/221318621-82e15186-8c2c-41ff-843d-3f68235d8715.png)


