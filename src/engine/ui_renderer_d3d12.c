#include "engine/ui_renderer.h"

#define COBJMACROS
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "engine/generated/ui_ps.h"
#include "engine/generated/ui_vs.h"
#include "engine/image_wic.h"

#include <stddef.h>
#include <limits.h>
#include <string.h>

#define FRAME_COUNT 2U
#define UI_QUAD_BYTES 48U
#define SRV_SLOT_FONT 0U
#define SRV_SLOT_IMAGE 1U
#define SRV_SLOT_COUNT 2U

struct UiRenderer
{
    IDXGIFactory4* factory;
    ID3D12Device* device;
    ID3D12CommandQueue* commandQueue;
    IDXGISwapChain3* swapChain;
    ID3D12CommandAllocator* commandAllocators[FRAME_COUNT];
    ID3D12GraphicsCommandList* commandList;
    ID3D12Resource* renderTargets[FRAME_COUNT];
    ID3D12DescriptorHeap* renderTargetViewHeap;
    UINT renderTargetViewSize;

    ID3D12Fence* fence;
    HANDLE fenceEvent;
    UINT64 nextFenceValue;
    UINT64 frameFenceValues[FRAME_COUNT];
    UINT frameIndex;

    ID3D12RootSignature* rootSignature;
    ID3D12PipelineState* pipelineState;
    ID3D12DescriptorHeap* srvHeap;
    UINT srvDescriptorSize;

    ID3D12Resource* quadBuffers[FRAME_COUNT];
    uint8_t* quadMapped[FRAME_COUNT];
    uint32_t quadCount;
    ID3D12Resource* fontTexture;
    ID3D12Resource* imageTexture;

    D3D12_VIEWPORT viewport;
    D3D12_RECT scissorRect;
    int32_t width;
    int32_t height;
    int32_t resizeWidth;
    int32_t resizeHeight;
    bool resizePending;
    bool verticalSync;
    bool tearingSupported;
    bool tearingEnabled;
    bool frameOpen;
};

_Static_assert(sizeof(RendererUiQuad) == UI_QUAD_BYTES,
    "RendererUiQuad must match shaders/ui.hlsl");

static D3D12_RESOURCE_BARRIER TransitionBarrier(ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER barrier;
    memset(&barrier, 0, sizeof(barrier));
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    return barrier;
}

static D3D12_CPU_DESCRIPTOR_HANDLE SrvCpuHandle(UiRenderer* renderer,
    uint32_t slot)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle;
    ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(
        renderer->srvHeap, &handle);
    handle.ptr += (SIZE_T)slot * renderer->srvDescriptorSize;
    return handle;
}

static D3D12_GPU_DESCRIPTOR_HANDLE SrvGpuHandle(UiRenderer* renderer,
    uint32_t slot)
{
    D3D12_GPU_DESCRIPTOR_HANDLE handle;
    ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(
        renderer->srvHeap, &handle);
    handle.ptr += (UINT64)slot * renderer->srvDescriptorSize;
    return handle;
}

static bool WaitForGpu(UiRenderer* renderer)
{
    if (renderer->commandQueue == NULL || renderer->fence == NULL
        || renderer->fenceEvent == NULL)
    {
        return false;
    }
    UINT64 value = renderer->nextFenceValue++;
    if (FAILED(ID3D12CommandQueue_Signal(renderer->commandQueue,
            renderer->fence, value)))
    {
        return false;
    }
    if (ID3D12Fence_GetCompletedValue(renderer->fence) < value)
    {
        if (FAILED(ID3D12Fence_SetEventOnCompletion(renderer->fence,
                value, renderer->fenceEvent)))
        {
            return false;
        }
        WaitForSingleObject(renderer->fenceEvent, INFINITE);
    }
    return true;
}

