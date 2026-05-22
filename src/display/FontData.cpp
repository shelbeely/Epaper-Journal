// ─────────────────────────────────────────────────────────────────────────────
// FontData.cpp — Defines the well-known BitmapFont instances.
//
// This is the ONLY translation unit that includes the raw font data headers
// (Font5x7.h, FontFira_8x16.h, FontFira_12x26.h).  Those headers contain large
// static data arrays; including them from multiple .cpp files would produce
// duplicate-symbol linker errors.
//
// Other code accesses the fonts via the extern declarations in BitmapFont.h.
// ─────────────────────────────────────────────────────────────────────────────

#include "BitmapFont.h"
#include <stdint.h>

// ── Font data arrays (each header guards itself with #pragma once) ───────────
#include "Font5x7.h"
#include "FontFira_8x16.h"
#include "FontFira_12x26.h"

// ── Public BitmapFont instances ──────────────────────────────────────────────

// NOLINT: These definitions intentionally expose the static instances through
//         the well-known extern names declared in BitmapFont.h.

const BitmapFont FONT_5X7        = FONT_5X7_INSTANCE;
const BitmapFont FONT_FIRA_8X16  = FONT_FIRA_8X16_INSTANCE;   // NOLINT: symbol from FontFira_8x16.h
const BitmapFont FONT_FIRA_12X26 = FONT_FIRA_12X26_INSTANCE;  // NOLINT: symbol from FontFira_12x26.h
