/***************************************************************************
# Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***************************************************************************/
#include "01-CreateWindow.h"

// 2.1 createDxgiSwapChain
IDXGISwapChain3Ptr createDxgiSwapChain(IDXGIFactory4Ptr pFactory, HWND hwnd, uint32_t width, uint32_t height, DXGI_FORMAT format, ID3D12CommandQueuePtr pCommandQueue)
{
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = kDefaultSwapChainBuffers;
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = format;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    // CreateSwapChainForHwnd() doesn't accept IDXGISwapChain3 (Why MS? Why?)
    MAKE_SMART_COM_PTR(IDXGISwapChain1);
    IDXGISwapChain1Ptr pSwapChain;

    HRESULT hr = pFactory->CreateSwapChainForHwnd(pCommandQueue, hwnd, &swapChainDesc, nullptr, nullptr, &pSwapChain);
    if (FAILED(hr))
    {
        d3dTraceHR("Failed to create the swap-chain", hr);
        return false;
    }

    IDXGISwapChain3Ptr pSwapChain3;
    d3d_call(pSwapChain->QueryInterface(IID_PPV_ARGS(&pSwapChain3)));
    return pSwapChain3;
}

// 2.2 createDevice
ID3D12Device5Ptr createDevice(IDXGIFactory4Ptr pDxgiFactory)
{
    // Find the HW adapter
    IDXGIAdapter1Ptr pAdapter;

    for (uint32_t i = 0; DXGI_ERROR_NOT_FOUND != pDxgiFactory->EnumAdapters1(i, &pAdapter); i++)
    {
        DXGI_ADAPTER_DESC1 desc;
        pAdapter->GetDesc1(&desc);

        // Skip SW adapters
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
#ifdef _DEBUG
        ID3D12DebugPtr pDx12Debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pDx12Debug))))
        {
            pDx12Debug->EnableDebugLayer();
        }
#endif
        // Create the device
        ID3D12Device5Ptr pDevice;
        d3d_call(D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&pDevice)));

        D3D12_FEATURE_DATA_D3D12_OPTIONS5 features5;
        HRESULT hr = pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features5, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5));
        if (SUCCEEDED(hr) && features5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
        {
            return pDevice;
        }
    }

    msgBox("Raytracing is not supported on this device. Make sure your GPU supports DXR (such as Nvidia's Volta or Turing RTX) and you're on the latest drivers. The DXR fallback layer is not supported.");
    exit(1);
    return nullptr;
}

// 2.3 createCommandQueue
ID3D12CommandQueuePtr createCommandQueue(ID3D12Device5Ptr pDevice)
{
    ID3D12CommandQueuePtr pQueue;
    D3D12_COMMAND_QUEUE_DESC cqDesc = {};
    cqDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    cqDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    d3d_call(pDevice->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&pQueue)));
    return pQueue;
}

// 2.4 createDescriptorHeap
ID3D12DescriptorHeapPtr createDescriptorHeap(ID3D12Device5Ptr pDevice, uint32_t count, D3D12_DESCRIPTOR_HEAP_TYPE type, bool shaderVisible)
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = count;
    desc.Type = type;
    desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    ID3D12DescriptorHeapPtr pHeap;
    d3d_call(pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pHeap)));
    return pHeap;
}

// 2.5 createRTV
D3D12_CPU_DESCRIPTOR_HANDLE createRTV(ID3D12Device5Ptr pDevice, ID3D12ResourcePtr pResource, ID3D12DescriptorHeapPtr pHeap, uint32_t& usedHeapEntries, DXGI_FORMAT format)
{
    D3D12_RENDER_TARGET_VIEW_DESC desc = {};
    desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    desc.Format = format;
    desc.Texture2D.MipSlice = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = pHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += usedHeapEntries * pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    usedHeapEntries++;
    pDevice->CreateRenderTargetView(pResource, &desc, rtvHandle);
    return rtvHandle;
}

// 2.6 resourceBarrier
void resourceBarrier(ID3D12GraphicsCommandList4Ptr pCmdList, ID3D12ResourcePtr pResource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter)
{
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = pResource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = stateBefore;
    barrier.Transition.StateAfter = stateAfter;
    pCmdList->ResourceBarrier(1, &barrier);
}

// 2.7 submitCommandList
uint64_t submitCommandList(ID3D12GraphicsCommandList4Ptr pCmdList, ID3D12CommandQueuePtr pCmdQueue, ID3D12FencePtr pFence, uint64_t fenceValue)
{
    pCmdList->Close();
    ID3D12CommandList* pGraphicsList = pCmdList.GetInterfacePtr();
    pCmdQueue->ExecuteCommandLists(1, &pGraphicsList);
    fenceValue++;
    pCmdQueue->Signal(pFence, fenceValue);
    return fenceValue;
}

// 2.8 initDXR
void Tutorial01::initDXR(HWND winHandle, uint32_t winWidth, uint32_t winHeight)
{
    mHwnd = winHandle;
    mSwapChainSize = uvec2(winWidth, winHeight);

    // Initialize the debug layer for debug builds
#ifdef _DEBUG
    ID3D12DebugPtr pDebug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pDebug))))
    {
        pDebug->EnableDebugLayer();
    }
