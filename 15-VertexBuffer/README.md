DirectX Raytracing Tutorials
============
This repository contain tutorials demonstrating how to use DirectX Raytracing.

Requirements:
- A GPU that supports DXR (Such as NVIDIA's Volta or Turing hardware)
- Windows 10 RS5 (version 1809) or newer
- [Windows 10 SDK version 1809 (10.0.17763.0)](https://developer.microsoft.com/en-us/windows/downloads/sdk-archive)
- Visual Studio 2022

# DXR Tutorial 15

## 15.0 Vertex Buffer

## Overview
There will be times that you need the Vertex and Indice information in the shaders for DXR and RTX for things like vertex positions, texture uvs, normals, binormals, tangents, and even colors.  We do this with a shader Structured Buffer.

## Structured Buffer
If you have been following along then you may already have some ideas about how to do this.  Should be easy right?
1.  Create the SRV
2.  Create the Root Signature
3.  Bind the srv in the Shader Binding Table
4.  Create the structured buffer in the shader file
5.  Make sure the structured buffer and vertex buffers match

## 15.1 createShaderResources()
Increase the size of the buffer
```c++
// 15.1.a Create an SRV/UAV descriptor heap. Need 3 entries - 1 SRV for the scene and 1 UAV for the output and 1 for the vertex information
mpSrvUavHeap = createDescriptorHeap(mpDevice, 3, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
```
Create the SRV
```c++
// 15.1.b
srvDesc = {};
srvDesc.ViewDimension = D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_BUFFER;
srvDesc.Format = DXGI_FORMAT::DXGI_FORMAT_UNKNOWN;
srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
srvDesc.Buffer.StructureByteStride = sizeof(TriVertex); // your vertex struct size goes here
srvDesc.Buffer.NumElements = 3; // number of vertices go here
srvHandle.ptr += mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
mpDevice->CreateShaderResourceView(mpVertexBuffer[0], &srvDesc, srvHandle);
mpVertexBuffer[0]->SetName(L"SRV VB");
```

## 15.2 createHitRootDesc()
Add the SRV to the root signature. We don't have to alter the createRtPipelineState() because the rootsignature is already associated.
```c++
// 15.2
RootSignatureDesc createHitRootDesc()
{
    RootSignatureDesc desc;

    desc.rootParams.resize(2); // cbv + srv
    // CBV
    desc.rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    desc.rootParams[0].Descriptor.RegisterSpace = 0;
    desc.rootParams[0].Descriptor.ShaderRegister = 0;

    // SRV
    desc.range.resize(1); // srv
    desc.range[0].BaseShaderRegister = 1; // gOutput used the first t() register in the shader
    desc.range[0].NumDescriptors = 1;
    desc.range[0].RegisterSpace = 0;
    desc.range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    desc.range[0].OffsetInDescriptorsFromTableStart = 0;
    // SRV
    desc.rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    desc.rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    desc.rootParams[1].DescriptorTable.pDescriptorRanges = desc.range.data();

    desc.desc.NumParameters = 2; // cbv + srv
    desc.desc.pParameters = desc.rootParams.data();
    desc.desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

    return desc;
}
```

## 15.3 
Bind the srv in the Shader Binding Table
```c++
// 15.3.a
*(uint64_t*)(pEntry3 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + sizeof(D3D12_GPU_VIRTUAL_ADDRESS)) = heapStart + mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 2; // The SRV comes 2 after the program id
```
```c++
// 15.3.b
*(uint64_t*)(pEntry7 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + sizeof(D3D12_GPU_VIRTUAL_ADDRESS)) = heapStart + mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 2; // The SRV comes 2 after the program id
```

```c++
// 15.3.c
*(uint64_t*)(pEntry9 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + sizeof(D3D12_GPU_VIRTUAL_ADDRESS)) = heapStart + mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 2; // The SRV comes 2 after the program id
```

## 15.4 04-Shaders.hlsl
Add the code for the structured buffer
```c++
// 15.4.a
struct STriVertex
{
    float3 vertex;
    float4 color;
};
StructuredBuffer<STriVertex> BTriVertex : register(t1);
```
Update the CHS so that we can test that it works
```c++
// 15.4.b
uint instance = InstanceID();
float3 hitColor = BTriVertex[instance].color * barycentrics.x + BTriVertex[instance].color * barycentrics.y + BTriVertex[instance].color * barycentrics.z;
payload.color = hitColor;
```

## 15.5 Vertex Struct
Create a struct that matches the Structured buffer
```c++
// 15.5.a
struct TriVertex
{
    vec3 vertex;
    vec4 color;
};
```
Update the createTriangleVB
```c++
// 15.5.b
const TriVertex vertices[] =
{
    vec3(0,          1,  0), vec4(1, 0, 0, 1),
    vec3(0.866f,  -0.5f, 0), vec4(0, 1, 0, 1),
    vec3(-0.866f, -0.5f, 0), vec4(0, 0, 1, 1),
};
```

## 15.6 createBottomLevelAS
Since the size of the vertex buffer has changed, we need to update the BLAS
```c++
geomDesc[i].Triangles.VertexBuffer.StrideInBytes = sizeof(TriVertex); // 15.6
```
## 15.7
And since the BLAS's stride is now the sizeof(TriVertex) we have to make sure createPlaneVB either matches or create a different BLAS for it.  To keep things simple, we will do the easist option and update it to use TriVertex
```c++
// 15.7
const TriVertex vertices[] =
{
    vec3(-100, -1,  -2), vec4(0, 0, 0, 1),
    vec3(100, -1,  100), vec4(0, 0, 0, 1),
    vec3(-100, -1,  100), vec4(0, 0, 0, 1),

    vec3(-100, -1,  -2), vec4(0, 0, 0, 1),
    vec3(100, -1,  -2), vec4(0, 0, 0, 1),
    vec3(100, -1,  100), vec4(0, 0, 0, 1),
};
```
![image](https://user-images.githubusercontent.com/17934438/221937776-264c6de5-0577-4236-8d35-1e8c1833dcef.png)

