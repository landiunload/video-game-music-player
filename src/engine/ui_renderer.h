#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct UiRenderer UiRenderer;

#define RENDERER_UI_MAX_QUADS 4096U
#define RENDERER_UI_QUAD_TEXT 1U
#define RENDERER_UI_QUAD_IMAGE 2U

// GPU-формат унаследован от UI-pass laiue: 48 байт на квад,
// vertex pulling по SV_VertexID, без vertex/index buffers.
typedef struct RendererUiQuad
{
    float rect[4];
    float uv[4];
    uint32_t colorRGBA;
    float cornerRadius;
    uint32_t flags;
    uint32_t reserved;
} RendererUiQuad;

UiRenderer* UiRendererCreate(void* windowHandle, int32_t width, int32_t height);
void UiRendererDestroy(UiRenderer* renderer);

void UiRendererResize(UiRenderer* renderer, int32_t width, int32_t height);
void UiRendererSetVerticalSync(UiRenderer* renderer, bool enabled);

bool UiRendererSetFontAtlas(UiRenderer* renderer,
    const uint8_t* alphaPixels, uint32_t width, uint32_t height);
bool UiRendererLoadImage(UiRenderer* renderer, const wchar_t* path,
    uint32_t* outWidth, uint32_t* outHeight);
bool UiRendererClearImage(UiRenderer* renderer);

bool UiRendererBeginFrame(UiRenderer* renderer,
    float red, float green, float blue);
void UiRendererQueue(UiRenderer* renderer,
    const RendererUiQuad* quads, uint32_t count);
bool UiRendererEndFrame(UiRenderer* renderer);

