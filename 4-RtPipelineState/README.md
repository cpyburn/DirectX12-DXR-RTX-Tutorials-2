DirectX Raytracing Tutorials
============
This repository contain tutorials demonstrating how to use DirectX Raytracing.

Requirements:
- A GPU that supports DXR (Such as NVIDIA's Volta or Turing hardware)
- Windows 10 RS5 (version 1809) or newer
- [Windows 10 SDK version 1809 (10.0.17763.0)](https://developer.microsoft.com/en-us/windows/downloads/sdk-archive)
- Visual Studio 2022

DXR Tutorial 04
Raytracing Pipeline State

Overview
Just like in regular rasterization code, we need to create a pipeline state that controls the fixed-function units and describes the shader which will be used. In this tutorial we will focus on the API that creates an ID3D12StateObjectPtr object. It’s a new DXR state interface, and is created differently than ID3D12PipelineState. This entire operation happens during load-time inside createRtPipelineState().

## 4.0 01-CreateWindow.h
```c++ 
// Tutorial 04
void createRtPipelineState();
ID3D12StateObjectPtr mpPipelineState;
ID3D12RootSignaturePtr mpEmptyRootSig;
```
## 4.1 Shader-Libraries
dxcompiler, the new SM6.x compiler, introduces a new concept called shader-libraries. Libraries allow us to compile a file containing multiple shaders without specifying an entry point. We create shader libraries by specifying "lib_6_3" as the target profile, which requires us to use an empty string for the entry point. Using dxcompiler is straightforward but is not in the scope of this tutorial. You can take a look at compileShader() to see an example usage. 
```c++
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
```

## 4.2 Ray-Tracing Shaders
DXR introduces 5 new shader types – ray-generation, miss, closest-hit, any-hit, and intersection. All the shaders for this tutorial can be found in 04-Shaders.hlsl.
*include 04-Shaders.hlsl and set property - Does not participate in build*
```c++
// 4.2 Ray-Tracing Shaders 04-Shaders.hlsl
RaytracingAccelerationStructure gRtScene : register(t0);
RWTexture2D<float4> gOutput : register(u0);

float3 linearToSrgb(float3 c)
{
    // Based on http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
    float3 sq1 = sqrt(c);
    float3 sq2 = sqrt(sq1);
    float3 sq3 = sqrt(sq2);
    float3 srgb = 0.662002687 * sq1 + 0.684122060 * sq2 - 0.323583601 * sq3 - 0.0225411470 * c;
    return srgb;
}

[shader("raygeneration")]
void rayGen()
{  
    uint3 launchIndex = DispatchRaysIndex();
    float3 col = linearToSrgb(float3(0.4, 0.6, 0.2));
    gOutput[launchIndex.xy] = float4(col, 1);
}

struct Payload
{
    bool hit;
};

[shader("miss")]
void miss(inout Payload payload)
{
    payload.hit = false;
}

[shader("closesthit")]
void chs(inout Payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    payload.hit = true;
}
```

## 4.3 Ray-Generation Shader
Ray-generation shader is the first stage in the ray-tracing pipeline. We will see in tutorial 06 that ray-tracing commands work on a 2D-grid. The ray-generation shader will be executed once per work item. This is where the user generates the primary-rays and dispatches ray-query calls.

Here is our ray-generation shader:
```c++
// 4.3.a Ray-Generation Shader
RaytracingAccelerationStructure gRtScene : register(t0);
RWTexture2D<float4> gOutput : register(u0);

// 4.3.b Ray-Generation Shader
[shader("raygeneration")]
void rayGen()
{  
    uint3 launchIndex = DispatchRaysIndex();
    float3 col = linearToSrgb(float3(0.4, 0.6, 0.2));
    gOutput[launchIndex.xy] = float4(col, 1);
}
```

The most notable thing in this file is the first line. There’s a new HLSL data type called RaytracingAccelerationStructure. As you can guess by the name, it contains a resource view to a TLAS. As you can see, it shares the same register type as a regular shader resource view.

gOutput is a UAV to a 2D texture which we will use to output the results.

As for the shader itself, we first declare the type of the shader - [shader("raygeneration")]. Remember that when we create a shader-library we do not specify a shader profile. The compiler uses that declaration to figure out the shader type.

The last thing to note is DispatchRaysIndex(). This intrinsic will return the 3D-index of the current work item.

The rest of the shader simply writes a constant color to the screen. In later tutorials we will see how to implement a more interesting RGS which dispatches rays.

## 4.4 Miss-Shader
A miss-shader will be called whenever a raytrace query did not hit any of the objects in the TLAS. Here’s our miss-shader:
```c++
// 4.4.a Miss - Shader
struct Payload
{
    bool hit;
};
// 4.4.b Miss - Shader
[shader("miss")]
void miss(inout Payload payload)
{
    payload.hit = false;
}
```

The miss-shader accepts a single inout argument – the ray payload. The ray-payload is a user-defined struct which is used to communicate data between the different shader stages. Note that the payload must be a struct. 

OK. Now to the last program type.

## 4.5 Hit-Group
A hit group is a collection of Closest-Hit, Any-Hit and Intersection Shaders. It is a single state element describing how to test for intersection and what should be the behavior in case an intersection is detected.

*Any Hit Shader* will be invoked whenever an intersection is found in the traversal. Its main use is to programmatically decide whether an intersection should be accepted. For example, for alpha-tested geometry we would like to ignore the intersection if the alpha test failed. The any-hit shader will be ignored for acceleration structures created with the D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE flag. Note that there is no guarantee on the order that any-hit shaders are executed when multiple intersections are found. This means the first invocation may not be the closest intersection to the origin, and the number of times the shader is invoked for a specific ray may vary!

*Closest Hit Shader* will be invoked exactly once per traversal if an intersection was found. As the name suggests, it will be invoked for the closest intersection found which falls in the ray range specified by the user (we will cover this in the next tutorial).

*Intersection Shader* will be invoked only when the primitive type is axis-aligned bounding-box. This topic is not covered in this tutorial series. For triangles, an internal triangle-intersection shader is used regardless of whether or not an intersection shader was specified. We can ignore this shader type for now.

As you might recall, our acceleration structures were built with the D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE flag and the primitive type is triangles. That means the only shader which is relevant in our case is the CHS. 
```c++
// 4.5 Hit-Group
[shader("closesthit")]
void chs(inout Payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    payload.hit = true;
}
```

This shader is very similar to the miss-shader above. It accepts the payload and another input – the intersection attributes. The intersection attributes come from the intersection shader. In our case, we are using the built-in intersection shader, which uses a new built-in HLSL struct to pass attributes:
```c++
struct BuiltInTriangleIntersectionAttributes
{
    float2 barycentrics;
};
```
We will see how to use these attributes in tutorial 7.

## Creating the RT Pipeline State Object
Now that we learnt about the new shader types, we can create our RTPSO. As mentioned before, creating an RTPSO is different from the way we created PSOs in DX12. Instead of a struct similar to D3D12_GRAPHICS_PIPELINE_STATE_DESC, we are going to build an array of D3D12_STATE_SUBOBJECT. Each sub-object describes a single element of the state. Most sub-objects reference other data structures, so we need to make sure all the referenced objects are valid when we call CreateStateObject(). For that reason, we are going to create a simple abstraction for each sub-object type.

Let’s go over the code in createRtPipelineState().
```c++
std::array&lt;D3D12_STATE_SUBOBJECT, 10&gt; subobjects;
```

First, we allocate an array containing 10 sub-objects. We will see why 10 as we progress through the tutorial.
*DxilLibrary*
This is our abstraction for a sub-object of type D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY. 
Let’s start from the struct members:
```c++
D3D12_DXIL_LIBRARY_DESC dxilLibDesc = {};
D3D12_STATE_SUBOBJECT stateSubobject{};
ID3DBlobPtr pShaderBlob;
std::vector&lt;D3D12_EXPORT_DESC&gt; exportDesc;
std::vector&lt;std::wstring&gt; exportName;
```
As you can see, we store a bunch of D3D12 objects. As we will see in a second, a DXIL library sub-objects needs to reference multiple structs and we need to make sure they are valid when we create the RTPSO. 
```c++
DxilLibrary(ID3DBlobPtr pBlob, const WCHAR* entryPoint[], uint32_t entryPointCount) :
```

The library accepts a single ID3DBlob object which contains an SM6.1 library. This library can contain multiple entry points, and we need to specify which entry points we plan to use. In our case, we store shaders entry points (see createDxilLibrary()).

Next, we will initialize the D3D12_STATE_SUBOBJECT. pDesc has a void* type, so we need to make sure we assign the right data structure to it. In this case, it’s a pointer to D3D12_DXIL_LIBRARY_DESC.
```c++
stateSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
stateSubobject.pDesc = &amp;dxilLibDesc;
```

Next, we clear the library desc and allocate space for the export desc and export names.
```c++
dxilLibDesc = {};
exportDesc.resize(entryPointCount);
exportName.resize(entryPointCount);
```


Now, assuming pBlob is not null, we can initialize the D3D12_DXIL_LIBRARY_DESC object.
```c++
dxilLibDesc.DXILLibrary.pShaderBytecode = pBlob-&gt;GetBufferPointer();
dxilLibDesc.DXILLibrary.BytecodeLength = pBlob-&gt;GetBufferSize();
dxilLibDesc.NumExports = entryPointCount;
dxilLibDesc.pExports = exportDesc.data();
```


We need to set the blob address, blob size, number of exports (AKA entry-points) and a pointer to the array of export-desc. Since we already resized our exportDesc vector, we know that the address will not change. Take care when dynamically resizing vectors, it’s not uncommon to forget that the address changes which makes pExports invalid.

We then go over the entry-points and initialize the D3D12_EXPORT_DESC vector.
```c++
for (uint32_t i = 0; i &lt; entryPointCount; i++)
{
exportName[i] = entryPoint[i];
exportDesc[i].Name = exportName[i].c_str();
exportDesc[i].Flags = D3D12_EXPORT_FLAG_NONE;
exportDesc[i].ExportToRename = nullptr;
}
```




2 things to note:
* We cache the entry-point name into a pre-allocated member vector of strings.
* We set ExportToRename to nullptr. Later we will see that we need a way to identify each shader inside a state-object. This is usually done by passing the entry-point name to the required function. There could be cases where shaders from different blobs share the same entry-point name, making the identification ambiguous. To resolve this, we can use ExportToRename to give each shader a unique name. In our case, we set it to nullptr since each shader has a unique-entry point name.

 
Back in createRtPipelineState(), we create a DxilLibrary  object and add it to the sub-object array.
```c++
// Create the DXIL library
DxilLibrary dxilLib = createDxilLibrary();
subobjects[index++] = dxilLib.stateSubobject;
```


We got ourselves our first sub-object object! As you can see, there’s a lot of memory management code here. This is a recurring theme with the new method of creating RTPSO.
## HitProgram
HitProgram is an abstraction over a D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP sub-object. A hit-group is a collection of intersection, any-hit and closest-hit shaders, at most one of each type. Since we don’t use custom intersection-shaders in these tutorials, our HitProgram object only accepts AHS and CHS entry point name.
```c++
HitProgram(LPCWSTR ahsExport, LPCWSTR chsExport, const std::wstring&amp; name) : exportName(name)
{
desc = {};
desc.AnyHitShaderImport = ahsExport;
desc.ClosestHitShaderImport = chsExport;
desc.HitGroupExport = exportName.c_str();
subObject.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
subObject.pDesc = &amp;desc;
}
```
The code should be self-explanatory. The AnyHitShaderImport and ClosestHitShaderImport reference export names declared in the DxilLibrary we created. HitGroupExport is a unique name we will use to identify this hit-group in subsequent calls.

## LocalRootSignature
DXR introduces a new concept called Local Root Signature (LRS). In graphics and compute pipelines, we have a single, global root-signature used by all programs. For ray-tracing, in addition to that root-signature, we can create local root-signatures and bind them to specific shaders. As we will see in the next tutorial, the size of the root-signature affects the size of the Shader Binding Table and LRSs allow us to optimize the SBT.

Looking at createRayGenRootDesc(), you can see that creating an LRS is similar to a global root-signature generation, except we need to set the D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE flag.

The LRS abstraction is in LocalRootSignature:
```c++
LocalRootSignature(ID3D12Device5Ptr pDevice, const D3D12_ROOT_SIGNATURE_DESC&amp; desc)
{
pRootSig = createRootSignature(pDevice, desc);
pInterface = pRootSig.GetInterfacePtr();
subobject.pDesc = &amp;pInterface;
subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
}
```
As you can see, we store the ID3D12RootSignature object into a member variable, then set its address into pDesc.

Now that we created a LocalRootSignature object, we need to associate it with one of the shader. We do that using something called an Export Assoication.

## ExportAssociation
An ExportAssociation object binds a sub-object into shaders and hit-groups. The object itself is very simple:
```c++
ExportAssociation(const WCHAR* exportNames[], uint32_t exportCount, const D3D12_STATE_SUBOBJECT*
pSubobjectToAssociate)
{
association.NumExports = exportCount;
association.pExports = exportNames;
association.pSubobjectToAssociate = pSubobjectToAssociate;
subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
subobject.pDesc = &amp;association;
}
```

There’s one important detail we must remember when creating an ExportAssociation object- pSubobjectToAssociate must point to an object which is part of the array we are passing into CreateStateObject(). Let’s look at how we use the last 2 objects and see what that means:

First, we create the local root-signature for the ray-generation shader. We then insert its D3D12_STATE_SUBOBJECT into the subobjects array. Then, when we create our ExportAssociation object, we pass the address of subobjects[rgsRootIndex]. Be careful not making the mistake of using the address of rgsRootSignature.subobject, it will not work as expected.

The next bit of code creates an empty local root-signature and binds it to the miss-shader and the hit-program. We need to bind an LRS for every object we export from our DxilLibrary. 
```c++
// Create the miss- and hit-programs root-signature and association
D3D12_ROOT_SIGNATURE_DESC emptyDesc = {};
emptyDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
LocalRootSignature hitMissRootSignature(mpDevice, emptyDesc);
subobjects[index] = hitMissRootSignature.subobject;
uint32_t hitMissRootIndex = index++;
const WCHAR* missHitExportName[] = { kMissShader, kClosestHitShader };
ExportAssociation missHitRootAssociation(missHitExportName, arraysize(missHitExportName),
&amp;(subobjects[hitMissRootIndex]));
subobjects[index++] = missHitRootAssociation.subobject;
```
An important detail to point out is that we are not using the hit group name specified earlier in the HitProgram sub-object. The official Windows 10 DXR release initially included an issue preventing associations with hit group names from working correctly. Normally, we would prefer to use hitProgram.exportName.c_str(), but until this is fixed you must include every entry point in the hit group when creating ExportAssociation objects. Currently, we only have one closest-hit shader.

## ShaderConfig
Next bit is the shader configuration. There are 2 values we need to set:
* The payload size in bytes. This is the size of the payload struct we defined in the HLSL. In our case the payload is a single bool (4-bytes in HLSL).
* The attributes size in bytes. This is the size of the data the hit-shader accepts as its intersection-attributes parameter. For the built-in intersection shader, the attributes size is 8-bytes (2 floats).
```c++
ShaderConfig(uint32_t maxAttributeSizeInBytes, uint32_t maxPayloadSizeInBytes)
{
shaderConfig.MaxAttributeSizeInBytes = maxAttributeSizeInBytes;
shaderConfig.MaxPayloadSizeInBytes = maxPayloadSizeInBytes;
subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
subobject.pDesc = &amp;shaderConfig;
}
```
The code should be self-explanatory. Once we create a ShaderConfig object, we need to associate it with our shaders, which is what the following snippet does.
```c++
// Bind the payload size to the programs
ShaderConfig shaderConfig(sizeof(float)*2, sizeof(float)*1);
subobjects[index] = shaderConfig.subobject;
uint32_t shaderConfigIndex = index++;
const WCHAR* shaderExports[] = { kMissShader, kClosestHitShader, kRayGenShader };
ExportAssociation configAssociation(shaderExports, arraysize(shaderExports),
&amp;(subobjects[shaderConfigIndex]));

subobjects[index++] = configAssociation.subobject;
```


Again, note that the ExportAssociation object accepts the address of a sub-object from the subobjects array. 

## PipelineConfig
The pipeline configuration is a global sub-object affecting all pipeline stages. In the case of raytracing, it contains a single value - MaxTraceRecursionDepth. This value simply tells the pipeline how many recursive raytracing calls we are going to make. Our object looks as follows:
```c++
PipelineConfig(uint32_t maxTraceRecursionDepth)
{
config.MaxTraceRecursionDepth = maxTraceRecursionDepth;
subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
subobject.pDesc = &amp;config;
}
```
Since our ray-generation shader doesn’t make any raytracing calls, we set this value to 0.
```c++
// Create the pipeline config
PipelineConfig config(0);
subobjects[index++] = config.subobject;
```
## GlobalRootSignature
The last piece of the puzzle is the global root-signature. As the name suggests, this root-signature affects all shaders attached to the pipeline. The final root-signature of a shader is defined by both the global and the shader’s local root-signature. The code is straightforward. This is how we initialize the GlobalRootSignature object:
```c++
GlobalRootSignature(ID3D12Device5Ptr pDevice, const D3D12_ROOT_SIGNATURE_DESC&amp; desc)
{
pRootSig = createRootSignature(pDevice, desc);
pInterface = pRootSig.GetInterfacePtr();
subobject.pDesc = &amp;pInterface;
subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
}
```
And this is how we use it, setting the global root-signature to an empty signature:
```c++
GlobalRootSignature root(mpDevice, {});
mpEmptyRootSig = root.pRootSig;
subobjects[index++] = root.subobject;
```


## CreateStateObject
Now that we initialized our array of D3D12_STATE_SUBOBJECT, we can finally create the ID3D12StateObject object. This is done using the following, simple snippet:
```c++
D3D12_STATE_OBJECT_DESC desc;
desc.NumSubobjects = index;
desc.pSubobjects = subobjects.data();
desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
d3d_call(mpDevice-&gt;CreateStateObject(&amp;desc, IID_PPV_ARGS(&amp;mpPipelineState)));
```
And we’re done!
There are more details and fine-print related to how to use sub-object and create a state-objects. I strongly suggest you read the spec to get all the details, but for now we have enough to work with.
