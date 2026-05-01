SRC += argos_combo.c
SRC += argos_tapdance.c
SRC += argos_pointer.c

# Enable required QMK features if not already enabled
COMBO_ENABLE ?= yes
TAP_DANCE_ENABLE ?= yes

# TODO tapping term per key