static bool AdvanceFrame(UiRenderer* renderer)
{
    UINT submittedFrame = renderer->frameIndex;
    UINT64 value = renderer->nextFenceValue++;
    if (FAILED(ID3D12CommandQueue_Signal(renderer->commandQueue,
            renderer->fence, value)))
    {
        return false;
    }
    renderer->frameFenceValues[submittedFrame] = value;
    renderer->frameIndex = IDXGISwapChain3_GetCurrentBackBufferIndex(
        renderer->swapChain);

    UINT64 required = renderer->frameFenceValues[renderer->frameIndex];
    if (required != 0
        && ID3D12Fence_GetCompletedValue(renderer->fence) < required)
    {
        if (FAILED(ID3D12Fence_SetEventOnCompletion(renderer->fence,
                required, renderer->fenceEvent)))
        {
            return false;
        }
        WaitForSingleObject(renderer->fenceEvent, INFINITE);
    }
    return true;
}

static bool CreateRootSignature(UiRenderer* renderer)
{
    D3D12_ROOT_PARAMETER parameters[3];
    memset(parameters, 0, sizeof(parameters));
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    parameters[0].Constants.ShaderRegister = 0;
    parameters[0].Constants.Num32BitValues = 2;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    parameters[1].Descriptor.ShaderRegister = 0;

    D3D12_DESCRIPTOR_RANGE textureRange;
    memset(&textureRange, 0, sizeof(textureRange));
    textureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    textureRange.NumDescriptors = SRV_SLOT_COUNT;
    textureRange.BaseShaderRegister = 1;
    textureRange.OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[2].DescriptorTable.NumDescriptorRanges = 1;
    parameters[2].DescriptorTable.pDescriptorRanges = &textureRange;

    D3D12_STATIC_SAMPLER_DESC sampler;
    memset(&sampler, 0, sizeof(sampler));
    sampler.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC description;
    memset(&description, 0, sizeof(description));
    description.NumParameters = 3;
    description.pParameters = parameters;
    description.NumStaticSamplers = 1;
    description.pStaticSamplers = &sampler;

    ID3DBlob* signature = NULL;
    ID3DBlob* errors = NULL;
    HRESULT result = D3D12SerializeRootSignature(&description,
        D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors);
    if (FAILED(result))
    {
        if (errors != NULL) ID3D10Blob_Release(errors);
        return false;
    }
    result = ID3D12Device_CreateRootSignature(renderer->device, 0,
        ID3D10Blob_GetBufferPointer(signature),
        ID3D10Blob_GetBufferSize(signature), &IID_ID3D12RootSignature,
        (void**)&renderer->rootSignature);
    ID3D10Blob_Release(signature);
    if (errors != NULL) ID3D10Blob_Release(errors);
    return SUCCEEDED(result);
}

static bool CreatePipelineState(UiRenderer* renderer)
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC description;
    memset(&description, 0, sizeof(description));
    description.pRootSignature = renderer->rootSignature;
    description.VS.pShaderBytecode = g_ui_vs;
    description.VS.BytecodeLength = sizeof(g_ui_vs);
    description.PS.pShaderBytecode = g_ui_ps;
    description.PS.BytecodeLength = sizeof(g_ui_ps);
    description.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    description.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    description.RasterizerState.DepthClipEnable = TRUE;

    D3D12_RENDER_TARGET_BLEND_DESC* blend =
        &description.BlendState.RenderTarget[0];
    blend->BlendEnable = TRUE;
    blend->SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend->DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blend->BlendOp = D3D12_BLEND_OP_ADD;
    blend->SrcBlendAlpha = D3D12_BLEND_ONE;
    blend->DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    blend->BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend->RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    description.SampleMask = UINT_MAX;
    description.PrimitiveTopologyType =
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    description.NumRenderTargets = 1;
    description.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    description.SampleDesc.Count = 1;
    return SUCCEEDED(ID3D12Device_CreateGraphicsPipelineState(
        renderer->device, &description, &IID_ID3D12PipelineState,
        (void**)&renderer->pipelineState));
}

