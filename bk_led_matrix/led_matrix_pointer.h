// Copyright 2026 Quentin LEBASTARD <bstkbd@gmail.com>
// Copyright 2026 Ying Kun Zhan <ying@zhan.co.nl>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "quantum.h"

/* The active pointing mode: an animation for drag-scroll and sniping, otherwise
 * its pictogram centered in the well.
 * Returns false when pixels is NULL, pointing is not built in, or the mode has no picture. */
bool bklm_pointer_paint(RGB *pixels);