#endif
    // Create the DXGI factory
    IDXGIFactory4Ptr pDxgiFactory;
    d3d_call(CreateDXGIFactory1(IID_PPV_ARGS(&pDxgiFactory)));
    mpDevice = createDevice(pDxgiFactory);
    mpCmdQueue = createCommandQueue(mpDevice);
    mpSwapChain = createDxgiSwapChain(pDxgiFactory, mHwnd, winWidth, winHeight, DXGI_FORMAT_R8G8B8A8_UNORM, mpCmdQueue);

    // Create a RTV descriptor heap
    mRtvHeap.pHeap = createDescriptorHeap(mpDevice, kRtvHeapSize, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false);

    // Create the per-frame objects
    for (uint32_t i = 0; i < kDefaultSwapChainBuffers; i++)
    {
        d3d_call(mpDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mFrameObjects[i].pCmdAllocator)));
        d3d_call(mpSwapChain->GetBuffer(i, IID_PPV_ARGS(&mFrameObjects[i].pSwapChainBuffer)));
        mFrameObjects[i].rtvHandle = createRTV(mpDevice, mFrameObjects[i].pSwapChainBuffer, mRtvHeap.pHeap, mRtvHeap.usedEntries, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
    }

    // Create the command-list
    d3d_call(mpDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mFrameObjects[0].pCmdAllocator, nullptr, IID_PPV_ARGS(&mpCmdList)));

    // Create a fence and the event
    d3d_call(mpDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mpFence)));
    mFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

// 2.9 beginFrame
uint32_t Tutorial01::beginFrame()
{
    // 6.5 Bind the descriptor heaps
    ID3D12DescriptorHeap* heaps[] = { mpSrvUavHeap };
    mpCmdList->SetDescriptorHeaps(arraysize(heaps), heaps);
    return mpSwapChain->GetCurrentBackBufferIndex();
}

