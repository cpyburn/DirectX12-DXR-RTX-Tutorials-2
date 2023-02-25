DirectX Raytracing Tutorials
============
This repository contain tutorials demonstrating how to use DirectX Raytracing.

Requirements:
- A GPU that supports DXR (Such as NVIDIA's Volta or Turing hardware)
- Windows 10 RS5 (version 1809) or newer
- [Windows 10 SDK version 1809 (10.0.17763.0)](https://developer.microsoft.com/en-us/windows/downloads/sdk-archive)
- Visual Studio 2022

# DXR Tutorial 12

## 12.0 Per-Geometry Hit-Shader

## Overview
In the previous tutorial we added a new geometry – a plane. The result wasn’t that impressive – the
plane used the same hit-shader and the same vertex colors as for the triangle, resulting in colorful
image.

In this tutorial, we will implement a new hit-shader specific to the plane and show how to invoke it
when the plane is hit by a ray.

## 12.1 Plane Hit-Program
For the plane, we will create a simple hit-program which returns a constant color. The following code
can be found in ’12-Shaders.hlsl’
```c++
// 12.1.a
[shader("closesthit")]
void planeChs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    payload.color = 0.9f;
}
```

We need to make the following changes to createRtPipelineState():
  * 13 subojbects
  ```c++
  // 12.1.b
  std::array<D3D12_STATE_SUBOBJECT, 13> subobjects;
  ```
  * Create a new HitProgram for the plane CHS.
  ```c++
  // 12.1.c  Create the plane HitProgram
  HitProgram planeHitProgram(nullptr, kPlaneChs, kPlaneHitGroup);
  subobjects[index++] = planeHitProgram.subObject; // 2 Plane Hit Group
  ```
  * Associate the empty-root signature with the new plane hit-group
  ```c++
  // 12.1.d
  uint32_t emptyRootIndex = index++; // 7
  const WCHAR* emptyRootExport[] = { kPlaneChs, kMissShader };
  ExportAssociation emptyRootAssociation(emptyRootExport, arraysize(emptyRootExport), &(subobjects[emptyRootIndex]));
  subobjects[index++] = emptyRootAssociation.subobject; // 8 Associate Miss Root Sig to Miss Shader
  ```
  * Associate the shader-config sub-object with the plane hit-group
  ```c++
  const WCHAR* shaderExports[] = { kMissShader, kClosestHitShader, kPlaneChs /*12.1.e*/, kRayGenShader};
  ```
Create the string representations (line 582~)
```c++
// 12.1.f
static const WCHAR* kPlaneChs = L"planeChs";
static const WCHAR* kPlaneHitGroup = L"PlaneHitGroup";
```

## 12.2 Shader-Table Layout
We would like the ray-tracing pipeline to invoke the new hit-program when the plane is hit. In tutorial
10 we learned that the hit-program indexing is computed as follows:

  (HitStartAddress +
  InstanceContributionToHitGroupIndex +
  GeometryIndex * MultiplierForGeometryContributionToShaderIndex +
  RayContributionToHitGroupIndex)

To understand how it can be used to invoke a different hit-program, let’s look again at our TLAS.

We have 3 instances:

  - Instance 0 with 2 geometries.
    * Geometry 0 – triangle
    * Geometry 1 – plane
  - Instance 1 – single geometry, a triangle
  - Instance 2 – single geometry, a triangle
  
Geometries in the same instance share the same InstanceContributionToHitGroupIndex. To direct
different geometries to different shader-table records, we need to use GeometryIndex.

Our new shader-table will look like this:

![image](https://user-images.githubusercontent.com/17934438/221359382-c4667656-0e00-4986-ac49-3855373507ab.png)

Let’s see how this layout works with the hit-program index computation:

    - BaseIndex is 2. It’s shared between all instances and geometries.
    - InstanceContributionToHitGroupIndex is per instance, specified when building the TLAS.
        * For instance 0 it will be 0.
        * For instance 1 it will be 2 (we need to skip both geometries in instance 0).
        * For instance 2 it will be 3.
    - GeometryIndex is generated automatically by the pipeline. This is the index of the geometry
    within an instance.
        * This value will be 0 for all the triangles, since they are the first geometry in the instance.
        * It will be 1 for the plane, since it’s the second geometry in the first instance.
    - MultiplierForGeometryContributionToShaderIndex should be 1.
        * This value doesn’t affect the triangles (their GeometryIndex is 0).
        * For the plane, (GeometryIndex *
    MultiplierForGeometryContributionToShaderIndex) will result in 1, which is the
    required offset of the record relative to the start of the instance.
    - RayContributionToHitGroupIndex should be 0.
    
You can plug these values into the formula above to see the final value for each geometry.

## 12.3 Shader-Table Changes
Now that we understand the new layout and the indexing, we can make the required code changes.

First, we need to create a larger shader-table. We need 6 entries in total. This happens at the beginning
of createShaderTable().

Next, we need to initialize the shader-table hit-program records. The first entry is for the triangle in
instance 0:
```c++
// Entry 2 - Triangle 0 hit program. ProgramID and constant-buffer data
uint8_t* pEntry2 = pData + mShaderTableEntrySize * 2;
memcpy(pEntry2, pRtsoProps-&gt;GetShaderIdentifier(kTriHitGroup), progIdSize);
*(D3D12_GPU_VIRTUAL_ADDRESS*)(pEntry2 + progIdSize) = mpConstantBuffer[0]-&gt;GetGPUVirtualAddress();
```

This code is similar to the code from the previous tutorials.

Now let’s initialize the entry for the plane. We have no shader resources, so we only need to set the
program identifier of the plane hit-program.
```c++
// Entry 3 - Plane hit program. ProgramID only
uint8_t* pEntry3 = pData + mShaderTableEntrySize * 3;
memcpy(pEntry3, pRtsoProps-&gt;GetShaderIdentifier(kPlaneHitGroup), progIdSize);
```

Entries 4 and 5 are for the 2 other triangles. The code is very similar to the code we used for the first
triangle.
```c++
// 12.3.a
void Tutorial01::createShaderTable()
{
    /** The shader-table layout is as follows:
        Entry 0 - Ray-gen program
        Entry 1 - Miss program
        Entry 2 - Hit program for triangle 0
        Entry 3 - Hit program for the plane
        Entry 4 - Hit program for triangle 1
        Entry 5 - Hit program for triangle 2
        All entries in the shader-table must have the same size, so we will choose it base on the largest required entry.
        The triangle hit program requires the largest entry - sizeof(program identifier) + 8 bytes for the constant-buffer root descriptor.
        The entry size must be aligned up to D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT
    */

    // Calculate the size and create the buffer
    mShaderTableEntrySize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    mShaderTableEntrySize += 8; // The hit shader constant-buffer descriptor
    mShaderTableEntrySize = align_to(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, mShaderTableEntrySize);
    uint32_t shaderTableSize = mShaderTableEntrySize * 6;

    // For simplicity, we create the shader-table on the upload heap. You can also create it on the default heap
    mpShaderTable = createBuffer(mpDevice, shaderTableSize, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);

    // Map the buffer
    uint8_t* pData;
    d3d_call(mpShaderTable->Map(0, nullptr, (void**)&pData));

    MAKE_SMART_COM_PTR(ID3D12StateObjectProperties);
    ID3D12StateObjectPropertiesPtr pRtsoProps;
    mpPipelineState->QueryInterface(IID_PPV_ARGS(&pRtsoProps));

    // Entry 0 - ray-gen program ID and descriptor data
    memcpy(pData, pRtsoProps->GetShaderIdentifier(kRayGenShader), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    uint64_t heapStart = mpSrvUavHeap->GetGPUDescriptorHandleForHeapStart().ptr;
    *(uint64_t*)(pData + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) = heapStart;

    // Entry 1 - miss program
    memcpy(pData + mShaderTableEntrySize, pRtsoProps->GetShaderIdentifier(kMissShader), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

    // Entry 2 - Triangle 0 hit program. ProgramID and constant-buffer data
    uint8_t* pEntry2 = pData + mShaderTableEntrySize * 2;
    memcpy(pEntry2, pRtsoProps->GetShaderIdentifier(kHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    assert(((uint64_t)(pEntry2 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) % 8) == 0); // Root descriptor must be stored at an 8-byte aligned address
    *(D3D12_GPU_VIRTUAL_ADDRESS*)(pEntry2 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) = mpConstantBuffer[0]->GetGPUVirtualAddress();

    // Entry 3 - Plane hit program. ProgramID only
    uint8_t* pEntry3 = pData + mShaderTableEntrySize * 3;
    memcpy(pEntry3, pRtsoProps->GetShaderIdentifier(kPlaneHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

    // Entry 4 - Triangle 1 hit. ProgramID and constant-buffer data
    uint8_t* pEntry4 = pData + mShaderTableEntrySize * 4;
    memcpy(pEntry4, pRtsoProps->GetShaderIdentifier(kHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    assert(((uint64_t)(pEntry4 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) % 8) == 0); // Root descriptor must be stored at an 8-byte aligned address
    *(D3D12_GPU_VIRTUAL_ADDRESS*)(pEntry4 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) = mpConstantBuffer[1]->GetGPUVirtualAddress();

    // Entry 5 - Triangle 2 hit. ProgramID and constant-buffer data
    uint8_t* pEntry5 = pData + mShaderTableEntrySize * 5;
    memcpy(pEntry5, pRtsoProps->GetShaderIdentifier(kHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    assert(((uint64_t)(pEntry5 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) % 8) == 0); // Root descriptor must be stored at an 8-byte aligned address
    *(D3D12_GPU_VIRTUAL_ADDRESS*)(pEntry5 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) = mpConstantBuffer[2]->GetGPUVirtualAddress();

    // Unmap
    mpShaderTable->Unmap(0, nullptr);
}
```

Four final changes:
    - We need to change the InstanceContributionToHitGroupIndex for the second and third
    instances. This happens during TLAS creation.
    ```c++
    // 12.3.b
    pInstanceDesc[i].InstanceContributionToHitGroupIndex = i + 1;  // The plane takes an additional entry in the shader-table, hence the +1
    ```
    - Hit the ray-generation shader (12-Shaders.hlsl), we need to change the TraceRay() call. We
    need to pass `1` as the MultiplierForGeometryContributionToShaderIndex argument.
    ```c++
    TraceRay(gRtScene, 0 /*rayFlags*/, 0xFF, 0 /* ray index*/, 1 /* 12.3.c MultiplierForGeometryContributionToShaderIndex */, 0, ray, payload);
    ```
    - In onFrameRender(), set raytraceDesc.HitGroupTable.SizeInBytes to mShaderTableEntrySize * 4.
    ```c++
    // 12.3.d
    raytraceDesc.HitGroupTable.SizeInBytes = mShaderTableEntrySize * 4;
    ```
    - update createDxilLibrary()
    ```c++
    const WCHAR* entryPoints[] = { kRayGenShader, kMissShader, kPlaneChs /* 12.3.e */, kClosestHitShader};
    ```

And that should do it!
