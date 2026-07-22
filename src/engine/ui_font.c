#include "engine/ui_font.h"

#include <windows.h>
#include <string.h>

// Диапазоны запекаемых кодовых точек (включительно).
typedef struct GlyphRange
{
    uint16_t first;
    uint16_t last;
} GlyphRange;

static const GlyphRange GLYPH_RANGES[] = {
    { 0x0020, 0x007E },  // ASCII
    { 0x00B0, 0x00B0 },  // знак градуса
    { 0x0401, 0x0401 },  // Ё
    { 0x0410, 0x044F },  // А..я
    { 0x0451, 0x0451 },  // ё
};
#define GLYPH_RANGE_COUNT (sizeof(GLYPH_RANGES) / sizeof(GLYPH_RANGES[0]))

#define GLYPH_PADDING 1

// Растр глифа во временной арене запекания: смещение и размер в байтах.
typedef struct GlyphBitmap
{
    size_t offset;
    uint32_t bytes;
} GlyphBitmap;

static void ReleaseBakeContext(HDC deviceContext, HGDIOBJ previousFont,
    HFONT gdiFont)
{
    SelectObject(deviceContext, previousFont);
    DeleteDC(deviceContext);
    DeleteObject(gdiFont);
}

static uint32_t CountGlyphs(void)
{
    uint32_t count = 0;
    for (uint32_t range = 0; range < GLYPH_RANGE_COUNT; ++range)
    {
        count += (uint32_t)(GLYPH_RANGES[range].last - GLYPH_RANGES[range].first + 1);
    }
    return count;
}

void UiFontRelease(UiFont* font)
{
    if (font->atlas != NULL)
    {
        HeapFree(GetProcessHeap(), 0, font->atlas);
        font->atlas = NULL;
    }
    if (font->glyphs != NULL)
    {
        HeapFree(GetProcessHeap(), 0, font->glyphs);
        font->glyphs = NULL;
    }
    font->glyphCount = 0;
    font->atlasWidth = 0;
    font->atlasHeight = 0;
    font->pixelSize = 0;
    memset(font->lookup, 0xFF, sizeof(font->lookup));
}

