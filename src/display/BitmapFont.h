#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// BitmapFont.h — Descriptor for a 1-bpp bitmap font used by X4Display
//
// Two storage formats are supported:
//
//   columnMajor = true  (Font5x7 legacy)
//     data[glyph_idx * glyphW + col] = column byte
//     bit 0 = top pixel row, bit (glyphH-1) = bottom row.
//     Works only for fonts with glyphH ≤ 8.
//
//   columnMajor = false  (Fira Code and future fonts)
//     data[glyph_idx * glyphH * bytesPerRow + row * bytesPerRow + b]
//     MSB of the first byte = leftmost pixel of that row.
//     bytesPerRow = ceil(glyphW / 8).
//
// The firstChar and numChars fields define which ASCII values are present;
// a character outside [firstChar, firstChar+numChars) is rendered as space.
// ─────────────────────────────────────────────────────────────────────────────

#include <stdint.h>

struct BitmapFont {
    const uint8_t* data;    ///< Packed glyph data (see format note above)
    uint8_t bytesPerRow;    ///< Bytes per row (row-major only; ignored for column-major)
    uint8_t glyphW;         ///< Pixel columns per glyph
    uint8_t glyphH;         ///< Pixel rows per glyph
    uint8_t advance;        ///< Pixel advance per character (glyphW + inter-glyph gap)
    uint8_t lineH;          ///< Line height in pixels (glyphH + leading)
    uint8_t firstChar;      ///< First ASCII code in the font (typically 0x20 = ' ')
    uint8_t numChars;       ///< Number of glyphs in the font
    bool    columnMajor;    ///< true = column-major (legacy), false = row-major
};

// ── Well-known font objects (defined in FontData.cpp) ────────────────────────

extern const BitmapFont FONT_5X7;       ///< Classic 5×7 pixel font (Font5x7.h)
extern const BitmapFont FONT_FIRA_8X16; ///< Fira Code Regular at 8×16 px
extern const BitmapFont FONT_FIRA_12X26;///< Fira Code Regular at 12×26 px
