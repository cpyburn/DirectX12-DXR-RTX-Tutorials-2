DirectX Raytracing Tutorials
============
This repository contain tutorials demonstrating how to use DirectX Raytracing.

Requirements:
- A GPU that supports DXR (Such as NVIDIA's Volta or Turing hardware)
- Windows 10 RS5 (version 1809) or newer
- [Windows 10 SDK version 1809 (10.0.17763.0)](https://developer.microsoft.com/en-us/windows/downloads/sdk-archive)
- Visual Studio 2022

# DXR Tutorial 16

## 16.0 Addding Normals and Diffuse Lighting

## Overview
In the last tutorial we saw how to use the vertex data in the shaders.  There isn't anything new here that we haven't already done so I will move quickly through all the code changes.

## 16.1
Update the TriVertex structure
```c++
// 15.5.a
struct TriVertex
{
    vec3 vertex;
    // 16.1.a
    vec3 normal;
};

```
One thing to note is that normal lighting would be hard to see with just 3 flat triangles (a single normal per vertex).  Even though a cube would look much better, to keep things simple, we will just add another triangle. Sooo, we add another triangle to the BLAS.

```c++
// 16.1.b
// The first bottom-level buffer is for the plane and the triangle
const uint32_t vertexCount[] = { 6, 6 }; // Triangle has 3 vertices x 2, plane has 6
```

Add normal data to the triangle and plane buffers

```c++
// 16.1.c
const TriVertex vertices[] =
{
    vec3(0,          1,  0), vec3(0, 0, -1),
    vec3(0.866f,  -0.5f, 0), vec3(0, 0, -1),
    vec3(-0.866f, -0.5f, 0), vec3(0, 0, -1),

    // Note: 16 also increase vertex count passed to const uint32_t vertexCount[] = { 6, 6 }
    vec3(0,          1,  0), vec3(1, 0, 0),
    vec3(0,  -0.5f, 0.866f), vec3(1, 0, 0),
    vec3(0, -0.5f, -0.866f), vec3(1, 0, 0),
};
```
```c++
// 16.1.d
const TriVertex vertices[] =
{
    vec3(-100, -1,  -2), vec3(0, 1, 0),
    vec3(100, -1,  100), vec3(0, 1, 0),
    vec3(-100, -1,  100), vec3(0, 1, 0),

    vec3(-100, -1,  -2), vec3(0, 1, 0),
    vec3(100, -1,  -2), vec3(0, 1, 0),
    vec3(100, -1,  100), vec3(0, 1, 0),
};
```

Update the createPlaneHitRootDesc()
```c++
// 16.1.e
RootSignatureDesc createPlaneHitRootDesc()
{
    RootSignatureDesc desc;
    desc.range.resize(2);
    desc.range[0].BaseShaderRegister = 0;
    desc.range[0].NumDescriptors = 1;
    desc.range[0].RegisterSpace = 0;
    desc.range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    desc.range[0].OffsetInDescriptorsFromTableStart = 0;

    // srv
    desc.range[1].BaseShaderRegister = 1;
    desc.range[1].NumDescriptors = 1;
    desc.range[1].RegisterSpace = 0;
    desc.range[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    desc.range[1].OffsetInDescriptorsFromTableStart = 1;

    desc.rootParams.resize(1);
    desc.rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    desc.rootParams[0].DescriptorTable.NumDescriptorRanges = 2;
    desc.rootParams[0].DescriptorTable.pDescriptorRanges = desc.range.data();

    desc.desc.NumParameters = 1;
    desc.desc.pParameters = desc.rootParams.data();
    desc.desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

    return desc;
}
```

Update createShaderTable()
```c++
// 16.1.f
*(uint64_t*)(pEntry5 + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + sizeof(uint64_t)) = heapStart + mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 2; // The SRV comes 2 after the program id
```

## 16.2 04-Shaders.hlsl
* Update the STriVertex
```c++
// 16.2
float3 normal;
```
* Add the methods that will help calculate diffuse lighting
```c++
// 16.2
static const float4 lightDiffuseColor = float4(0.2, 0.2, 0.2, 1.0);
static const float diffuseCoef = 0.9;
static const float3 lightPosition = float3(2.0, 2.0, -2.0);

// 16.2 Diffuse lighting calculation.
float CalculateDiffuseCoefficient(in float3 hitPosition, in float3 incidentLightRay, in float3 normal)
{
    float fNDotL = saturate(dot(-incidentLightRay, normal));
    return fNDotL;
}
// 16.2
float3 HitAttribute(float3 vertexAttribute[3], BuiltInTriangleIntersectionAttributes attr)
{
    return vertexAttribute[0] +
        attr.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
        attr.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

// 16.2 Retrieve hit world position.
float3 HitWorldPosition()
{
    return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}
```

## 16.3 Update the Hit Shaders
Add diffuse lighting to the hit shaders, chs and planechs
```c++
// 16.3.a
[shader("closesthit")]
void chs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    float3 barycentrics = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);
    float3 hitColor = A * barycentrics.x + B * barycentrics.y + C * barycentrics.z;

    // 15.4.b
    uint instance = InstanceID();
    //float3 hitColor = BTriVertex[instance].normal * barycentrics.x + BTriVertex[instance].normal * barycentrics.y + BTriVertex[instance].normal * barycentrics.z;

    float3 hitPosition = HitWorldPosition();
    float3 incidentLightRay = normalize(hitPosition - lightPosition);

    // Retrieve corresponding vertex normals for the triangle vertices.
    float3 vertexNormals[3] = {
        BTriVertex[instance + 0].normal,
        BTriVertex[instance + 1].normal,
        BTriVertex[instance + 2].normal,
    };

    float3 hitNormal = HitAttribute(vertexNormals, attribs);

    // Diffuse component.
    float Kd = CalculateDiffuseCoefficient(hitPosition, incidentLightRay, hitNormal);
    float4 diffuseColor = diffuseCoef * Kd * lightDiffuseColor;

    payload.color = diffuseColor; // hitColor + 
}
```
```c++
// 16.3.b
[shader("closesthit")]
void planeChs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    // 13.5.a
    float hitT = RayTCurrent();
    float3 rayDirW = WorldRayDirection();
    float3 rayOriginW = WorldRayOrigin();

    // 13.5.b Find the world-space hit position
    float3 posW = rayOriginW + hitT * rayDirW;

    // Fire a shadow ray. The direction is hard-coded here, but can be fetched from a constant-buffer
    RayDesc ray;
    ray.Origin = posW;
    // 13.5.c
    ray.Direction = normalize(float3(0.5, 0.5, -0.5));
    // 13.5.d
    ray.TMin = 0.01;
    ray.TMax = 100000;
    // 13.5.e
    ShadowPayload shadowPayload;
    TraceRay(gRtScene, 0  /*rayFlags*/, 0xFF, 1 /* ray index*/, 0, 1, ray, shadowPayload);
    // 13.5.f
    float factor = shadowPayload.hit ? 0.1 : 1.0;
    //payload.color = float4(0.9f, 0.9f, 0.9f, 1.0f) * factor;

    float3 hitPosition = HitWorldPosition();
    float3 incidentLightRay = normalize(hitPosition - lightPosition);

    // Retrieve corresponding vertex normals for the triangle vertices.
    uint vertId = PrimitiveIndex();
    float3 vertexNormals[3] = {
        BTriVertex[vertId + 0].normal,
        BTriVertex[vertId + 1].normal,
        BTriVertex[vertId + 2].normal,
    };

    float3 hitNormal = HitAttribute(vertexNormals, attribs);

    // Diffuse component.
    float Kd = CalculateDiffuseCoefficient(hitPosition, incidentLightRay, hitNormal);
    float4 diffuseColor = diffuseCoef * Kd * lightDiffuseColor;

    payload.color = diffuseColor; // float4(0.7f, 0.7f, 0.7f, 1.0f) * factor + 
}
```

![image](https://user-images.githubusercontent.com/17934438/222509414-c22fc5bd-a7cc-48d5-adc1-ec018cdda216.png)
