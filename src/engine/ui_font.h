#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

// Растровый атлас шрифта (внутренний компонент ядра): печётся из
// системного шрифта через GDI (GetGlyphOutlineW, серый антиалиасинг)
// в 8-битную альфа-карту. Набор глифов: ASCII, кириллица, знак градуса.

typedef struct UiGlyph
{
    uint16_t codepoint;
    int16_t offsetX;    // от пера до левого края бокса, px
    int16_t offsetY;    // от базовой линии вверх до верха бокса, px
    uint16_t width;
    uint16_t height;
    float advance;
    float u0, v0, u1, v1;
} UiGlyph;

// Прямая таблица кодовая точка -> индекс глифа. Покрывает весь запекаемый
// диапазон (0x0020..0x0451), что даёт поиск за O(1) вместо бинарного:
// текст рисуется посимвольно каждый кадр, и это самый горячий путь UI.
#define UI_FONT_LOOKUP_FIRST 0x0020u
#define UI_FONT_LOOKUP_LAST  0x0451u
#define UI_FONT_LOOKUP_COUNT (UI_FONT_LOOKUP_LAST - UI_FONT_LOOKUP_FIRST + 1u)
#define UI_FONT_GLYPH_NONE   0xFFFFu

typedef struct UiFont
{
    uint8_t* atlas;      // альфа 8 бит, atlasWidth * atlasHeight
    uint32_t atlasWidth;
    uint32_t atlasHeight;
    UiGlyph* glyphs;     // отсортированы по codepoint
    uint32_t glyphCount;
    float ascent;        // от верха строки до базовой линии
    float lineHeight;
    int32_t pixelSize;
    uint16_t lookup[UI_FONT_LOOKUP_COUNT];  // UI_FONT_GLYPH_NONE — нет глифа
} UiFont;

// Печёт атлас под размер шрифта в пикселях; повторный вызов перепекает,
// освобождая старый. false — шрифт или память недоступны.
bool UiFontBake(UiFont* font, int32_t pixelSize);
void UiFontRelease(UiFont* font);

const UiGlyph* UiFontFindGlyph(const UiFont* font, uint16_t codepoint);
float UiFontMeasure(const UiFont* font, const wchar_t* text);

