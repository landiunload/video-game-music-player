#include "engine/ui_font.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression); \
        ++failures; \
    } \
} while (0)

// Эталон, с которым сверяется таблица прямого доступа.
static const UiGlyph* FindGlyphLinear(const UiFont* font, uint16_t codepoint)
{
    for (uint32_t i = 0; i < font->glyphCount; ++i)
    {
        if (font->glyphs[i].codepoint == codepoint)
        {
            return &font->glyphs[i];
        }
    }
    return NULL;
}

static void TestLookupMatchesLinearScan(void)
{
    UiFont font;
    memset(&font, 0, sizeof(font));
    if (!UiFontBake(&font, 15))
    {
        fprintf(stderr, "FAIL: UiFontBake\n");
        ++failures;
        return;
    }

    CHECK(font.glyphCount > 0);
    CHECK(font.atlas != NULL);
    CHECK(font.lineHeight > 0.0f);

    // Полный обход BMP: быстрый поиск обязан совпадать с эталоном всюду,
    // включая кодовые точки вне запекаемых диапазонов.
    for (uint32_t codepoint = 0; codepoint <= 0xFFFF; ++codepoint)
    {
        const UiGlyph* fast = UiFontFindGlyph(&font, (uint16_t)codepoint);
        const UiGlyph* reference = FindGlyphLinear(&font, (uint16_t)codepoint);
        if (fast != reference)
        {
            fprintf(stderr, "FAIL: U+%04X fast=%p reference=%p\n",
                codepoint, (const void*)fast, (const void*)reference);
            ++failures;
            break;  // не заваливаем вывод повторами
        }
    }

    // Заведомо известные точки набора (числами, без зависимости от кодировки).
    CHECK(UiFontFindGlyph(&font, 0x0041) != NULL);  // A
    CHECK(UiFontFindGlyph(&font, 0x0020) != NULL);  // пробел
    CHECK(UiFontFindGlyph(&font, 0x0401) != NULL);  // Ё
    CHECK(UiFontFindGlyph(&font, 0x044F) != NULL);  // я
    CHECK(UiFontFindGlyph(&font, 0x0451) != NULL);  // ё
    CHECK(UiFontFindGlyph(&font, 0x00B0) != NULL);  // знак градуса
    CHECK(UiFontFindGlyph(&font, 0x0000) == NULL);
    CHECK(UiFontFindGlyph(&font, 0x0100) == NULL);  // внутри диапазона, нет глифа
    CHECK(UiFontFindGlyph(&font, 0x4E00) == NULL);  // за верхней границей

    CHECK(UiFontMeasure(&font, L"") == 0.0f);
    CHECK(UiFontMeasure(&font, L"AB") > 0.0f);

    UiFontRelease(&font);
    // После освобождения таблица обязана быть пустой, а не указывать в никуда.
    CHECK(UiFontFindGlyph(&font, 0x0041) == NULL);
    CHECK(font.glyphCount == 0);
}

int main(void)
{
    TestLookupMatchesLinearScan();
    if (failures == 0) printf("ui font: all tests passed\n");
    return failures == 0 ? 0 : 1;
}
