DirectX Raytracing Tutorials
============
This repository contain tutorials demonstrating how to use DirectX Raytracing.

Requirements:
- A GPU that supports DXR (Such as NVIDIA's Volta or Turing hardware)
- Windows 10 RS5 (version 1809) or newer
- [Windows 10 SDK version 1809 (10.0.17763.0)](https://developer.microsoft.com/en-us/windows/downloads/sdk-archive)
- Visual Studio 2022

# DXR Tutorial 17

## 7.0 Index Buffer

## Overview
There will be times that you need Indice information from a model

## Structured Buffer
If you have been following along then you may already have some ideas about how to do this.  Should be easy right? We just did it for the vertex buffer
1.  Create the SRV
2.  Create the Root Signature
3.  Bind the srv in the Shader Binding Table
4.  Create the structured buffer in the shader file
5.  Make sure the structured buffer and index buffers match

## 17.1 createShaderResources()
Increase the size of the buffer
```c++
// 17.1.a Create an SRV/UAV descriptor heap. Need 4 entries - 1 SRV for the scene and 1 UAV for the output, 1 for the vertex information, and now one for the Index buffer
mpSrvUavHeap = createDescriptorHeap(mpDevice, 4, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
```
Create buffer and the view the SRV
```c++
// 17.1.b mpVertexBuffer[0] is triangle, mpVertexBuffer[1] is plane, for this excercise we are only doing indices for the triangle
const int indices[] =
{
    0, 1, 2
};

// For simplicity, we create the vertex buffer on the upload heap, but that's not required
mpIndexBuffer = createBuffer(mpDevice, sizeof(indices), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
uint8_t* pData;
mpIndexBuffer->Map(0, nullptr, (void**)&pData);
memcpy(pData, indices, sizeof(indices));
mpIndexBuffer->Unmap(0, nullptr);

srvDesc = {};
srvDesc.ViewDimension = D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_BUFFER;
srvDesc.Format = DXGI_FORMAT::DXGI_FORMAT_UNKNOWN;
srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
srvDesc.Buffer.StructureByteStride = sizeof(int); // your index struct size goes here
srvDesc.Buffer.NumElements = 3; // number of indices go here
srvHandle.ptr += mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
mpDevice->CreateShaderResourceView(mpIndexBuffer, &srvDesc, srvHandle);
mpIndexBuffer->SetName(L"SRV IB");
```
Add the buffer members to the .h file
```c++
// 17.1.c
ID3D12ResourcePtr mpIndexBuffer;
```

## 17.2 createHitRootDesc()
Add the SRV to the root signature. We don't have to alter the createRtPipelineState() because the rootsignature is already associated.
```c++
// 17.2
RootSignatureDesc createHitRootDesc()
{
    RootSignatureDesc desc;

    desc.rootParams.resize(3); // cbv + vertex srv + index srv
    // CBV
    desc.rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    desc.rootParams[0].Descriptor.RegisterSpace = 0;
    desc.rootParams[0].Descriptor.ShaderRegister = 0;

    desc.range.resize(2); // vertex srv + index srv

    // vertex SRV
    desc.range[0].BaseShaderRegister = 1; // gOutput used the first t() register in the shader
    desc.range[0].NumDescriptors = 1;
    desc.range[0].RegisterSpace = 0;
    desc.range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    desc.range[0].OffsetInDescriptorsFromTableStart = 0;
    // vertex SRV
    desc.rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    desc.rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    desc.rootParams[1].DescriptorTable.pDescriptorRanges = &desc.range[0];

    // index SRV
    desc.range[1].BaseShaderRegister = 2; // gOutput used the first t() register in the shader
    desc.range[1].NumDescriptors = 1;
    desc.range[1].RegisterSpace = 0;
    desc.range[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    desc.range[1].OffsetInDescriptorsFromTableStart = 0;
    // index SRV
    desc.rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    desc.rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
    desc.rootParams[2].DescriptorTable.pDescriptorRanges = &desc.range[1];

    desc.desc.NumParameters = 3; // cbv + vertex srv + index srv
    desc.desc.pParameters = desc.rootParams.data();
    desc.desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

    return desc;
}
```

## 17.3 
Bind the srv in the Shader Binding Table
```c++
// 17.3.a
*(uint64_t*)(pEntry3 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + sizeof(D3D12_GPU_VIRTUAL_ADDRESS) * 2) = heapStart + mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 3; //  index SRV comes 3 after the program id
```
```c++
// 17.3.b
*(uint64_t*)(pEntry7 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + sizeof(D3D12_GPU_VIRTUAL_ADDRESS) * 2) = heapStart + mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 3; // The index SRV comes 3 after the program id
```

```c++
// 17.3.c
*(uint64_t*)(pEntry9 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + sizeof(D3D12_GPU_VIRTUAL_ADDRESS) * 2) = heapStart + mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 3; // The index SRV comes 3 after the program id
```

## 17.4 04-Shaders.hlsl
Add the code for the structured buffer
```c++
// 17.4.a
StructuredBuffer<uint> indices: register(t2);
```
Update the CHS so that we can test that it works
```c++
// 17.4.b
// Retrieve corresponding vertex normals for the triangle vertices.
float3 vertexNormals[3] = {
    BTriVertex[instance + indices[0]].normal,
    BTriVertex[instance + indices[1]].normal,
    BTriVertex[instance + indices[2]].normal,
};
```

![image](https://user-images.githubusercontent.com/17934438/222509414-c22fc5bd-a7cc-48d5-adc1-ec018cdda216.png)

