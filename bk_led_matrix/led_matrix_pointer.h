// Copyright 2026 Quentin LEBASTARD <bstkbd@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "quantum.h"

/* Active pointing pictogram, centered in the well.
 * Returns false when pixels is NULL, pointing is not built in, or the mode has no picture. */
bool bklm_pointer_paint(RGB *pixels);
