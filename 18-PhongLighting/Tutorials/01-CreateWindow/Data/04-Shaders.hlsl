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

// 4.2 Ray - Tracing Shaders 04 - Shaders.hlsl

// 4.3.a Ray-Generation Shader
RaytracingAccelerationStructure gRtScene : register(t0);
RWTexture2D<float4> gOutput : register(u0);

// 18.1
struct STriVertex
{
    float3 vertex;
    float3 normal;
    float3 tangent;
    float2 texCoord;
};
StructuredBuffer<STriVertex> BTriVertex : register(t1);
// 17.4.a
StructuredBuffer<uint> indices: register(t2);

// 10.1.a
cbuffer PerFrame : register(b0)
{
    float3 A;
    float3 B;
    float3 C;
}

float3 linearToSrgb(float3 c)
{
    // Based on http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
    float3 sq1 = sqrt(c);
    float3 sq2 = sqrt(sq1);
    float3 sq3 = sqrt(sq2);
    float3 srgb = 0.662002687 * sq1 + 0.684122060 * sq2 - 0.323583601 * sq3 - 0.0225411470 * c;
    return srgb;
}

// 7.1 Payload
struct RayPayload
{
    float3 color;
};

// 7.0
[shader("raygeneration")]
void rayGen()
{
    uint3 launchIndex = DispatchRaysIndex();
    uint3 launchDim = DispatchRaysDimensions();

    float2 crd = float2(launchIndex.xy);
    float2 dims = float2(launchDim.xy);

    float2 d = ((crd / dims) * 2.f - 1.f);
    float aspectRatio = dims.x / dims.y;

    RayDesc ray;
    ray.Origin = float3(0, 0, -2);
    ray.Direction = normalize(float3(d.x * aspectRatio, -d.y, 1));

    ray.TMin = 0;
    ray.TMax = 100000;

    RayPayload payload;
    TraceRay(gRtScene, 0 /*rayFlags*/, 0xFF, 0 /* ray index*/, 2 /* 13.4 MultiplierForGeometryContributionToShaderIndex */, 0, ray, payload);
    float3 col = linearToSrgb(payload.color);
    gOutput[launchIndex.xy] = float4(col, 1);
}

// 7.2 Miss Shader
[shader("miss")]
void miss(inout RayPayload payload)
{
    payload.color = float3(0.4, 0.6, 0.2);
}

// 16.2
static const float4 lightDiffuseColor = float4(0.2, 0.2, 0.2, 1.0);
static const float diffuseCoef = 0.9;
static const float3 lightPosition = float3(2.0, 2.0, -2.0);

static const float4 lightSpecularColor = float4(1, 1, 1, 1);
static const float specularCoef = 0.7;
static const float specularPower = 50;

static const float4 lightAmbientColor = float4(0.2, 0.2, 0.2, 1.0);
static const float4 albedo = float4(1.0, 0.0, 0.0, 1.0);

// 16.2 Diffuse lighting calculation.
float CalculateDiffuseCoefficient(in float3 hitPosition, in float3 incidentLightRay, in float3 normal)
{
    float fNDotL = saturate(dot(-incidentLightRay, normal));
    return fNDotL;
}

// Phong lighting specular component
float4 CalculateSpecularCoefficient(in float3 hitPosition, in float3 incidentLightRay, in float3 normal, in float specularPower)
{
    float3 reflectedLightRay = normalize(reflect(incidentLightRay, normal));
    return pow(saturate(dot(reflectedLightRay, normalize(-WorldRayDirection()))), specularPower);
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
        BTriVertex[instance + indices[0]].normal,
        BTriVertex[instance + indices[1]].normal,
        BTriVertex[instance + indices[2]].normal,
    };

    float3 hitNormal = HitAttribute(vertexNormals, attribs);

    // Diffuse component.
    float Kd = CalculateDiffuseCoefficient(hitPosition, incidentLightRay, hitNormal);
    float4 diffuseColor = diffuseCoef * Kd * lightDiffuseColor;

    // Specular component.
    float4 specularColor = float4(0, 0, 0, 0);
    float4 Ks = CalculateSpecularCoefficient(hitPosition, incidentLightRay, hitNormal, specularPower);
    specularColor = specularCoef * Ks * lightSpecularColor;

    // Ambient component.
    // Fake AO: Darken faces with normal facing downwards/away from the sky a little bit.
    float4 ambientColorMin = lightAmbientColor - 0.15;
    float4 ambientColorMax = lightAmbientColor;
    float fNDotL = saturate(dot(-incidentLightRay, hitNormal));
    float4 ambientColor = albedo * lerp(ambientColorMin, ambientColorMax, fNDotL);

    //payload.color = hitColor + diffuseColor;  
    payload.color = ambientColor + diffuseColor + specularColor;
}

// 13.1.a
struct ShadowPayload
{
    bool hit;
};

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

    // Specular component.
    float4 specularColor = float4(0, 0, 0, 0);
    float4 Ks = CalculateSpecularCoefficient(hitPosition, incidentLightRay, hitNormal, specularPower);
    specularColor = specularCoef * Ks * lightSpecularColor;

    // Ambient component.
    // Fake AO: Darken faces with normal facing downwards/away from the sky a little bit.
    float4 ambientColorMin = lightAmbientColor - 0.15;
    float4 ambientColorMax = lightAmbientColor;
    float fNDotL = saturate(dot(-incidentLightRay, hitNormal));
    float4 ambientColor = albedo * lerp(ambientColorMin, ambientColorMax, fNDotL);

    //payload.color = hitColor + diffuseColor;  
    payload.color = ambientColor + diffuseColor + specularColor + float4(0.7f, 0.7f, 0.7f, 1.0f) * factor;

    //payload.color = float4(0.7f, 0.7f, 0.7f, 1.0f) * factor + diffuseColor; // 
}

// 13.1.b
[shader("closesthit")]
void shadowChs(inout ShadowPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    payload.hit = true;
}

// 13.1.c
[shader("miss")]
void shadowMiss(inout ShadowPayload payload)
{
    payload.hit = false;
}

