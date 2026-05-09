// Copyright 2026 Quentin LEBASTARD <bstkbd@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define RAW_USAGE_PAGE 0xFF60
#define RAW_USAGE_ID 0x61

#ifdef EXTRA_EXTRA_LONG_COMBOS
#undef EXTRA_EXTRA_LONG_COMBOS
#endif

#ifdef EXTRA_SHORT_COMBOS
#undef EXTRA_SHORT_COMBOS
#endif

#ifdef EXTRA_LONG_COMBOS
#undef EXTRA_LONG_COMBOS
#endif

// enables custom global combo term by Argos
#ifndef COMBO_TERM_PER_COMBO
#define COMBO_TERM_PER_COMBO
#endif

// enables custom global tapping term by Argos
#ifndef TAPPING_TERM_PER_KEY
#define TAPPING_TERM_PER_KEY
#endif