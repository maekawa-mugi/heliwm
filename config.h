/*
 * heliwm - Highly Essential Light Itsy-bitsy Window Manager
 *
 * config.h - default configuration
 * Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005 Hidetoshi Ohtomo
 */

#define HOME_CF "~/.heliwmrc"
#define SYSTEM_CF "/usr/lib/X11/heliwm/heliwmrc"

#define DEF_TEXT_COLOR "black"
#define DEF_FRAME_COLOR "gray"
#define DEF_BORDER_COLOR "white"
#define DEF_SHADE0_COLOR "white"
#define DEF_SHADE1_COLOR "DimGray"
#define DEF_BAND_COLOR "white"

#define DEF_FONT "fixed"
#define DEF_CURSOR XC_left_ptr

#define DEF_BORDER_WIDTH 1
#define DEF_FRAME_WIDTH 2
#define DEF_SHADE_WIDTH 1

/* Offsets for text placement */
#define H_OFFSET 4 /* horizontal */
#define V_OFFSET 1 /* vertical */

/* Default boolean options; see heliwm.h for available options. */
#define DEF_OTHER_OPTIONS                                                      \
  (FOCUS_WHEN_RAISED | FOCUS_WHEN_CIRCULATED | FOCUS_WHEN_MAPPED |             \
   RAISE_WHEN_CIRCULATED | SET_ORIGINAL_BORDER_TO_ZERO)

/* Move/resize step sizes for move_or_resize() */
#define DEF_SCALE_NORM 64  /* when no modifier is being pressed */
#define DEF_SCALE_SHIFT 16 /* when shift key is being pressed */
#define DEF_SCALE_CTRL 1   /* when control key is being pressed */
#define DEF_SCALE_MOD1 256 /* when modifier key 1 is being pressed */

#define MAX_KEY_DEFINITIONS 16    /* maximum number of key definitions */
#define MAX_BUTTON_DEFINITIONS 10 /* maximum number of button definitions */