static bool CreateQuadBuffers(UiRenderer* renderer)
{
    D3D12_HEAP_PROPERTIES heap = { .Type = D3D12_HEAP_TYPE_UPLOAD };
    D3D12_RESOURCE_DESC description;
    memset(&description, 0, sizeof(description));
    description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    description.Width = (UINT64)RENDERER_UI_MAX_QUADS * UI_QUAD_BYTES;
    description.Height = 1;
    description.DepthOrArraySize = 1;
    description.MipLevels = 1;
    description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    description.SampleDesc.Count = 1;

    for (UINT frame = 0; frame < FRAME_COUNT; ++frame)
    {
        if (FAILED(ID3D12Device_CreateCommittedResource(renderer->device,
                &heap, D3D12_HEAP_FLAG_NONE, &description,
                D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                &IID_ID3D12Resource,
                (void**)&renderer->quadBuffers[frame])))
        {
            return false;
        }
        D3D12_RANGE emptyRange = { 0, 0 };
        if (FAILED(ID3D12Resource_Map(renderer->quadBuffers[frame], 0,
                &emptyRange, (void**)&renderer->quadMapped[frame])))
        {
            return false;
        }
    }
    return true;
}

static bool CreateBackBuffers(UiRenderer* renderer)
{
    D3D12_RENDER_TARGET_VIEW_DESC description;
    memset(&description, 0, sizeof(description));
    description.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    description.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    D3D12_CPU_DESCRIPTOR_HANDLE handle;
    ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(
        renderer->renderTargetViewHeap, &handle);
    for (UINT frame = 0; frame < FRAME_COUNT; ++frame)
    {
        if (FAILED(IDXGISwapChain3_GetBuffer(renderer->swapChain, frame,
                &IID_ID3D12Resource,
                (void**)&renderer->renderTargets[frame])))
        {
            return false;
        }
        ID3D12Device_CreateRenderTargetView(renderer->device,
            renderer->renderTargets[frame], &description, handle);
        handle.ptr += renderer->renderTargetViewSize;
    }
    return true;
}