// 2.10 endFrame
void Tutorial01::endFrame(uint32_t rtvIndex)
{
    // 6.6 update before state D3D12_RESOURCE_STATE_COPY_DEST
    resourceBarrier(mpCmdList, mFrameObjects[rtvIndex].pSwapChainBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
    mFenceValue = submitCommandList(mpCmdList, mpCmdQueue, mpFence, mFenceValue);
    mpSwapChain->Present(0, 0);

    // Prepare the command list for the next frame
    uint32_t bufferIndex = mpSwapChain->GetCurrentBackBufferIndex();

    // 14.3.d Sync. We need to do this because the TLAS resources are not double-buffered and we are going to update them
    // Make sure we have the new back-buffer is ready
    //if (mFenceValue > kDefaultSwapChainBuffers)
    {
        //mpFence->SetEventOnCompletion(mFenceValue - kDefaultSwapChainBuffers + 1, mFenceEvent);
        mpFence->SetEventOnCompletion(mFenceValue, mFenceEvent);
        WaitForSingleObject(mFenceEvent, INFINITE);
    }

    mFrameObjects[bufferIndex].pCmdAllocator->Reset();
    mpCmdList->Reset(mFrameObjects[bufferIndex].pCmdAllocator, nullptr);
}

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

// 3.2 TriangleVB upload heap props
static const D3D12_HEAP_PROPERTIES kUploadHeapProps =
{
    D3D12_HEAP_TYPE_UPLOAD,
    D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
    D3D12_MEMORY_POOL_UNKNOWN,
    0,
    0,
};

// 15.5.a
struct TriVertex
{
    vec3 vertex;
    vec4 color;
};

// 3.3 createTriangleVB
ID3D12ResourcePtr createTriangleVB(ID3D12Device5Ptr pDevice)
{
    // 15.5.b
    const TriVertex vertices[] =
    {
        vec3(0,          1,  0), vec4(1, 0, 0, 1),
        vec3(0.866f,  -0.5f, 0), vec4(0, 1, 0, 1),
        vec3(-0.866f, -0.5f, 0), vec4(0, 0, 1, 1),
    };

    // For simplicity, we create the vertex buffer on the upload heap, but that's not required
    ID3D12ResourcePtr pBuffer = createBuffer(pDevice, sizeof(vertices), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
    uint8_t* pData;
    pBuffer->Map(0, nullptr, (void**)&pData);
    memcpy(pData, vertices, sizeof(vertices));
    pBuffer->Unmap(0, nullptr);
    return pBuffer;
}

// 3.4.a bottom-level acceleration structure
static const D3D12_HEAP_PROPERTIES kDefaultHeapProps =
{
    D3D12_HEAP_TYPE_DEFAULT,
    D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
    D3D12_MEMORY_POOL_UNKNOWN,
    0,
    0
};

//11.2.a bottom-level acceleration structure
Tutorial01::AccelerationStructureBuffers createBottomLevelAS(ID3D12Device5Ptr pDevice, ID3D12GraphicsCommandList4Ptr pCmdList, ID3D12ResourcePtr pVB[], const uint32_t vertexCount[], uint32_t geometryCount)
{
    // 11.2.b
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geomDesc;
    geomDesc.resize(geometryCount);

    for (uint32_t i = 0; i < geometryCount; i++)
    {
        geomDesc[i].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geomDesc[i].Triangles.VertexBuffer.StartAddress = pVB[i]->GetGPUVirtualAddress();
        geomDesc[i].Triangles.VertexBuffer.StrideInBytes = sizeof(TriVertex); // 15.6
        geomDesc[i].Triangles.VertexCount = vertexCount[i];
        geomDesc[i].Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        geomDesc[i].Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
    }

    // Get the size requirements for the scratch and AS buffers
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    // 11.2.c
    inputs.NumDescs = geometryCount;
    inputs.pGeometryDescs = geomDesc.data();
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
    pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

    // Create the buffers. They need to support UAV, and since we are going to immediately use them, we create them with an unordered-access state
    Tutorial01::AccelerationStructureBuffers buffers;
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

// 14.1.a
void buildTopLevelAS(ID3D12Device5Ptr pDevice, ID3D12GraphicsCommandList4Ptr pCmdList, ID3D12ResourcePtr pBottomLevelAS[2], uint64_t& tlasSize, float rotation, bool update, Tutorial01::AccelerationStructureBuffers& buffers)
{
    // First, get the size of the TLAS buffers and create them
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    // 14.1.b
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
    // 8.0.a 
    inputs.NumDescs = 3;
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
    pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

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

    D3D12_RAYTRACING_INSTANCE_DESC* pInstanceDesc;
    buffers.pInstanceDesc->Map(0, nullptr, (void**)&pInstanceDesc);
    ZeroMemory(pInstanceDesc, sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * 3);

    // Initialize the instance desc. We only have a single instance
    pInstanceDesc->InstanceID = 0;                            // This value will be exposed to the shader via InstanceID()
    pInstanceDesc->InstanceContributionToHitGroupIndex = 0;   // This is the offset inside the shader-table. We only have a single geometry, so the offset 0
    pInstanceDesc->Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;

    // 8.0.c
    mat4 transformation[3];
    transformation[0] = mat4(); // Identity
    // 14.1.d
    mat4 rotationMat = eulerAngleY(rotation);
    transformation[1] = translate(mat4(), vec3(-2, 0, 0)) * rotationMat;
    transformation[2] = translate(mat4(), vec3(2, 0, 0)) * rotationMat;

    // 11.3.b Create the desc for the triangle/plane instance
    pInstanceDesc[0].InstanceID = 0;
    pInstanceDesc[0].InstanceContributionToHitGroupIndex = 0;
    pInstanceDesc[0].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
    memcpy(pInstanceDesc[0].Transform, &transformation[0], sizeof(pInstanceDesc[0].Transform));
    pInstanceDesc[0].AccelerationStructure = pBottomLevelAS[0]->GetGPUVirtualAddress();
    pInstanceDesc[0].InstanceMask = 0xFF;

    // 8.0.d
    for (uint32_t i = 1 /*11.3.c*/; i < 3; i++)
    {
        pInstanceDesc[i].InstanceID = i; // This value will be exposed to the shader via InstanceID()
        // 13.3.a
        pInstanceDesc[i].InstanceContributionToHitGroupIndex = (i * 2) + 2;  // The indices are relative to to the start of the hit-table entries specified in Raytrace(), so we need 4 and 6
        pInstanceDesc[i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        mat4 m = transpose(transformation[i]); // GLM is column major, the INSTANCE_DESC is row major
        memcpy(pInstanceDesc[i].Transform, &m, sizeof(pInstanceDesc[i].Transform));
        pInstanceDesc[i].AccelerationStructure = pBottomLevelAS[1]->GetGPUVirtualAddress();
        pInstanceDesc[i].InstanceMask = 0xFF;
    }

    // Unmap
    buffers.pInstanceDesc->Unmap(0, nullptr);

    // Create the TLAS
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
    asDesc.Inputs = inputs;
    asDesc.Inputs.InstanceDescs = buffers.pInstanceDesc->GetGPUVirtualAddress();
    asDesc.DestAccelerationStructureData = buffers.pResult->GetGPUVirtualAddress();
    asDesc.ScratchAccelerationStructureData = buffers.pScratch->GetGPUVirtualAddress();

    // 14.1.e If this is an update operation, set the source buffer and the perform_update flag
    if (update)
    {
        asDesc.Inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
        asDesc.SourceAccelerationStructureData = buffers.pResult->GetGPUVirtualAddress();
    }

    pCmdList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

    // We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
    D3D12_RESOURCE_BARRIER uavBarrier = {};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = buffers.pResult;
    pCmdList->ResourceBarrier(1, &uavBarrier);
}

// 11.1.c
ID3D12ResourcePtr createPlaneVB(ID3D12Device5Ptr pDevice)
{
    // 15.7
    const TriVertex vertices[] =
    {
        vec3(-100, -1,  -2), vec4(1, 0, 0, 1),
        vec3(100, -1,  100), vec4(1, 0, 0, 1),
        vec3(-100, -1,  100), vec4(1, 0, 0, 1),

        vec3(-100, -1,  -2), vec4(1, 0, 0, 1),
        vec3(100, -1,  -2), vec4(1, 0, 0, 1),
        vec3(100, -1,  100), vec4(1, 0, 0, 1),
    };

    // For simplicity, we create the vertex buffer on the upload heap, but that's not required
    ID3D12ResourcePtr pBuffer = createBuffer(pDevice, sizeof(vertices), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
    uint8_t* pData;
    pBuffer->Map(0, nullptr, (void**)&pData);
    memcpy(pData, vertices, sizeof(vertices));
    pBuffer->Unmap(0, nullptr);
    return pBuffer;
}

// 3.6 createAccelerationStructures()
void Tutorial01::createAccelerationStructures()
{
    // 11.1.d
    mpVertexBuffer[0] = createTriangleVB(mpDevice);
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

    // 14.3.a Refit the top-level acceleration structure and set update to false
    buildTopLevelAS(mpDevice, mpCmdList, mpBottomLevelAS, mTlasSize, mRotation, false, mpTopLevelAS);
    mRotation += 0.005f;

    // The tutorial doesn't have any resource lifetime management, so we flush and sync here. This is not required by the DXR spec - you can submit the list whenever you like as long as you take care of the resources lifetime.
    mFenceValue = submitCommandList(mpCmdList, mpCmdQueue, mpFence, mFenceValue);
    mpFence->SetEventOnCompletion(mFenceValue, mFenceEvent);
    WaitForSingleObject(mFenceEvent, INFINITE);
    uint32_t bufferIndex = mpSwapChain->GetCurrentBackBufferIndex();
    mpCmdList->Reset(mFrameObjects[0].pCmdAllocator, nullptr);
}

// 4.1 Shader-Libraries
#include <sstream>
static dxc::DxcDllSupport gDxcDllHelper;
MAKE_SMART_COM_PTR(IDxcCompiler);
MAKE_SMART_COM_PTR(IDxcLibrary);
MAKE_SMART_COM_PTR(IDxcBlobEncoding);
MAKE_SMART_COM_PTR(IDxcOperationResult);

ID3DBlobPtr compileLibrary(const WCHAR* filename, const WCHAR* targetString)
{
    // Initialize the helper
    d3d_call(gDxcDllHelper.Initialize());
    IDxcCompilerPtr pCompiler;
    IDxcLibraryPtr pLibrary;
    d3d_call(gDxcDllHelper.CreateInstance(CLSID_DxcCompiler, &pCompiler));
    d3d_call(gDxcDllHelper.CreateInstance(CLSID_DxcLibrary, &pLibrary));

    // Open and read the file
    std::ifstream shaderFile(filename);
    if (shaderFile.good() == false)
    {
        msgBox("Can't open file " + wstring_2_string(std::wstring(filename)));
        return nullptr;
    }
    std::stringstream strStream;
    strStream << shaderFile.rdbuf();
    std::string shader = strStream.str();

    // Create blob from the string
    IDxcBlobEncodingPtr pTextBlob;
    d3d_call(pLibrary->CreateBlobWithEncodingFromPinned((LPBYTE)shader.c_str(), (uint32_t)shader.size(), 0, &pTextBlob));

    // Compile
    IDxcOperationResultPtr pResult;
    d3d_call(pCompiler->Compile(pTextBlob, filename, L"", targetString, nullptr, 0, nullptr, 0, nullptr, &pResult));

    // Verify the result
    HRESULT resultCode;
    d3d_call(pResult->GetStatus(&resultCode));
    if (FAILED(resultCode))
    {
        IDxcBlobEncodingPtr pError;
        d3d_call(pResult->GetErrorBuffer(&pError));
        std::string log = convertBlobToString(pError.GetInterfacePtr());
        msgBox("Compiler error:\n" + log);
        return nullptr;
    }

    MAKE_SMART_COM_PTR(IDxcBlob);
    IDxcBlobPtr pBlob;
    d3d_call(pResult->GetResult(&pBlob));
    return pBlob;
}

// 4.6.b DxilLibrary
struct DxilLibrary
{
    // 4.6.d
    DxilLibrary(ID3DBlobPtr pBlob, const WCHAR* entryPoint[], uint32_t entryPointCount) : pShaderBlob(pBlob)
    {
        // 4.6.e
        stateSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
        stateSubobject.pDesc = &dxilLibDesc;

        // 4.6.f
        dxilLibDesc = {};
        exportDesc.resize(entryPointCount);
        exportName.resize(entryPointCount);
        if (pBlob)
        {
            // 4.6.g
            dxilLibDesc.DXILLibrary.pShaderBytecode = pBlob->GetBufferPointer();
            dxilLibDesc.DXILLibrary.BytecodeLength = pBlob->GetBufferSize();
            dxilLibDesc.NumExports = entryPointCount;
            dxilLibDesc.pExports = exportDesc.data();

            // 4.6.h
            for (uint32_t i = 0; i < entryPointCount; i++)
            {
                exportName[i] = entryPoint[i];
                exportDesc[i].Name = exportName[i].c_str();
                exportDesc[i].Flags = D3D12_EXPORT_FLAG_NONE;
                exportDesc[i].ExportToRename = nullptr;
            }
        }
    };

    DxilLibrary() : DxilLibrary(nullptr, nullptr, 0) {}

    // 4.6.c
    D3D12_DXIL_LIBRARY_DESC dxilLibDesc = {};
    D3D12_STATE_SUBOBJECT stateSubobject{};
    ID3DBlobPtr pShaderBlob;
    std::vector<D3D12_EXPORT_DESC> exportDesc;
    std::vector<std::wstring> exportName;
};

// 4.6.i
static const WCHAR* kRayGenShader = L"rayGen";
static const WCHAR* kMissShader = L"miss";
static const WCHAR* kClosestHitShader = L"chs";
static const WCHAR* kHitGroup = L"HitGroup";
// 12.1.f
static const WCHAR* kPlaneChs = L"planeChs";
static const WCHAR* kPlaneHitGroup = L"PlaneHitGroup";
// 13.2.a
static const WCHAR* kShadowChs = L"shadowChs";
static const WCHAR* kShadowMiss = L"shadowMiss";
static const WCHAR* kShadowHitGroup = L"ShadowHitGroup";

// 4.6.j
DxilLibrary createDxilLibrary()
{
    // Compile the shader
    ID3DBlobPtr pDxilLib = compileLibrary(L"Data/04-Shaders.hlsl", L"lib_6_3");
    const WCHAR* entryPoints[] = { kRayGenShader, kMissShader, kPlaneChs /* 12.3.e */, kClosestHitShader, kShadowMiss /* 12.3.b */, kShadowChs /* 12.3.b */ };
    return DxilLibrary(pDxilLib, entryPoints, arraysize(entryPoints));
}

// 4.7.a HitProgram
struct HitProgram
{
    HitProgram(LPCWSTR ahsExport, LPCWSTR chsExport, const std::wstring& name) : exportName(name)
    {
        desc = {};
        desc.AnyHitShaderImport = ahsExport;
        desc.ClosestHitShaderImport = chsExport;
        desc.HitGroupExport = exportName.c_str();

        subObject.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
        subObject.pDesc = &desc;
    }

    std::wstring exportName;
    D3D12_HIT_GROUP_DESC desc;
    D3D12_STATE_SUBOBJECT subObject;
};

// 4.8.a RootSignature create helper class
ID3D12RootSignaturePtr createRootSignature(ID3D12Device5Ptr pDevice, const D3D12_ROOT_SIGNATURE_DESC& desc)
{
    ID3DBlobPtr pSigBlob;
    ID3DBlobPtr pErrorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &pSigBlob, &pErrorBlob);
    if (FAILED(hr))
    {
        std::string msg = convertBlobToString(pErrorBlob.GetInterfacePtr());
        msgBox(msg);
        return nullptr;
    }
    ID3D12RootSignaturePtr pRootSig;
    d3d_call(pDevice->CreateRootSignature(0, pSigBlob->GetBufferPointer(), pSigBlob->GetBufferSize(), IID_PPV_ARGS(&pRootSig)));
    return pRootSig;
}

// 4.8.b LocalRootSignature
struct LocalRootSignature
{
    LocalRootSignature(ID3D12Device5Ptr pDevice, const D3D12_ROOT_SIGNATURE_DESC& desc)
    {
        pRootSig = createRootSignature(pDevice, desc);
        pInterface = pRootSig.GetInterfacePtr();
        subobject.pDesc = &pInterface;
        subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
    }
    ID3D12RootSignaturePtr pRootSig;
    ID3D12RootSignature* pInterface = nullptr;
    D3D12_STATE_SUBOBJECT subobject = {};
};

// 4.8.c
struct RootSignatureDesc
{
    D3D12_ROOT_SIGNATURE_DESC desc = {};
    std::vector<D3D12_DESCRIPTOR_RANGE> range;
    std::vector<D3D12_ROOT_PARAMETER> rootParams;
};

// 4.8.d
RootSignatureDesc createRayGenRootDesc()
{
    // Create the root-signature
    RootSignatureDesc desc;
    desc.range.resize(2);
    // gOutput
    desc.range[0].BaseShaderRegister = 0;
    desc.range[0].NumDescriptors = 1;
    desc.range[0].RegisterSpace = 0;
    desc.range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    desc.range[0].OffsetInDescriptorsFromTableStart = 0;

    // gRtScene
    desc.range[1].BaseShaderRegister = 0;
    desc.range[1].NumDescriptors = 1;
    desc.range[1].RegisterSpace = 0;
    desc.range[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    desc.range[1].OffsetInDescriptorsFromTableStart = 1;

    desc.rootParams.resize(1);
    desc.rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    desc.rootParams[0].DescriptorTable.NumDescriptorRanges = 2;
    desc.rootParams[0].DescriptorTable.pDescriptorRanges = desc.range.data();

    // Create the desc
    desc.desc.NumParameters = 1;
    desc.desc.pParameters = desc.rootParams.data();
    desc.desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

    return desc;
}

// 4.8
struct ExportAssociation
{
    ExportAssociation(const WCHAR* exportNames[], uint32_t exportCount, const D3D12_STATE_SUBOBJECT* pSubobjectToAssociate)
    {
        association.NumExports = exportCount;
        association.pExports = exportNames;
        association.pSubobjectToAssociate = pSubobjectToAssociate;

        subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
        subobject.pDesc = &association;
    }

    D3D12_STATE_SUBOBJECT subobject = {};
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION association = {};
};

// 4.9 ShaderConfig
struct ShaderConfig
{
    ShaderConfig(uint32_t maxAttributeSizeInBytes, uint32_t maxPayloadSizeInBytes)
    {
        shaderConfig.MaxAttributeSizeInBytes = maxAttributeSizeInBytes;
        shaderConfig.MaxPayloadSizeInBytes = maxPayloadSizeInBytes;

        subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
        subobject.pDesc = &shaderConfig;
    }

    D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {};
    D3D12_STATE_SUBOBJECT subobject = {};
};

// 4.10 PipelineConfig
struct PipelineConfig
{
    PipelineConfig(uint32_t maxTraceRecursionDepth)
    {
        config.MaxTraceRecursionDepth = maxTraceRecursionDepth;

        subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
        subobject.pDesc = &config;
    }

    D3D12_RAYTRACING_PIPELINE_CONFIG config = {};
    D3D12_STATE_SUBOBJECT subobject = {};
};

// 4.11 GlobalRootSignature
struct GlobalRootSignature
{
    GlobalRootSignature(ID3D12Device5Ptr pDevice, const D3D12_ROOT_SIGNATURE_DESC& desc)
    {
        pRootSig = createRootSignature(pDevice, desc);
        pInterface = pRootSig.GetInterfacePtr();
        subobject.pDesc = &pInterface;
        subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
    }
    ID3D12RootSignaturePtr pRootSig;
    ID3D12RootSignature* pInterface = nullptr;
    D3D12_STATE_SUBOBJECT subobject = {};
};

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

    // 10.2.b
    for (uint32_t i = 0; i < 3; i++)
    {
        const uint32_t bufferSize = sizeof(vec4) * 3;
        mpConstantBuffer[i] = createBuffer(mpDevice, bufferSize, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
        uint8_t* pData;
        d3d_call(mpConstantBuffer[i]->Map(0, nullptr, (void**)&pData));
        memcpy(pData, &bufferData[i * 3], sizeof(bufferData));
        mpConstantBuffer[i]->Unmap(0, nullptr);
    }
}

// 13.2.h
RootSignatureDesc createPlaneHitRootDesc()
{
    RootSignatureDesc desc;
    desc.range.resize(1);
    desc.range[0].BaseShaderRegister = 0;
    desc.range[0].NumDescriptors = 1;
    desc.range[0].RegisterSpace = 0;
    desc.range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    desc.range[0].OffsetInDescriptorsFromTableStart = 0;

    desc.rootParams.resize(1);
    desc.rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    desc.rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
    desc.rootParams[0].DescriptorTable.pDescriptorRanges = desc.range.data();

    desc.desc.NumParameters = 1;
    desc.desc.pParameters = desc.rootParams.data();
    desc.desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

    return desc;
}

// 4.6 Creating the RT Pipeline State Object
void Tutorial01::createRtPipelineState()
{
    // Need 10 subobjects:
    //  1 for the DXIL library
    //  1 for hit-group
    //  2 for RayGen root-signature (root-signature and the subobject association)
    //  9.2.a 2 for hit-program root-signature (root-signature and the subobject association)
    //  9.2.b 2 for miss-shader root-signature (signature and association)
    //  2 for shader config (shared between all programs. 1 for the config, 1 for association)
    //  1 for pipeline config
    //  1 for the global root signature

    // 13.2.i
    std::array<D3D12_STATE_SUBOBJECT, 16> subobjects;
    uint32_t index = 0;

    // 4.6.k Create the DXIL library
    DxilLibrary dxilLib = createDxilLibrary();
    subobjects[index++] = dxilLib.stateSubobject; // 0 Library
    // 4.7.b createRtPipelineState
    HitProgram hitProgram(nullptr, kClosestHitShader, kHitGroup);
    subobjects[index++] = hitProgram.subObject; // 1 Hit Group

    // 12.1.c  Create the plane HitProgram
    HitProgram planeHitProgram(nullptr, kPlaneChs, kPlaneHitGroup);
    subobjects[index++] = planeHitProgram.subObject; // 2 Plane Hit Group

    // 13.2.c Create the shadow-ray hit group
    HitProgram shadowHitProgram(nullptr, kShadowChs, kShadowHitGroup);
    subobjects[index++] = shadowHitProgram.subObject; // 3 Shadow Hit Group

    // 4.8.e Create the ray-gen root-signature and association
    LocalRootSignature rgsRootSignature(mpDevice, createRayGenRootDesc().desc);
    subobjects[index] = rgsRootSignature.subobject; // 3 RayGen Root Sig

    // 4.8.a createRtPipelineState
    uint32_t rgsRootIndex = index++; // 3
    ExportAssociation rgsRootAssociation(&kRayGenShader, 1, &(subobjects[rgsRootIndex]));
    subobjects[index++] = rgsRootAssociation.subobject; // 4 Associate Root Sig to RGS

    // 9.2.d Create the hit root-signature and association
    LocalRootSignature hitRootSignature(mpDevice, createHitRootDesc().desc);
    subobjects[index] = hitRootSignature.subobject; // 5 Hit Root Sig

    uint32_t hitRootIndex = index++; // 5
    ExportAssociation hitRootAssociation(&kClosestHitShader, 1, &(subobjects[hitRootIndex]));
    subobjects[index++] = hitRootAssociation.subobject; // 6 Associate Hit Root Sig to Hit Group

    // 13.2.g Create the plane hit root-signature and association
    LocalRootSignature planeHitRootSignature(mpDevice, createPlaneHitRootDesc().desc);
    subobjects[index] = planeHitRootSignature.subobject; // 8 Plane Hit Root Sig

    uint32_t planeHitRootIndex = index++; // 8
    ExportAssociation planeHitRootAssociation(&kPlaneHitGroup, 1, &(subobjects[planeHitRootIndex]));
    subobjects[index++] = planeHitRootAssociation.subobject; // 9 Associate Plane Hit Root Sig to Plane Hit Group

    // 9.2.e Create the miss root-signature and association
    D3D12_ROOT_SIGNATURE_DESC emptyDesc = {};
    emptyDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
    LocalRootSignature missRootSignature(mpDevice, emptyDesc);
    subobjects[index] = missRootSignature.subobject; // 7 Miss Root Sig

    // 12.1.d
    uint32_t emptyRootIndex = index++; // 7
    const WCHAR* emptyRootExport[] = { kMissShader, kShadowChs /* 13.2.d */, kShadowMiss /* 13.2.d */ };
    ExportAssociation emptyRootAssociation(emptyRootExport, arraysize(emptyRootExport), &(subobjects[emptyRootIndex]));
    subobjects[index++] = emptyRootAssociation.subobject; // 8 Associate Miss Root Sig to Miss Shader

    // 4.9.a Bind the payload size to the programs
    ShaderConfig shaderConfig(sizeof(float) * 2, sizeof(float) * 3); // 7.1 Payload
    subobjects[index] = shaderConfig.subobject; // 9 Shader Config

    // 4.9.b
    uint32_t shaderConfigIndex = index++; // 9
    const WCHAR* shaderExports[] = { kMissShader, kClosestHitShader, kPlaneChs /*12.1.e*/, kRayGenShader, kShadowMiss /* 13.2.e */, kShadowChs /* 13.2.e */ };
    ExportAssociation configAssociation(shaderExports, arraysize(shaderExports), &(subobjects[shaderConfigIndex]));
    subobjects[index++] = configAssociation.subobject; // 10 Associate Shader Config to Miss, CHS, RGS

    // 4.10.a Create the pipeline config
    PipelineConfig config(2); // 13.2.f
    subobjects[index++] = config.subobject; // 11

    // 4.11.a Create the global root signature and store the empty signature
    GlobalRootSignature root(mpDevice, {});
    mpEmptyRootSig = root.pRootSig;
    subobjects[index++] = root.subobject; // 12

    // 4.12 Create the state
    D3D12_STATE_OBJECT_DESC desc;
    desc.NumSubobjects = index; // 13
    desc.pSubobjects = subobjects.data();
    desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;

    d3d_call(mpDevice->CreateStateObject(&desc, IID_PPV_ARGS(&mpPipelineState)));
}

void Tutorial01::createShaderTable()
{
    /** The shader-table layout is as follows:
        Entry 0 - Ray-gen program
        Entry 1 - Miss program for the primary ray
        Entry 2 - Miss program for the shadow ray
        Entries 3,4 - Hit programs for triangle 0 (primary followed by shadow)
        Entries 5,6 - Hit programs for the plane (primary followed by shadow)
        Entries 7,8 - Hit programs for triangle 1 (primary followed by shadow)
        Entries 9,10 - Hit programs for triangle 2 (primary followed by shadow)
        All entries in the shader-table must have the same size, so we will choose it base on the largest required entry.
        The triangle primary-ray hit program requires the largest entry - sizeof(program identifier) + 8 bytes for the constant-buffer root descriptor.
        The entry size must be aligned up to D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT
    */

    // Calculate the size and create the buffer
    mShaderTableEntrySize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    mShaderTableEntrySize += 8; // The hit shader constant-buffer descriptor
    mShaderTableEntrySize = align_to(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, mShaderTableEntrySize);
    uint32_t shaderTableSize = mShaderTableEntrySize * 11;

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

    // Entry 1 - primary ray miss
    memcpy(pData + mShaderTableEntrySize, pRtsoProps->GetShaderIdentifier(kMissShader), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

    // Entry 2 - shadow-ray miss
    memcpy(pData + mShaderTableEntrySize * 2, pRtsoProps->GetShaderIdentifier(kShadowMiss), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

    // Entry 3 - Triangle 0, primary ray. ProgramID and constant-buffer data
    uint8_t* pEntry3 = pData + mShaderTableEntrySize * 3;
    memcpy(pEntry3, pRtsoProps->GetShaderIdentifier(kHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    assert(((uint64_t)(pEntry3 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) % 8) == 0); // Root descriptor must be stored at an 8-byte aligned address
    *(D3D12_GPU_VIRTUAL_ADDRESS*)(pEntry3 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) = mpConstantBuffer[0]->GetGPUVirtualAddress();
    // 15.3.a
    *(uint64_t*)(pEntry3 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + sizeof(D3D12_GPU_VIRTUAL_ADDRESS)) = heapStart + mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 2; // The SRV comes 2 after the program id

    // Entry 4 - Triangle 0, shadow ray. ProgramID only
    uint8_t* pEntry4 = pData + mShaderTableEntrySize * 4;
    memcpy(pEntry4, pRtsoProps->GetShaderIdentifier(kShadowHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

    // Entry 5 - Plane, primary ray. ProgramID only and the TLAS SRV
    uint8_t* pEntry5 = pData + mShaderTableEntrySize * 5;
    memcpy(pEntry5, pRtsoProps->GetShaderIdentifier(kPlaneHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    *(uint64_t*)(pEntry5 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) = heapStart + mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV); // The SRV comes directly after the program id

    // Entry 6 - Plane, shadow ray
    uint8_t* pEntry6 = pData + mShaderTableEntrySize * 6;
    memcpy(pEntry6, pRtsoProps->GetShaderIdentifier(kShadowHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

    // Entry 7 - Triangle 1, primary ray. ProgramID and constant-buffer data
    uint8_t* pEntry7 = pData + mShaderTableEntrySize * 7;
    memcpy(pEntry7, pRtsoProps->GetShaderIdentifier(kHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    assert(((uint64_t)(pEntry7 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) % 8) == 0); // Root descriptor must be stored at an 8-byte aligned address
    *(D3D12_GPU_VIRTUAL_ADDRESS*)(pEntry7 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) = mpConstantBuffer[1]->GetGPUVirtualAddress();
    // 15.3.b
    *(uint64_t*)(pEntry7 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + sizeof(D3D12_GPU_VIRTUAL_ADDRESS)) = heapStart + mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 2; // The SRV comes 2 after the program id

    // Entry 8 - Triangle 1, shadow ray. ProgramID only
    uint8_t* pEntry8 = pData + mShaderTableEntrySize * 8;
    memcpy(pEntry8, pRtsoProps->GetShaderIdentifier(kShadowHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

    // Entry 9 - Triangle 2, primary ray. ProgramID and constant-buffer data
    uint8_t* pEntry9 = pData + mShaderTableEntrySize * 9;
    memcpy(pEntry9, pRtsoProps->GetShaderIdentifier(kHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    assert(((uint64_t)(pEntry9 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) % 8) == 0); // Root descriptor must be stored at an 8-byte aligned address
    *(D3D12_GPU_VIRTUAL_ADDRESS*)(pEntry9 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) = mpConstantBuffer[2]->GetGPUVirtualAddress();
    // 15.3.c
    *(uint64_t*)(pEntry9 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + sizeof(D3D12_GPU_VIRTUAL_ADDRESS)) = heapStart + mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 2; // The SRV comes 2 after the program id

    // Entry 10 - Triangle 2, shadow ray. ProgramID only
    uint8_t* pEntry10 = pData + mShaderTableEntrySize * 10;
    memcpy(pEntry10, pRtsoProps->GetShaderIdentifier(kShadowHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

    // Unmap
    mpShaderTable->Unmap(0, nullptr);
}

// 6.0 01-CreateWindow.cpp
void Tutorial01::createShaderResources()
{
    // Create the output resource. The dimensions and format should match the swap-chain
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.DepthOrArraySize = 1;
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // The backbuffer is actually DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, but sRGB formats can't be used with UAVs. We will convert to sRGB ourselves in the shader
    resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    resDesc.Width = mSwapChainSize.x;
    resDesc.Height = mSwapChainSize.y;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resDesc.MipLevels = 1;
    resDesc.SampleDesc.Count = 1;
    d3d_call(mpDevice->CreateCommittedResource(&kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(&mpOutputResource))); // Starting as copy-source to simplify onFrameRender()

    // 15.1.a Create an SRV/UAV descriptor heap. Need 3 entries - 1 SRV for the scene and 1 UAV for the output and 1 for the vertex information
    mpSrvUavHeap = createDescriptorHeap(mpDevice, 3, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

    // Create the UAV. Based on the root signature we created it should be the first entry
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    mpDevice->CreateUnorderedAccessView(mpOutputResource, nullptr, &uavDesc, mpSrvUavHeap->GetCPUDescriptorHandleForHeapStart());

    // 6.1 Create the TLAS SRV right after the UAV. Note that we are using a different SRV desc here
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    // 14.3.d
    srvDesc.RaytracingAccelerationStructure.Location = mpTopLevelAS.pResult->GetGPUVirtualAddress();
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = mpSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
    srvHandle.ptr += mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    mpDevice->CreateShaderResourceView(nullptr, &srvDesc, srvHandle);

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
}

//////////////////////////////////////////////////////////////////////////
// Callbacks
//////////////////////////////////////////////////////////////////////////
void Tutorial01::onLoad(HWND winHandle, uint32_t winWidth, uint32_t winHeight)
{
    // 2.11 onLoad
    initDXR(winHandle, winWidth, winHeight); // Tutorial 02
    createAccelerationStructures(); // Tutorial 03
    createRtPipelineState(); // Tutorial 04
    createShaderResources(); // Tutorial 06. Need to do this before initializing the shader-table
    createConstantBuffer(); // Tutorial 09. Yes, we need to do it before creating the shader-table
    createShaderTable(); // Tutorial 05
}

void Tutorial01::onFrameRender()
{
    // 2.12 onFrameRender
    uint32_t rtvIndex = beginFrame();

    // Refit the top-level acceleration structure
    buildTopLevelAS(mpDevice, mpCmdList, mpBottomLevelAS, mTlasSize, mRotation, true, mpTopLevelAS);
    mRotation += 0.005f;

    // 6.4 this is rasterization and no longer needed
    //const float clearColor[4] = { 0.4f, 0.6f, 0.2f, 1.0f };
    //resourceBarrier(mpCmdList, mFrameObjects[rtvIndex].pSwapChainBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    //mpCmdList->ClearRenderTargetView(mFrameObjects[rtvIndex].rtvHandle, clearColor, 0, nullptr);

    // 6.4.a Let's raytrace
    resourceBarrier(mpCmdList, mpOutputResource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    D3D12_DISPATCH_RAYS_DESC raytraceDesc = {};
    raytraceDesc.Width = mSwapChainSize.x;
    raytraceDesc.Height = mSwapChainSize.y;
    raytraceDesc.Depth = 1;

    // 6.4.b RayGen is the first entry in the shader-table
    raytraceDesc.RayGenerationShaderRecord.StartAddress = mpShaderTable->GetGPUVirtualAddress() + 0 * mShaderTableEntrySize;
    raytraceDesc.RayGenerationShaderRecord.SizeInBytes = mShaderTableEntrySize;

    // 6.4.c Miss is the second entry in the shader-table
    size_t missOffset = 1 * mShaderTableEntrySize;
    raytraceDesc.MissShaderTable.StartAddress = mpShaderTable->GetGPUVirtualAddress() + missOffset;
    raytraceDesc.MissShaderTable.StrideInBytes = mShaderTableEntrySize;
    raytraceDesc.MissShaderTable.SizeInBytes = mShaderTableEntrySize * 2;   // 13.3.b 2 miss-entries

    // 6.4.d Hit is the third entry in the shader-table
    size_t hitOffset = 3 * mShaderTableEntrySize; // 13.3.c
    raytraceDesc.HitGroupTable.StartAddress = mpShaderTable->GetGPUVirtualAddress() + hitOffset;
    raytraceDesc.HitGroupTable.StrideInBytes = mShaderTableEntrySize;
    raytraceDesc.HitGroupTable.SizeInBytes = mShaderTableEntrySize * 8;    // 13.3.d 8 hit-entries

    // 6.4.e Bind the empty root signature
    mpCmdList->SetComputeRootSignature(mpEmptyRootSig);

    // 6.4.f Set Pipeline
    mpCmdList->SetPipelineState1(mpPipelineState.GetInterfacePtr());

    // 6.4.g Dispatch
    mpCmdList->DispatchRays(&raytraceDesc);

    // 6.4.h Copy the results to the back-buffer
    resourceBarrier(mpCmdList, mpOutputResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    resourceBarrier(mpCmdList, mFrameObjects[rtvIndex].pSwapChainBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
    mpCmdList->CopyResource(mFrameObjects[rtvIndex].pSwapChainBuffer, mpOutputResource);

    endFrame(rtvIndex);
}

void Tutorial01::onShutdown()
{
    // 2.13 onShutdown
    // Wait for the command queue to finish execution
    mFenceValue++;
    mpCmdQueue->Signal(mpFence, mFenceValue);
    mpFence->SetEventOnCompletion(mFenceValue, mFenceEvent);
    WaitForSingleObject(mFenceEvent, INFINITE);
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
    Framework::run(Tutorial01(), "Tutorial 01 - Create Window");
}
