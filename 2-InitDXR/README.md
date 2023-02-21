DirectX Raytracing Tutorials
============
This repository contain tutorials demonstrating how to use DirectX Raytracing.

Requirements:
- A GPU that supports DXR (Such as NVIDIA's Volta or Turing hardware)
- Windows 10 RS5 (version 1809) or newer
- [Windows 10 SDK version 1809 (10.0.17763.0)](https://developer.microsoft.com/en-us/windows/downloads/sdk-archive)
- Visual Studio 2022

DXR Tutorial 02
Initialize DXR

Overview
In this tutorial we will create all the API objects required to clear and present the back-buffer. We will need to create a device, swap-chain, command-queue, command-list, command-allocator, descriptor-heap and a fence. Remember – it is assumed that the user is familiar with DirectX12 programming, so we will not actually cover most of those objects.
Using DXR
As of Windows 10 version 1809, also known as RS5, DXR is no longer an experimental feature and is a part of standard DirectX 12. This means there are no extra steps required to enable DXR. However, note that DXR functions are a part of the ID3D12Device5 and ID3D12GraphicsCommandList4 interfaces.
The code here is normal D3D12 boilerplate application code – creating command-list, command-queue, command-allocator, fence object, swap-chain, render target view, etc.