static bool UploadTexture(UiRenderer* renderer, const uint8_t* pixels,
    uint32_t width, uint32_t height, uint32_t bytesPerPixel,
    DXGI_FORMAT resourceFormat, DXGI_FORMAT viewFormat, uint32_t srvSlot,
    ID3D12Resource** destinationSlot)
{
    if (pixels == NULL || width == 0 || height == 0)
    {
        return false;
    }
    if (!WaitForGpu(renderer)) return false;

    D3D12_RESOURCE_DESC textureDescription;
    memset(&textureDescription, 0, sizeof(textureDescription));
    textureDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDescription.Width = width;
    textureDescription.Height = height;
    textureDescription.DepthOrArraySize = 1;
    textureDescription.MipLevels = 1;
    textureDescription.Format = resourceFormat;
    textureDescription.SampleDesc.Count = 1;

    D3D12_HEAP_PROPERTIES defaultHeap = { .Type = D3D12_HEAP_TYPE_DEFAULT };
    ID3D12Resource* texture = NULL;
    if (FAILED(ID3D12Device_CreateCommittedResource(renderer->device,
            &defaultHeap, D3D12_HEAP_FLAG_NONE, &textureDescription,
            D3D12_RESOURCE_STATE_COPY_DEST, NULL, &IID_ID3D12Resource,
            (void**)&texture)))
    {
        return false;
    }

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
    memset(&layout, 0, sizeof(layout));
    UINT64 uploadBytes = 0;
    ID3D12Device_GetCopyableFootprints(renderer->device,
        &textureDescription, 0, 1, 0, &layout, NULL, NULL, &uploadBytes);

    D3D12_RESOURCE_DESC uploadDescription;
    memset(&uploadDescription, 0, sizeof(uploadDescription));
    uploadDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDescription.Width = uploadBytes;
    uploadDescription.Height = 1;
    uploadDescription.DepthOrArraySize = 1;
    uploadDescription.MipLevels = 1;
    uploadDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    uploadDescription.SampleDesc.Count = 1;
    D3D12_HEAP_PROPERTIES uploadHeap = { .Type = D3D12_HEAP_TYPE_UPLOAD };
    ID3D12Resource* upload = NULL;
    if (FAILED(ID3D12Device_CreateCommittedResource(renderer->device,
            &uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDescription,
            D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource,
            (void**)&upload)))
    {
        ID3D12Resource_Release(texture);
        return false;
    }

    D3D12_RANGE emptyRange = { 0, 0 };
    uint8_t* mapped = NULL;
    bool succeeded = SUCCEEDED(ID3D12Resource_Map(upload, 0,
        &emptyRange, (void**)&mapped));
    if (succeeded)
    {
        size_t sourceStride = (size_t)width * bytesPerPixel;
        for (uint32_t row = 0; row < height; ++row)
        {
            memcpy(mapped + layout.Offset
                    + (size_t)row * layout.Footprint.RowPitch,
                pixels + (size_t)row * sourceStride, sourceStride);
        }
        ID3D12Resource_Unmap(upload, 0, NULL);
    }

    if (succeeded)
    {
        ID3D12CommandAllocator_Reset(
            renderer->commandAllocators[renderer->frameIndex]);
        succeeded = SUCCEEDED(ID3D12GraphicsCommandList_Reset(
            renderer->commandList,
            renderer->commandAllocators[renderer->frameIndex], NULL));
    }
    if (succeeded)
    {
        D3D12_TEXTURE_COPY_LOCATION target;
        memset(&target, 0, sizeof(target));
        target.pResource = texture;
        target.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        D3D12_TEXTURE_COPY_LOCATION source;
        memset(&source, 0, sizeof(source));
        source.pResource = upload;
        source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        source.PlacedFootprint = layout;
        ID3D12GraphicsCommandList_CopyTextureRegion(renderer->commandList,
            &target, 0, 0, 0, &source, NULL);
        D3D12_RESOURCE_BARRIER barrier = TransitionBarrier(texture,
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        ID3D12GraphicsCommandList_ResourceBarrier(renderer->commandList,
            1, &barrier);
        succeeded = SUCCEEDED(ID3D12GraphicsCommandList_Close(
            renderer->commandList));
    }
    if (succeeded)
    {
        ID3D12CommandList* commandLists[] = {
            (ID3D12CommandList*)renderer->commandList,
        };
        ID3D12CommandQueue_ExecuteCommandLists(renderer->commandQueue,
            1, commandLists);
        succeeded = WaitForGpu(renderer);
    }

    ID3D12Resource_Release(upload);
    if (!succeeded)
    {
        ID3D12Resource_Release(texture);
        return false;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC view;
    memset(&view, 0, sizeof(view));
    view.Format = viewFormat;
    view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    view.Texture2D.MipLevels = 1;
    ID3D12Device_CreateShaderResourceView(renderer->device, texture,
        &view, SrvCpuHandle(renderer, srvSlot));

    if (*destinationSlot != NULL) ID3D12Resource_Release(*destinationSlot);
    *destinationSlot = texture;
    return true;
}

UiRenderer* UiRendererCreate(void* windowHandle, int32_t width, int32_t height)
{
    if (windowHandle == NULL || width <= 0 || height <= 0) return NULL;
    UiRenderer* renderer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
        sizeof(*renderer));
    if (renderer == NULL) return NULL;
    renderer->width = width;
    renderer->height = height;
    renderer->verticalSync = true;
    renderer->nextFenceValue = 1;

    if (FAILED(CreateDXGIFactory2(0, &IID_IDXGIFactory4,
            (void**)&renderer->factory)))
    {
        UiRendererDestroy(renderer);
        return NULL;
    }
    IDXGIFactory5* factory5 = NULL;
    if (SUCCEEDED(IDXGIFactory4_QueryInterface(renderer->factory,
            &IID_IDXGIFactory5, (void**)&factory5)))
    {
        BOOL allowTearing = FALSE;
        if (SUCCEEDED(IDXGIFactory5_CheckFeatureSupport(factory5,
                DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing,
                sizeof(allowTearing))))
        {
            renderer->tearingSupported = allowTearing != FALSE;
            renderer->tearingEnabled = renderer->tearingSupported;
        }
        IDXGIFactory5_Release(factory5);
    }

    if (FAILED(D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0,
            &IID_ID3D12Device, (void**)&renderer->device)))
    {
        IDXGIAdapter* warp = NULL;
        if (FAILED(IDXGIFactory4_EnumWarpAdapter(renderer->factory,
                &IID_IDXGIAdapter, (void**)&warp)))
        {
            UiRendererDestroy(renderer);
            return NULL;
        }
        HRESULT result = D3D12CreateDevice((IUnknown*)warp,
            D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device,
            (void**)&renderer->device);
        IDXGIAdapter_Release(warp);
        if (FAILED(result))
        {
            UiRendererDestroy(renderer);
            return NULL;
        }
    }

    D3D12_COMMAND_QUEUE_DESC queue = {
        .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
    };
    if (FAILED(ID3D12Device_CreateCommandQueue(renderer->device, &queue,
            &IID_ID3D12CommandQueue, (void**)&renderer->commandQueue)))
    {
        UiRendererDestroy(renderer);
        return NULL;
    }

    DXGI_SWAP_CHAIN_DESC1 swapDescription;
    memset(&swapDescription, 0, sizeof(swapDescription));
    swapDescription.BufferCount = FRAME_COUNT;
    swapDescription.Width = (UINT)width;
    swapDescription.Height = (UINT)height;
    swapDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDescription.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapDescription.SampleDesc.Count = 1;
    swapDescription.Flags = renderer->tearingSupported
        ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    IDXGISwapChain1* swapChain1 = NULL;
    if (FAILED(IDXGIFactory4_CreateSwapChainForHwnd(renderer->factory,
            (IUnknown*)renderer->commandQueue, (HWND)windowHandle,
            &swapDescription, NULL, NULL, &swapChain1)))
    {
        UiRendererDestroy(renderer);
        return NULL;
    }
    HRESULT queryResult = IDXGISwapChain1_QueryInterface(swapChain1,
        &IID_IDXGISwapChain3, (void**)&renderer->swapChain);
    IDXGISwapChain1_Release(swapChain1);
    if (FAILED(queryResult))
    {
        UiRendererDestroy(renderer);
        return NULL;
    }
    IDXGIFactory4_MakeWindowAssociation(renderer->factory,
        (HWND)windowHandle, DXGI_MWA_NO_ALT_ENTER);
    renderer->frameIndex = IDXGISwapChain3_GetCurrentBackBufferIndex(
        renderer->swapChain);

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeap = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .NumDescriptors = FRAME_COUNT,
    };
    D3D12_DESCRIPTOR_HEAP_DESC srvHeap = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = SRV_SLOT_COUNT,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
    };
    if (FAILED(ID3D12Device_CreateDescriptorHeap(renderer->device,
            &rtvHeap, &IID_ID3D12DescriptorHeap,
            (void**)&renderer->renderTargetViewHeap))
        || FAILED(ID3D12Device_CreateDescriptorHeap(renderer->device,
            &srvHeap, &IID_ID3D12DescriptorHeap,
            (void**)&renderer->srvHeap)))
    {
        UiRendererDestroy(renderer);
        return NULL;
    }
    renderer->renderTargetViewSize =
        ID3D12Device_GetDescriptorHandleIncrementSize(renderer->device,
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    renderer->srvDescriptorSize =
        ID3D12Device_GetDescriptorHandleIncrementSize(renderer->device,
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    if (!CreateBackBuffers(renderer))
    {
        UiRendererDestroy(renderer);
        return NULL;
    }

    for (UINT frame = 0; frame < FRAME_COUNT; ++frame)
    {
        if (FAILED(ID3D12Device_CreateCommandAllocator(renderer->device,
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                &IID_ID3D12CommandAllocator,
                (void**)&renderer->commandAllocators[frame])))
        {
            UiRendererDestroy(renderer);
            return NULL;
        }
    }
    if (FAILED(ID3D12Device_CreateCommandList(renderer->device, 0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            renderer->commandAllocators[0], NULL,
            &IID_ID3D12GraphicsCommandList,
            (void**)&renderer->commandList)))
    {
        UiRendererDestroy(renderer);
        return NULL;
    }
    ID3D12GraphicsCommandList_Close(renderer->commandList);
    if (FAILED(ID3D12Device_CreateFence(renderer->device, 0,
            D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence,
            (void**)&renderer->fence)))
    {
        UiRendererDestroy(renderer);
        return NULL;
    }
    renderer->fenceEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (renderer->fenceEvent == NULL || !CreateRootSignature(renderer)
        || !CreatePipelineState(renderer) || !CreateQuadBuffers(renderer))
    {
        UiRendererDestroy(renderer);
        return NULL;
    }

    renderer->viewport.Width = (float)width;
    renderer->viewport.Height = (float)height;
    renderer->viewport.MaxDepth = 1.0f;
    renderer->scissorRect.right = width;
    renderer->scissorRect.bottom = height;

    const uint8_t white = 255;
    const uint8_t transparentImage[4] = { 255, 255, 255, 0 };
    if (!UploadTexture(renderer, &white, 1, 1, 1,
            DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8_UNORM,
            SRV_SLOT_FONT, &renderer->fontTexture)
        || !UploadTexture(renderer, transparentImage, 1, 1, 4,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
            SRV_SLOT_IMAGE, &renderer->imageTexture))
    {
        UiRendererDestroy(renderer);
        return NULL;
    }
    return renderer;
}

void UiRendererDestroy(UiRenderer* renderer)
{
    if (renderer == NULL) return;
    if (renderer->swapChain != NULL)
        IDXGISwapChain3_SetFullscreenState(renderer->swapChain, FALSE, NULL);
    if (renderer->commandQueue != NULL && renderer->fence != NULL
        && renderer->fenceEvent != NULL)
    {
        WaitForGpu(renderer);
    }
    if (renderer->imageTexture != NULL)
        ID3D12Resource_Release(renderer->imageTexture);
    if (renderer->fontTexture != NULL)
        ID3D12Resource_Release(renderer->fontTexture);
    for (UINT frame = 0; frame < FRAME_COUNT; ++frame)
    {
        if (renderer->quadBuffers[frame] != NULL)
        {
            if (renderer->quadMapped[frame] != NULL)
                ID3D12Resource_Unmap(renderer->quadBuffers[frame], 0, NULL);
            ID3D12Resource_Release(renderer->quadBuffers[frame]);
        }
        if (renderer->renderTargets[frame] != NULL)
            ID3D12Resource_Release(renderer->renderTargets[frame]);
        if (renderer->commandAllocators[frame] != NULL)
            ID3D12CommandAllocator_Release(
                renderer->commandAllocators[frame]);
    }
    if (renderer->pipelineState != NULL)
        ID3D12PipelineState_Release(renderer->pipelineState);
    if (renderer->rootSignature != NULL)
        ID3D12RootSignature_Release(renderer->rootSignature);
    if (renderer->srvHeap != NULL)
        ID3D12DescriptorHeap_Release(renderer->srvHeap);
    if (renderer->renderTargetViewHeap != NULL)
        ID3D12DescriptorHeap_Release(renderer->renderTargetViewHeap);
    if (renderer->commandList != NULL)
        ID3D12GraphicsCommandList_Release(renderer->commandList);
    if (renderer->fence != NULL) ID3D12Fence_Release(renderer->fence);
    if (renderer->fenceEvent != NULL) CloseHandle(renderer->fenceEvent);
    if (renderer->swapChain != NULL)
        IDXGISwapChain3_Release(renderer->swapChain);
    if (renderer->commandQueue != NULL)
        ID3D12CommandQueue_Release(renderer->commandQueue);
    if (renderer->device != NULL) ID3D12Device_Release(renderer->device);
    if (renderer->factory != NULL) IDXGIFactory4_Release(renderer->factory);
    HeapFree(GetProcessHeap(), 0, renderer);
}

void UiRendererResize(UiRenderer* renderer, int32_t width, int32_t height)
{
    if (renderer == NULL || width <= 0 || height <= 0) return;
    renderer->resizeWidth = width;
    renderer->resizeHeight = height;
    renderer->resizePending = true;
}

void UiRendererSetVerticalSync(UiRenderer* renderer, bool enabled)
{
    if (renderer != NULL) renderer->verticalSync = enabled;
}

static bool ApplyResize(UiRenderer* renderer)
{
    if (!renderer->resizePending) return true;
    if (!WaitForGpu(renderer)) return false;
    for (UINT frame = 0; frame < FRAME_COUNT; ++frame)
    {
        if (renderer->renderTargets[frame] != NULL)
        {
            ID3D12Resource_Release(renderer->renderTargets[frame]);
            renderer->renderTargets[frame] = NULL;
        }
        renderer->frameFenceValues[frame] = 0;
    }
    UINT flags = renderer->tearingSupported
        ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    if (FAILED(IDXGISwapChain3_ResizeBuffers(renderer->swapChain,
            FRAME_COUNT, (UINT)renderer->resizeWidth,
            (UINT)renderer->resizeHeight, DXGI_FORMAT_R8G8B8A8_UNORM,
            flags)))
    {
        return false;
    }
    renderer->frameIndex = IDXGISwapChain3_GetCurrentBackBufferIndex(
        renderer->swapChain);
    if (!CreateBackBuffers(renderer)) return false;
    renderer->width = renderer->resizeWidth;
    renderer->height = renderer->resizeHeight;
    renderer->viewport.Width = (float)renderer->width;
    renderer->viewport.Height = (float)renderer->height;
    renderer->scissorRect.right = renderer->width;
    renderer->scissorRect.bottom = renderer->height;
    renderer->resizePending = false;
    return true;
}

bool UiRendererSetFontAtlas(UiRenderer* renderer,
    const uint8_t* alphaPixels, uint32_t width, uint32_t height)
{
    return renderer != NULL && UploadTexture(renderer, alphaPixels,
        width, height, 1, DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8_UNORM,
        SRV_SLOT_FONT, &renderer->fontTexture);
}

bool UiRendererLoadImage(UiRenderer* renderer, const wchar_t* path,
    uint32_t* outWidth, uint32_t* outHeight)
{
    if (renderer == NULL || path == NULL) return false;
    uint8_t* pixels = NULL;
    uint32_t width = 0;
    uint32_t height = 0;
    if (!UiImageLoadRgba(path, &pixels, &width, &height)) return false;
    bool result = UploadTexture(renderer, pixels, width, height, 4,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        SRV_SLOT_IMAGE, &renderer->imageTexture);
    HeapFree(GetProcessHeap(), 0, pixels);
    if (result)
    {
        if (outWidth != NULL) *outWidth = width;
        if (outHeight != NULL) *outHeight = height;
    }
    return result;
}

bool UiRendererClearImage(UiRenderer* renderer)
{
    const uint8_t transparent[4] = { 255, 255, 255, 0 };
    return renderer != NULL && UploadTexture(renderer, transparent,
        1, 1, 4, DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        SRV_SLOT_IMAGE, &renderer->imageTexture);
}

bool UiRendererBeginFrame(UiRenderer* renderer,
    float red, float green, float blue)
{
    if (renderer == NULL || renderer->frameOpen || !ApplyResize(renderer))
        return false;
    if (FAILED(ID3D12CommandAllocator_Reset(
            renderer->commandAllocators[renderer->frameIndex]))
        || FAILED(ID3D12GraphicsCommandList_Reset(renderer->commandList,
            renderer->commandAllocators[renderer->frameIndex], NULL)))
    {
        return false;
    }
    renderer->quadCount = 0;
    renderer->frameOpen = true;

    D3D12_RESOURCE_BARRIER barrier = TransitionBarrier(
        renderer->renderTargets[renderer->frameIndex],
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    ID3D12GraphicsCommandList_ResourceBarrier(renderer->commandList,
        1, &barrier);
    D3D12_CPU_DESCRIPTOR_HANDLE rtv;
    ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(
        renderer->renderTargetViewHeap, &rtv);
    rtv.ptr += (SIZE_T)renderer->frameIndex * renderer->renderTargetViewSize;
    ID3D12GraphicsCommandList_OMSetRenderTargets(renderer->commandList,
        1, &rtv, FALSE, NULL);
    ID3D12GraphicsCommandList_RSSetViewports(renderer->commandList,
        1, &renderer->viewport);
    ID3D12GraphicsCommandList_RSSetScissorRects(renderer->commandList,
        1, &renderer->scissorRect);
    const float clearColor[4] = { red, green, blue, 1.0f };
    ID3D12GraphicsCommandList_ClearRenderTargetView(renderer->commandList,
        rtv, clearColor, 0, NULL);
    return true;
}

void UiRendererQueue(UiRenderer* renderer,
    const RendererUiQuad* quads, uint32_t count)
{
    if (renderer == NULL || !renderer->frameOpen
        || quads == NULL || count == 0) return;
    uint32_t remaining = RENDERER_UI_MAX_QUADS - renderer->quadCount;
    if (count > remaining) count = remaining;
    if (count == 0) return;
    memcpy(renderer->quadMapped[renderer->frameIndex]
            + (size_t)renderer->quadCount * UI_QUAD_BYTES,
        quads, (size_t)count * UI_QUAD_BYTES);
    renderer->quadCount += count;
}

bool UiRendererEndFrame(UiRenderer* renderer)
{
    if (renderer == NULL || !renderer->frameOpen) return false;
    renderer->frameOpen = false;

    D3D12_CPU_DESCRIPTOR_HANDLE rtv;
    ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(
        renderer->renderTargetViewHeap, &rtv);
    rtv.ptr += (SIZE_T)renderer->frameIndex * renderer->renderTargetViewSize;
    if (renderer->quadCount > 0)
    {
        ID3D12DescriptorHeap* heaps[] = { renderer->srvHeap };
        ID3D12GraphicsCommandList_SetDescriptorHeaps(renderer->commandList,
            1, heaps);
        ID3D12GraphicsCommandList_IASetPrimitiveTopology(
            renderer->commandList, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D12GraphicsCommandList_OMSetRenderTargets(renderer->commandList,
            1, &rtv, FALSE, NULL);
        ID3D12GraphicsCommandList_SetPipelineState(renderer->commandList,
            renderer->pipelineState);
        ID3D12GraphicsCommandList_SetGraphicsRootSignature(
            renderer->commandList, renderer->rootSignature);
        float screenSize[2] = {
            (float)renderer->width, (float)renderer->height,
        };
        ID3D12GraphicsCommandList_SetGraphicsRoot32BitConstants(
            renderer->commandList, 0, 2, screenSize, 0);
        ID3D12GraphicsCommandList_SetGraphicsRootShaderResourceView(
            renderer->commandList, 1,
            ID3D12Resource_GetGPUVirtualAddress(
                renderer->quadBuffers[renderer->frameIndex]));
        ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(
            renderer->commandList, 2,
            SrvGpuHandle(renderer, SRV_SLOT_FONT));
        ID3D12GraphicsCommandList_DrawInstanced(renderer->commandList,
            renderer->quadCount * 6, 1, 0, 0);
    }

    D3D12_RESOURCE_BARRIER barrier = TransitionBarrier(
        renderer->renderTargets[renderer->frameIndex],
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    ID3D12GraphicsCommandList_ResourceBarrier(renderer->commandList,
        1, &barrier);
    if (FAILED(ID3D12GraphicsCommandList_Close(renderer->commandList)))
        return false;
    ID3D12CommandList* commandLists[] = {
        (ID3D12CommandList*)renderer->commandList,
    };
    ID3D12CommandQueue_ExecuteCommandLists(renderer->commandQueue,
        1, commandLists);

    UINT syncInterval = renderer->verticalSync ? 1 : 0;
    UINT presentFlags = 0;
    if (!renderer->verticalSync && renderer->tearingEnabled)
    {
        BOOL fullscreen = TRUE;
        if (SUCCEEDED(IDXGISwapChain3_GetFullscreenState(renderer->swapChain,
                &fullscreen, NULL)) && !fullscreen)
        {
            presentFlags = DXGI_PRESENT_ALLOW_TEARING;
        }
    }
    HRESULT present = IDXGISwapChain3_Present(renderer->swapChain,
        syncInterval, presentFlags);
    if (present == DXGI_ERROR_INVALID_CALL
        && presentFlags == DXGI_PRESENT_ALLOW_TEARING)
    {
        renderer->tearingEnabled = false;
        present = IDXGISwapChain3_Present(renderer->swapChain, 0, 0);
    }
    return SUCCEEDED(present) && AdvanceFrame(renderer);
}
