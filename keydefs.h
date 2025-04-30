/*
 * heliwm - Highly Essential Light Itsy-bitsy Window Manager
 *
 * keydefs.h - predefined key bind table
 * Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2006 Hidetoshi Ohtomo
 */

/* Number of keys defined below */
static int predefined_keys = 13;

/* Number of buttons defined below */
static int predefined_buttons = 3;

/*
 * Notes:
 *
 * - Because linear search is used to match the key pressed, it is better to
 *   order the entries from most to least frequently used keys.
 *
 * - Use bitwise-or operator `|' to specify more than one modifier key.
 *
 * - "ShiftMask" and "ControlMask" are for shift and control key, respectively,
 *   while "None" is for no modifier.  "Mod1Mask" on some machines is for
 *   alt key.  Use xmodmap(1) to find out more.
 *
 * - OP_* and XK_* are defined in heliwm.h and <X11/keysymdef.h>,
 *   respectively.
 *
 * - If you want to change the number of entries, pay attention to
 *   MAX_KEY_DEFINITIONS and MAX_BUTTON_DEFINITIONS, which are defined in
 *   config.h, and don't forget to adjust defined_keys and defined_buttons
 *   above.
 */

struct keyop keys[MAX_KEY_DEFINITIONS] = {
    /* {keycode (don't touch), modifier(s), keysym, operation code} */

    {0, Mod1Mask, XK_Tab, OP_NEXT},
    {0, Mod1Mask | ShiftMask, XK_Tab, OP_PREV},
    {0, Mod1Mask, XK_F7, OP_MOVE},
    {0, Mod1Mask, XK_F8, OP_RESIZE},
    {0, Mod1Mask, XK_F9, OP_ICONIFY},
    {0, Mod1Mask, XK_F10, OP_MAXIMIZE},
    {0, Mod1Mask, XK_F11, OP_MAXIMIZE_H},
    {0, Mod1Mask, XK_F12, OP_MAXIMIZE_V},
    {0, Mod1Mask | ShiftMask, XK_F1, OP_REORDER},
    {0, Mod1Mask, XK_F2, OP_RAISE},
    {0, Mod1Mask, XK_F3, OP_LOWER},
    {0, Mod1Mask, XK_F4, OP_DELETE},
    {0, ShiftMask | ControlMask, XK_Escape, OP_TERMINATE}};

struct buttonop buttons[MAX_BUTTON_DEFINITIONS] = {
    /* {modifier(s), button number, operation code} */

    {None, Button1, OP_RAISE},
    {None, Button2, OP_LOWER},
    {None, Button3, OP_ICONIFY}};