bool UiFontBake(UiFont* font, int32_t pixelSize)
{
    if (pixelSize < 6)
    {
        pixelSize = 6;
    }

    HFONT gdiFont = CreateFontW(
        -pixelSize, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    if (gdiFont == NULL)
    {
        return false;
    }

    HDC deviceContext = CreateCompatibleDC(NULL);
    if (deviceContext == NULL)
    {
        DeleteObject(gdiFont);
        return false;
    }
    HGDIOBJ previousFont = SelectObject(deviceContext, gdiFont);

    TEXTMETRICW textMetrics;
    if (!GetTextMetricsW(deviceContext, &textMetrics))
    {
        ReleaseBakeContext(deviceContext, previousFont, gdiFont);
        return false;
    }

    const MAT2 identity = { { 0, 1 }, { 0, 0 }, { 0, 0 }, { 0, 1 } };
    uint32_t glyphCount = CountGlyphs();
    UiGlyph* glyphs = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
        (size_t)glyphCount * sizeof(UiGlyph));
    GlyphBitmap* bitmaps = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
        (size_t)glyphCount * sizeof(GlyphBitmap));

    // Арена под растры всех глифов: GetGlyphOutlineW отдаёт метрики и
    // растр за один вызов, поэтому отдельный проход только за метриками
    // не нужен — он стоил 3.3 мс из 8.2 мс запекания (162 вызова GDI по
    // ~20 мкс). Оценка ёмкости — по площади кегля; при промахе арена
    // растёт вдвое, смещения от этого не портятся.
    size_t arenaCapacity = (size_t)glyphCount * (size_t)pixelSize
        * (size_t)pixelSize;
    if (arenaCapacity < 4096)
    {
        arenaCapacity = 4096;
    }
    uint8_t* arena = HeapAlloc(GetProcessHeap(), 0, arenaCapacity);
    if (glyphs == NULL || bitmaps == NULL || arena == NULL)
    {
        if (glyphs != NULL) HeapFree(GetProcessHeap(), 0, glyphs);
        if (bitmaps != NULL) HeapFree(GetProcessHeap(), 0, bitmaps);
        if (arena != NULL) HeapFree(GetProcessHeap(), 0, arena);
        ReleaseBakeContext(deviceContext, previousFont, gdiFont);
        return false;
    }

    // Верхняя оценка растра одного глифа: чёрный бокс не превышает
    // удвоенный кегль с запасом на наклон и антиалиасинг.
    uint32_t glyphBound = (uint32_t)pixelSize * 2u + 8u;
    size_t glyphReserve = (size_t)((glyphBound + 3u) & ~3u) * glyphBound;

    // Единственный проход GDI: метрики, растр и раскладка полками.
    uint32_t atlasWidth = pixelSize <= 28 ? 512 : 1024;
    uint32_t shelfX = GLYPH_PADDING;
    uint32_t shelfY = GLYPH_PADDING;
    uint32_t shelfHeight = 0;
    size_t arenaUsed = 0;
    uint32_t glyphIndex = 0;

    for (uint32_t range = 0; range < GLYPH_RANGE_COUNT; ++range)
    {
        for (uint32_t code = GLYPH_RANGES[range].first;
             code <= GLYPH_RANGES[range].last; ++code, ++glyphIndex)
        {
            UiGlyph* glyph = &glyphs[glyphIndex];
            glyph->codepoint = (uint16_t)code;

            if (arenaCapacity - arenaUsed < glyphReserve)
            {
                size_t grown = arenaCapacity * 2u;
                while (grown - arenaUsed < glyphReserve)
                {
                    grown *= 2u;
                }
                uint8_t* resized = HeapReAlloc(GetProcessHeap(), 0,
                    arena, grown);
                if (resized == NULL)
                {
                    HeapFree(GetProcessHeap(), 0, arena);
                    HeapFree(GetProcessHeap(), 0, bitmaps);
                    HeapFree(GetProcessHeap(), 0, glyphs);
                    ReleaseBakeContext(deviceContext, previousFont, gdiFont);
                    return false;
                }
                arena = resized;
                arenaCapacity = grown;
            }

            GLYPHMETRICS metrics;
            memset(&metrics, 0, sizeof(metrics));
            DWORD bytes = GetGlyphOutlineW(deviceContext, code,
                GGO_GRAY8_BITMAP, &metrics,
                (DWORD)(arenaCapacity - arenaUsed), arena + arenaUsed,
                &identity);
            if (bytes == GDI_ERROR)
            {
                // Глиф недоступен: аванс по ширине текста, бокс пустой.
                // (Места в арене заведомо хватало — она под glyphReserve.)
                wchar_t character = (wchar_t)code;
                SIZE extent;
                if (GetTextExtentPoint32W(deviceContext, &character, 1, &extent))
                {
                    glyph->advance = (float)extent.cx;
                }
                continue;
            }

            glyph->advance = (float)metrics.gmCellIncX;
            glyph->offsetX = (int16_t)metrics.gmptGlyphOrigin.x;
            glyph->offsetY = (int16_t)metrics.gmptGlyphOrigin.y;
            glyph->width = (uint16_t)metrics.gmBlackBoxX;
            glyph->height = (uint16_t)metrics.gmBlackBoxY;
            if (glyph->width == 0 || glyph->height == 0 || bytes == 0)
            {
                glyph->width = 0;
                glyph->height = 0;
                continue;
            }

            bitmaps[glyphIndex].offset = arenaUsed;
            bitmaps[glyphIndex].bytes = bytes;
            arenaUsed += bytes;

            uint32_t paddedWidth = (uint32_t)glyph->width + GLYPH_PADDING;
            if (shelfX + paddedWidth > atlasWidth)
            {
                shelfX = GLYPH_PADDING;
                shelfY += shelfHeight + GLYPH_PADDING;
                shelfHeight = 0;
            }

            // Временно кладём позицию в UV-поля (пиксели до нормировки).
            glyph->u0 = (float)shelfX;
            glyph->v0 = (float)shelfY;
            shelfX += paddedWidth;
            if ((uint32_t)glyph->height > shelfHeight)
            {
                shelfHeight = glyph->height;
            }
        }
    }

    uint32_t atlasHeight = shelfY + shelfHeight + GLYPH_PADDING;
    uint8_t* atlas = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
        (size_t)atlasWidth * atlasHeight);
    if (atlas == NULL)
    {
        HeapFree(GetProcessHeap(), 0, arena);
        HeapFree(GetProcessHeap(), 0, bitmaps);
        HeapFree(GetProcessHeap(), 0, glyphs);
        ReleaseBakeContext(deviceContext, previousFont, gdiFont);
        return false;
    }

    // Перенос растров из арены в атлас (GGO_GRAY8: 0..64 -> 0..255,
    // строки исходника выровнены по 4 байта). GDI здесь уже не нужен.
    for (uint32_t i = 0; i < glyphCount; ++i)
    {
        UiGlyph* glyph = &glyphs[i];
        if (glyph->width == 0 || glyph->height == 0)
        {
            glyph->u0 = glyph->v0 = glyph->u1 = glyph->v1 = 0.0f;
            continue;
        }

        uint32_t atlasX = (uint32_t)glyph->u0;
        uint32_t atlasY = (uint32_t)glyph->v0;
        const uint8_t* bitmap = arena + bitmaps[i].offset;
        uint32_t pitch = ((uint32_t)glyph->width + 3u) & ~3u;
        for (uint32_t row = 0; row < glyph->height; ++row)
        {
            const uint8_t* source = bitmap + (size_t)row * pitch;
            uint8_t* destination =
                atlas + (size_t)(atlasY + row) * atlasWidth + atlasX;
            for (uint32_t column = 0; column < glyph->width; ++column)
            {
                uint32_t value = source[column];
                destination[column] =
                    (uint8_t)(value >= 64 ? 255 : value * 4);
            }
        }

        float inverseWidth = 1.0f / (float)atlasWidth;
        float inverseHeight = 1.0f / (float)atlasHeight;
        glyph->u0 = (float)atlasX * inverseWidth;
        glyph->v0 = (float)atlasY * inverseHeight;
        glyph->u1 = (float)(atlasX + glyph->width) * inverseWidth;
        glyph->v1 = (float)(atlasY + glyph->height) * inverseHeight;
    }

    HeapFree(GetProcessHeap(), 0, arena);
    HeapFree(GetProcessHeap(), 0, bitmaps);
    ReleaseBakeContext(deviceContext, previousFont, gdiFont);

    UiFontRelease(font);
    font->atlas = atlas;
    font->atlasWidth = atlasWidth;
    font->atlasHeight = atlasHeight;
    font->glyphs = glyphs;
    font->glyphCount = glyphCount;
    memset(font->lookup, 0xFF, sizeof(font->lookup));
    for (uint32_t i = 0; i < glyphCount; ++i)
    {
        uint16_t codepoint = glyphs[i].codepoint;
        if (codepoint >= UI_FONT_LOOKUP_FIRST && codepoint <= UI_FONT_LOOKUP_LAST)
        {
            font->lookup[codepoint - UI_FONT_LOOKUP_FIRST] = (uint16_t)i;
        }
    }
    font->ascent = (float)textMetrics.tmAscent;
    font->lineHeight = (float)(textMetrics.tmHeight + textMetrics.tmExternalLeading);
    font->pixelSize = pixelSize;
    return true;
}

const UiGlyph* UiFontFindGlyph(const UiFont* font, uint16_t codepoint)
{
    if (codepoint < UI_FONT_LOOKUP_FIRST || codepoint > UI_FONT_LOOKUP_LAST)
    {
        return NULL;
    }
    uint16_t index = font->lookup[codepoint - UI_FONT_LOOKUP_FIRST];
    if (index >= font->glyphCount)
    {
        return NULL;  // UI_FONT_GLYPH_NONE либо атлас ещё не запечён
    }
    return &font->glyphs[index];
}

float UiFontMeasure(const UiFont* font, const wchar_t* text)
{
    float width = 0.0f;
    for (const wchar_t* character = text; *character != L'\0'; ++character)
    {
        const UiGlyph* glyph = UiFontFindGlyph(font, (uint16_t)*character);
        if (glyph != NULL)
        {
            width += glyph->advance;
        }
    }
    return width;
}
