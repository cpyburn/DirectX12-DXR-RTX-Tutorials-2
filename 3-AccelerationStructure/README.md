DirectX Raytracing Tutorials
============
This repository contain tutorials demonstrating how to use DirectX Raytracing.

Requirements:
- A GPU that supports DXR (Such as NVIDIA's Volta or Turing hardware)
- Windows 10 RS5 (version 1809) or newer
- [Windows 10 SDK version 1809 (10.0.17763.0)](https://developer.microsoft.com/en-us/windows/downloads/sdk-archive)
- Visual Studio 2022

DXR Tutorial 03
Acceleration Structure
Overview
Now that we can clear the screen, it’s time to render something. The following 5 tutorials will do just that – we will write some code that will use raytracing to render a triangle to the screen.
The first thing we need to create are acceleration structures. An acceleration structure is an opaque data structure that represents the scene’s geometry. This structure is used in rendering time to intersect rays against. For more information on it and optimized usage please refer to the spec. In this tutorial we will focus on how to create it.

## 3.0 01-CreateWindow.h
```c++ 
// Tutorial 3
void createAccelerationStructures();
ID3D12ResourcePtr mpVertexBuffer;
ID3D12ResourcePtr mpTopLevelAS;
ID3D12ResourcePtr mpBottomLevelAS;
uint64_t mTlasSize = 0;
```

Most of the action happens inside createAccelerationStructures().It’s a new function we added which is called from onLoad().

## 3.1 createBuffer
need helper method for creating all the buffers
```c++
// 3.1 createBuffer
ID3D12ResourcePtr createBuffer(ID3D12Device5Ptr pDevice, uint64_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initState, const D3D12_HEAP_PROPERTIES& heapProps)
{
    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Alignment = 0;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Flags = flags;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.Height = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufDesc.MipLevels = 1;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.SampleDesc.Quality = 0;
    bufDesc.Width = size;

    ID3D12ResourcePtr pBuffer;
    d3d_call(pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, initState, nullptr, IID_PPV_ARGS(&pBuffer)));
    return pBuffer;
}
```
## 3.2 TriangleVB upload heap props
need upload heap properties for creating buffers starting with the triangle vertex buffer
```c++
// 3.2 TriangleVB upload heap props
static const D3D12_HEAP_PROPERTIES kUploadHeapProps =
{
    D3D12_HEAP_TYPE_UPLOAD,
    D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
    D3D12_MEMORY_POOL_UNKNOWN,
    0,
    0,
};
```

## 3.3 createTriangleVB
The first line of code there is
* mpVertexBuffer = createTriangleVB(mpDevice);

This is a standard triangle vertex-buffer, created using the regular DX12 API and so we will not go into details. The only thing to note is that we allocate buffer on the upload heap, but that’s just for convenience as it simplifies the code.

```c++
// 3.3 createTriangleVB
ID3D12ResourcePtr createTriangleVB(ID3D12Device5Ptr pDevice)
{
    const vec3 vertices[] =
    {
        vec3(0,          1,  0),
        vec3(0.866f,  -0.5f, 0),
        vec3(-0.866f, -0.5f, 0),
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

## 3.4 bottom-level acceleration structure
Next, we will create the bottom-level acceleration structure

* AccelerationStructureBuffers bottomLevelBuffers = createBottomLevelAS(mpDevice, mpCmdList, mpVertexBuffer);
Bottom-Level Acceleration Structure

The BLAS is a data structure that represent a local-space mesh. It does not contain information regarding the world-space location of the vertices or instancing information. 

The first thing in creating it is initializing a D3D12_RAYTRACING_GEOMETRY_DESC struct:


We first set the type to D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES. This implies we will be using the built-in triangle intersection shader, but we will get to what that exactly means in tutorial 7. 

Next, we set the GPU virtual address of the vertex-buffer.

The next 3 fields are equivalent to an input element layout descriptor. They describe the vertex stride, the offset of the position element inside the vertex and the position format. We only have a single element in our VB, which is the position, meaning VertexByteOffset equals 0. Each vertex is exactly 3 floats, and that’s the size and format of the vertex.

Next, we will set the number of vertices in the buffer. We only have 3.
The Flags field allows us to control some aspects of the acceleration structure. In this case, we know that the triangle is not transparent and so we set the D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE flag.
The spec recommends using this flag as much as possible. We will get to what this flag means exactly in tutorial 7.

Now that we are done with the descriptor, let’s create the buffer. As you know, in DX12, resource allocation and lifetime management is the user’s responsibility. DXR is no different in this regard – it will not allocate buffers for us, not even internal temporary buffers required during acceleration structure creation.
DXR requires 2 buffers:
* Scratch buffer which is required for intermediate computation.
* The result buffer which will hold the acceleration data.
To allocate these buffers, we need to know the required size. This is done using the following snippet:
We first initialize a D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS struct:
* DescsLayout – We are using an array, so the layout is D3D12_ELEMENTS_LAYOUT_ARRAY.
* Flags – This field should match the value we will later use for building the acceleration structures. In our case we don’t use any special flags.
* The next 2 fields are the number of descriptors and the pointer to the descriptor array (in our case the array size is 1)
* Type – The type of the acceleration structure we are going to generate, bottom-level in our case.

Next, we need to call GetRaytracingAccelerationStructurePrebuildInfo() function. Once we get the information we can allocate the buffers:

The buffers are allocated on the default heap, since we don’t need read/write access to them. Both buffers must be created with the D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS flag because the implementation will be performing read/write operations, and synchronizing operations on these buffers are done through UAV barriers.

The spec also requires the state of the buffers to be:
* D3D12_RESOURCE_STATE_UNORDERED_ACCESS for the scratch buffer.
* D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE for the destination buffer.

Now that we have everything we need, we can create the acceleration structure. We start by initializing the AS descriptor. This also requires the same parameters we used to call GetRaytracingAccelerationStructurePrebuildInfo().

Additionally, we must pass the GPU virtual addresses of the destination AS and the scratch buffer.
Now that we have a descriptor ready, we can record a command.

```c++
// 3.4.a bottom-level acceleration structure
static const D3D12_HEAP_PROPERTIES kDefaultHeapProps =
{
    D3D12_HEAP_TYPE_DEFAULT,
    D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
    D3D12_MEMORY_POOL_UNKNOWN,
    0,
    0
};
// 3.4.b bottom-level acceleration structure
struct AccelerationStructureBuffers
{
    ID3D12ResourcePtr pScratch;
    ID3D12ResourcePtr pResult;
    ID3D12ResourcePtr pInstanceDesc;    // Used only for top-level AS
};
//3.4.c bottom-level acceleration structure
AccelerationStructureBuffers createBottomLevelAS(ID3D12Device5Ptr pDevice, ID3D12GraphicsCommandList4Ptr pCmdList, ID3D12ResourcePtr pVB)
{
    D3D12_RAYTRACING_GEOMETRY_DESC geomDesc = {};
    geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geomDesc.Triangles.VertexBuffer.StartAddress = pVB->GetGPUVirtualAddress();
    geomDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(vec3);
    geomDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geomDesc.Triangles.VertexCount = 3;
    geomDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    // Get the size requirements for the scratch and AS buffers
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    inputs.NumDescs = 1;
    inputs.pGeometryDescs = &geomDesc;
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
    pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

    // Create the buffers. They need to support UAV, and since we are going to immediately use them, we create them with an unordered-access state
    AccelerationStructureBuffers buffers;
    buffers.pScratch = createBuffer(pDevice, info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
    buffers.pResult = createBuffer(pDevice, info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, kDefaultHeapProps);

    // Create the bottom-level AS
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
    asDesc.Inputs = inputs;
    asDesc.DestAccelerationStructureData = buffers.pResult->GetGPUVirtualAddress();
    asDesc.ScratchAccelerationStructureData = buffers.pScratch->GetGPUVirtualAddress();

    pCmdList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

    // We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
    D3D12_RESOURCE_BARRIER uavBarrier = {};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = buffers.pResult;
    pCmdList->ResourceBarrier(1, &uavBarrier);

    return buffers;
}
```

Calling BuildRaytracingAccelerationStructure() will record a command into the list. This command will note be processed until we submit the command list, so make sure the scratch-buffer will not be released until execution finishes.
In the next section we will use the BLAS as an input for another BuildRaytracingAccelerationStructure() operation. We need to make sure that the write operation will finish before reading data from the result buffer. We do that using a regular UAV-barrier.



## 3.5 Top-Level Acceleration Structure
The TLAS is an opaque data structure that represents the entire scene. As you recall, BLAS represents objects in local space. The TLAS references the bottom-level structures, with each reference containing local-to-world transformation matrix.

Let’s take a look at createTopLevelAS().

Like bottom-level AS creation, we need to create the result and scratch buffers. The code is very similar, the only difference is how we query the required sizes. This happens in the following snippet:

The only difference is the Type field – we are requesting information for creating a TLAS.

Next, we will create the scratch and result buffer. Nothing new here.

Now we can proceed to describe the instances used for the TLAS. We do that by filling a buffer of D3D12_RAYTRACING_INSTANCE_DESC. We pass an array of such descriptors to the BuildRaytracingAcceleration() function. This array describes the scene.

The first thing to know about this array, is that it can’t simply reside on the regular C++ heap. We need to pass this array to BuildRaytracingAcceleration() in a GPU buffer (either on the upload or default heap). Since it’s a DX resource accessed by the GPU, all the regular synchronization and lifetime management rules apply.

We only have a single instance, so we create a buffer with that size, then map it to write. Next, we will initialize it.

The first field is InstanceID. It doesn’t affect raytracing at all, and the runtime ignores it while tracing rays. It’s simply a user-defined value that will communicated to the shader via the InstanceID() intrinsic.

InstanceContributionToHitGroupIndex is the offset of the instance inside the shader-binding-table. Let’s set it to 0 for now. This value will be explained in tutorial 5.

There are numerous options for the Flags. Refer to the spec for more details, in the tutorial we will just set it to D3D12_RAYTRACING_INSTANCE_FLAG_NONE.

Next is the transformation matrix. It’s a 3x4 affine transform matrix in row-major layout. This transformation will be applied to each vertex in the bottom-level structure. In this case, we are setting an identity matrix. This value can also be nullptr, which is equivalent to setting an identity matrix, but may result in better performance.

The last field – AccelerationStructure – is the GPU virtual address of the bottom-level acceleration structure containing the vertex data.

After we finish the initialization, we can unmap the desc-buffer and call BuildRaytracingAccelerationStructure(). The code is almost identical to the one used to create the BLAS, except:
* We need to set the instance-descriptor buffer GPU VA into the InstanceDescs field.
* We need to set the Type field to D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL

Just as we did for the BLAS, we need to insert a UAV barrier for the result buffer. This step is required because we need to make sure that the write operation performed in BuildRaytracingAccelerationStructure() finishes before the read operation in DispatchRays() (will be shown in tutorial 6).

```c++
// 3.5 Top-Level Acceleration Structure
AccelerationStructureBuffers createTopLevelAS(ID3D12Device5Ptr pDevice, ID3D12GraphicsCommandList4Ptr pCmdList, ID3D12ResourcePtr pBottomLevelAS, uint64_t& tlasSize)
{
    // First, get the size of the TLAS buffers and create them
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    inputs.NumDescs = 1;
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
    pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

    // Create the buffers
    AccelerationStructureBuffers buffers;
    buffers.pScratch = createBuffer(pDevice, info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
    buffers.pResult = createBuffer(pDevice, info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, kDefaultHeapProps);
    tlasSize = info.ResultDataMaxSizeInBytes;

    // The instance desc should be inside a buffer, create and map the buffer
    buffers.pInstanceDesc = createBuffer(pDevice, sizeof(D3D12_RAYTRACING_INSTANCE_DESC), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
    D3D12_RAYTRACING_INSTANCE_DESC* pInstanceDesc;
    buffers.pInstanceDesc->Map(0, nullptr, (void**)&pInstanceDesc);

    // Initialize the instance desc. We only have a single instance
    pInstanceDesc->InstanceID = 0;                            // This value will be exposed to the shader via InstanceID()
    pInstanceDesc->InstanceContributionToHitGroupIndex = 0;   // This is the offset inside the shader-table. We only have a single geometry, so the offset 0
    pInstanceDesc->Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
    mat4 m; // Identity matrix
    memcpy(pInstanceDesc->Transform, &m, sizeof(pInstanceDesc->Transform));
    pInstanceDesc->AccelerationStructure = pBottomLevelAS->GetGPUVirtualAddress();
    pInstanceDesc->InstanceMask = 0xFF;
    
    // Unmap
    buffers.pInstanceDesc->Unmap(0, nullptr);
        
    // Create the TLAS
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
    asDesc.Inputs = inputs;
    asDesc.Inputs.InstanceDescs = buffers.pInstanceDesc->GetGPUVirtualAddress();
    asDesc.DestAccelerationStructureData = buffers.pResult->GetGPUVirtualAddress();
    asDesc.ScratchAccelerationStructureData = buffers.pScratch->GetGPUVirtualAddress();

    pCmdList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

    // We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
    D3D12_RESOURCE_BARRIER uavBarrier = {};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = buffers.pResult;
    pCmdList->ResourceBarrier(1, &uavBarrier);

    return buffers;
}
```

## 3.6 createAccelerationStructures()
We created some buffers and recorded commands to create bottom-level and top-level acceleration structures. We now need to execute the command-list. To simplify resource lifetime management, we will submit the list and wait until the GPU finishes its execution. This is not required by the spec – the list can be submitted whenever as long as the resources are kept alive until execution finishes.

The last part is releasing resources that are no longer required and keep references to the resources which will be used for rendering.

Remember that we are using smart COM-pointers, so keeping reference is as simple as storing a copy of the smart-pointer. This happens in the following code:


Note that we need to store both top-level and bottom-level structures. The scratch buffers and the instance-desc buffers will be released automatically once the local variable holding their smart pointer goes out of scope.

And that’s it! We have acceleration structures, which means one major concept of DXRT is behind us!

```c++
// 3.6 createAccelerationStructures()
void Tutorial01::createAccelerationStructures()
{
    mpVertexBuffer = createTriangleVB(mpDevice);
    AccelerationStructureBuffers bottomLevelBuffers = createBottomLevelAS(mpDevice, mpCmdList, mpVertexBuffer);
    AccelerationStructureBuffers topLevelBuffers = createTopLevelAS(mpDevice, mpCmdList, bottomLevelBuffers.pResult, mTlasSize);

    // The tutorial doesn't have any resource lifetime management, so we flush and sync here. This is not required by the DXR spec - you can submit the list whenever you like as long as you take care of the resources lifetime.
    mFenceValue = submitCommandList(mpCmdList, mpCmdQueue, mpFence, mFenceValue);
    mpFence->SetEventOnCompletion(mFenceValue, mFenceEvent);
    WaitForSingleObject(mFenceEvent, INFINITE);
    uint32_t bufferIndex = mpSwapChain->GetCurrentBackBufferIndex();
    mpCmdList->Reset(mFrameObjects[0].pCmdAllocator, nullptr);

    // Store the AS buffers. The rest of the buffers will be released once we exit the function
    mpTopLevelAS = topLevelBuffers.pResult;
    mpBottomLevelAS = bottomLevelBuffers.pResult;
}
```

## 3.7 onLoad
```c++
createAccelerationStructures();             // Tutorial 03
```
