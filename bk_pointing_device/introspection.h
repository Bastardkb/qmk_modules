#pragma once

/*
    Previously we defined the keycodes in keyboard space.
    We switched to module space, which means the keycodes defined start at a different range.
    For legacy reasons, we manually define the keycodes here, so that we may stay backward compatible.
*/

// IMPORTANT:
// the trigger and toggle keycodes must be one after the other
// if there are keycodes in between, we must respect the odd/even rule.

#define DPI_MOD 0x7E00
#define DPI_RMOD 0x7E01
#define S_D_MOD 0x7E02
#define S_D_RMOD 0x7E03
#define SNIPING 0x7E04
#define SNP_TOG 0x7E05
#define DRGSCRL 0x7E06
#define DRG_TOG 0x7E07
#define CURSOR 0x7E08
#define CUR_TOG 0x7E09
#define PBRIGHT 0x7E0A
#define PBRIGHT_TOG 0x7E0B
#define PZOOM 0x7E0C
#define PZOOM_TOG 0x7E0D
#define PVOLUME 0x7E0E
#define PVOLUME_TOG 0x7E0F
#define PTABS 0x7E10
#define PTABS_TOG 0x7E11
#define PHIST 0x7E12
#define PHIST_TOG 0x7E13
#define PCUSTOM1 0x7E14
#define PCUSTOM1_TOG 0x7E15
#define PCUSTOM2 0x7E16
#define PCUSTOM2_TOG 0x7E17
#define PCUSTOM3 0x7E18
#define PCUSTOM3_TOG 0x7E19
#define PCUSTOM4 0x7E1A
#define PCUSTOM4_TOG 0x7E1B
#define PCUSTOM5 0x7E1C
#define PCUSTOM5_TOG 0x7E1D
#define INVX 0x7E1E
#define INVY 0x7E1F
#define PMODE_LAST 0x7E20 // used only for determining mouse keys, keep last