DirectX Raytracing Tutorials
============
This repository contain tutorials demonstrating how to use DirectX Raytracing.

Requirements:
- A GPU that supports DXR (Such as NVIDIA's Volta or Turing hardware)
- Windows 10 RS5 (version 1809) or newer
- [Windows 10 SDK version 1809 (10.0.17763.0)](https://developer.microsoft.com/en-us/windows/downloads/sdk-archive)
- Visual Studio 2022

# DXR Tutorial 11

## 11.0 Adding a Second Geometry

## Overview
So far, we used a single mesh, which had a single triangle it. We used instancing to render 3 triangles to
the screen. However, in real-world scenarios scenes are comprised of thousands of meshes. In this
tutorial we learn how to support multiple geometries by way of adding a plane to our scene.

## Acceleration Structures Revisited
Time to talk a little bit about the acceleration structure hierarchy. During the previous tutorials we
mentioned the top-level and bottom-level acceleration structures a lot. We also mentioned briefly that
there’s something called geometry. Let’s fully understand what each means conceptually. We will only
discuss triangles. We will not cover axis-aligned bounding-box geometries, but know that they exist.

## Geometry
A geometry describes a single mesh. It must have exactly 1 vertex buffer which contains the positions (it
can also contain other vertex-attributes). An index-buffer is optional. The only supported topology is
triangle-list. We can optionally provide a transformation matrix which will be applied to the positions at
build time.

A geometry is described using the D3D12_RAYTRACING_GEOMETRY_DESC struct.

## Bottom-Level Acceleration Structure
A BLAS is a collection of geometries. If a geometry describes a mesh, we can think of the BLAS as a
model (made up of multiple meshes). The BLAS defines a local-space for the geometries it contains.

## Top-Level Acceleration Structure
If a geometry describes a mesh and a BLAS defines a model, then the TLAS is a scene. It is a collection of
BLAS instances. Each instance is described by a BLAS and a transformation matrix.

## Our Goal
At the end of this tutorial we will use all the concept above. We will have 2 bottom-level acceleration
structures:
  1. Single geometry, containing our trusty triangle.
  2. 2 geometries – a triangle and a plane.
We will only make changes to the acceleration structure code. We do not need to change the shader-
table.

## 11.1 Creating the Plane
First increase the vertex buffer and BLAS to 2 in 01-CreateWindow.h
```c++
// 11.1.a
ID3D12ResourcePtr mpVertexBuffer[2];
```
```c++
// 11.1.b
ID3D12ResourcePtr mpBottomLevelAS[2];
```

We need to create a vertex buffer for the plane. This is standard DX12 code – see createPlaneVB().
```c++
// 11.1.c
ID3D12ResourcePtr createPlaneVB(ID3D12Device5Ptr pDevice)
{
    const vec3 vertices[] =
    {
        vec3(-100, -1,  -2),
        vec3(100, -1,  100),
        vec3(-100, -1,  100),

        vec3(-100, -1,  -2),
        vec3(100, -1,  -2),
        vec3(100, -1,  100),
    };

    // For simplicity, we create the vertex buffer on the upload heap, but that's not required
    ID3D12ResourcePtr pBuffer = createBuffer(pDevice, sizeof(vertices), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
    uint8_t* pData;
    pBuffer->Map(0, nullptr, (void**)&pData);
    memcpy(pData, vertices, sizeof(vertices));
    pBuffer->Unmap(0, nullptr);
    return pBuffer;
}
```

We will revist the createAccelerationStructures() and update the buffer and BLAS later in this tutorial but go ahead and update the createTriangleVB
```c++
// 11.1.d
mpVertexBuffer[0] = createTriangleVB(mpDevice);
```

## 11.2 Bottom-Level Acceleration Structures
We need 2 bottom-level acceleration structures. The code for both is very similar, so we will be using
the same function.
```c++
//11.2.a bottom-level acceleration structure
AccelerationStructureBuffers createBottomLevelAS(ID3D12Device5Ptr pDevice, ID3D12GraphicsCommandList4Ptr pCmdList, ID3D12ResourcePtr pVB[], const uint32_t vertexCount[], uint32_t geometryCount)
```

The function now accepts an array of vertex buffers. We call it once with both the plane and the
triangle, and once just with the triangle (the calls are in createAccelerationStructures()).

The first thing we do is initialize an array of D3D12_RAYTRACING_GEOMETRY_DESC.
```c++
// 11.2.b
std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geomDesc;
geomDesc.resize(geometryCount);

for (uint32_t i = 0; i < geometryCount; i++)
{
    geomDesc[i].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geomDesc[i].Triangles.VertexBuffer.StartAddress = pVB[i]->GetGPUVirtualAddress();
    geomDesc[i].Triangles.VertexBuffer.StrideInBytes = sizeof(vec3);
    geomDesc[i].Triangles.VertexCount = vertexCount[i];
    geomDesc[i].Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geomDesc[i].Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
}
```
```c++
// 11.2.c
inputs.NumDescs = geometryCount;
inputs.pGeometryDescs = geomDesc.data();
```
Update the createAccelerationStructures() to accept the new BLAS AccelerationStructureBuffers
```c++
// 11.2.d
mpVertexBuffer[1] = createPlaneVB(mpDevice);
AccelerationStructureBuffers bottomLevelBuffers[2];

// The first bottom-level buffer is for the plane and the triangle
const uint32_t vertexCount[] = { 3, 6 }; // Triangle has 3 vertices, plane has 6
bottomLevelBuffers[0] = createBottomLevelAS(mpDevice, mpCmdList, mpVertexBuffer, vertexCount, 2);
mpBottomLevelAS[0] = bottomLevelBuffers[0].pResult;

// The second bottom-level buffer is for the triangle only
bottomLevelBuffers[1] = createBottomLevelAS(mpDevice, mpCmdList, mpVertexBuffer, vertexCount, 1);
mpBottomLevelAS[1] = bottomLevelBuffers[1].pResult;
```

The rest of the code is similar to the code in the previous tutorials, except we use the array and
geometryCount when querying for the prebuild info and when creating the BLAS.

## 11.3 Top-Level Acceleration Structure
Nothing fancy in the code here.

Update the Definition 
```c++
ID3D12ResourcePtr pBottomLevelAS[2] /* 11.3.a */
```
```c++
// 11.3.b Create the desc for the triangle/plane instance
pInstanceDesc[0].InstanceID = 0;
pInstanceDesc[0].InstanceContributionToHitGroupIndex = 0;
pInstanceDesc[0].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
memcpy(pInstanceDesc[0].Transform, &transformation[0], sizeof(pInstanceDesc[0].Transform));
pInstanceDesc[0].AccelerationStructure = pBottomLevelAS[0]->GetGPUVirtualAddress();
pInstanceDesc[0].InstanceMask = 0xFF;
```

Important to note, we are not increasing the instances size, we are just making the plane and the first triangle use the same instance [0].
```c++
for (uint32_t i = 1 /*11.3.c*/; i < 3; i++)
```

We are still initializing 3 D3D12_RAYTRACING_INSTANCE_DESC structures. The difference is the we initialize
the first D3D12_RAYTRACING_INSTANCE_DESC instance with the bottom-level acceleration structure
containing both geometries.

The code for the other instance descs remains the same and will not be repeated here.

Finally update the createAccelerationStructures() to accept all the changes we made to BLAS
```c++
// 11.3.d
AccelerationStructureBuffers topLevelBuffers = createTopLevelAS(mpDevice, mpCmdList, mpBottomLevelAS, mTlasSize);
```

Launching the application, we can see the result.
![image](https://user-images.githubusercontent.com/17934438/221356591-d619a603-33b4-4fd1-8322-606751104621.png)

As you can see, the plane uses the same hit-shader and vertex-colors as the first triangle. That’s because
they share the same shader-table record. In the next tutorial we will learn how to use a different shader-
table record for each geometry, which will allow us to use different resources and even execute a
different shader for each geometry.
