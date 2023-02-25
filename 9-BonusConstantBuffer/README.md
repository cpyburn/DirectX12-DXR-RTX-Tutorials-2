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

## 9.6 Bonus: make the miss shader use the CBV
The CBV is already created, so we need use the same root sig for both the hit and miss shaders, remove either the hit or the miss and associate both shaders to the root sig
```c++
// 9.6.a Bonus
uint32_t missHitRootIndex = index++; // 4
const WCHAR* missHitExportName[] = { kMissShader, kClosestHitShader };
ExportAssociation missHitRootAssociation(missHitExportName, arraysize(missHitExportName), &(subobjects[missHitRootIndex]));
subobjects[index++] = missHitRootAssociation.subobject; // 5 Associate Hit Root Sig to Miss and Hit Group 
```
Also update the subobject array to only have 10 sub objects
```c++
std::array<D3D12_STATE_SUBOBJECT, 10> subobjects;
```

Update the shader binding table
```c++
    // 9.6.b bonus
    // Entry 1 - miss program
    uint8_t* pMissEntry = pData + mShaderTableEntrySize;
    memcpy(pMissEntry, pRtsoProps->GetShaderIdentifier(kMissShader), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    // move to the root sig of the miss program and assign cbv
    uint8_t* pCbDesc = pMissEntry + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;  // Adding `progIdSize` gets us to the location of the constant-buffer entry
    assert(((uint64_t)pCbDesc % 8) == 0); // Root descriptor must be stored at an 8-byte aligned address
    *(D3D12_GPU_VIRTUAL_ADDRESS*)pCbDesc = mpConstantBuffer->GetGPUVirtualAddress();

    // Entry 2 - hit program. Program ID and one constant-buffer as root descriptor    
    uint8_t* pHitEntry = pData + mShaderTableEntrySize * 2; // +2 skips the ray-gen and miss entries
    memcpy(pHitEntry, pRtsoProps->GetShaderIdentifier(kHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    // move to the root sig of the hit program and assign the cbv
    pCbDesc = pHitEntry + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;  // Adding `progIdSize` gets us to the location of the constant-buffer entry
    assert(((uint64_t)pCbDesc % 8) == 0); // Root descriptor must be stored at an 8-byte aligned address
    *(D3D12_GPU_VIRTUAL_ADDRESS*)pCbDesc = mpConstantBuffer->GetGPUVirtualAddress();
```

Let's prove it worked by using the CBV in the miss shader
```c++
[shader("miss")]
void miss(inout RayPayload payload)
{
    payload.color = A[0];
}
```
Awesome, bonus is DONE!
![image](https://user-images.githubusercontent.com/17934438/221323878-d6feacc0-14b4-413c-b14e-abf0d648e708.png)



