/*
 * heliwm - Highly Essential Light Itsy-bitsy Window Manager
 *
 * winops.c - window operation module
 * Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005 Hidetoshi Ohtomo
 */

#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "heliwm.h"

/*
 * update_indicator()
 *
 * Update either coordinates (OP_MOVE) or window size (OP_RESIZE) in the
 * indicator window.
 */

static void update_indicator(int operation, int x, int y) {
  char s[32];
  /*
   * s[] must be big enough to hold two 32-bit integers (in decimal) and
   * extra characters, in order to avoid buffer overflow with sprintf(3).
   */

#ifdef NLS
  XRectangle inkbox;
#endif

  sprintf(s, ((operation == OP_MOVE) ? "%d, %d" : "%d x %d"), x, y);

  XClearWindow(d, indicator);

#ifdef NLS
  XmbTextExtents(fontset, s, strlen(s), NULL, &inkbox);
  XmbDrawString(d, indicator, fontset, text_gc,
                (ind_w - inkbox.width) / 2 - inkbox.x,
                (title_h - inkbox.height) / 2 - inkbox.y, s, strlen(s));
#else
  XDrawString(d, indicator, text_gc,
              (ind_w - XTextWidth(font_info, s, strlen(s))) / 2,
              font_info->ascent + V_OFFSET, s, strlen(s));
#endif
}

/*
 * draw_band()
 *
 * Draw rubber band according to the coordinates and size.  Calling this
 * function twice with same arguments erases band, using XOR.
 */

static void draw_band(int x, int y, int width, int height) {
  XDrawRectangle(d, rootwin, band_gc, x, y, width - 1, height - 1);
  XDrawLine(d, rootwin, band_gc, x + width / 2, y, x + width / 2, y + height);
  XDrawLine(d, rootwin, band_gc, x, y + height / 2, x + width, y + height / 2);
}

/*
 * real_move()
 *
 * Actually move a heliwm_window.
 */

#ifdef FRAME

static void real_move(heliwm_window *hw) {
  XConfigureEvent e;

  XMoveWindow(d, hw->frame, hw->x, hw->y);

#ifdef SHAPE
  if (hw->flags & SHAPED) {
    XMoveWindow(d, hw->child, hw->x + frame_w, hw->y + frame_w * 2 + title_h);
    return;
  }
#endif

  /* Notify the child about the move */
  e.type = ConfigureNotify;
  e.event = hw->child;
  e.window = hw->child;
  e.x = hw->x + frame_w;
  e.y = hw->y + frame_w + title_h;
  e.width = hw->child_w;
  e.height = hw->child_h;
  e.border_width = hw->child_bw;
  e.above = None;
  e.override_redirect = False;
  XSendEvent(d, hw->child, False, StructureNotifyMask, (XEvent *)&e);
}

#else /* FRAME */

#define real_move(hw) XMoveWindow(d, hw->child, hw->x, hw->y)

#endif /* FRAME */

/*
 * adjust()
 *
 * Calculate adjusted size of a window according to its parameters:
 *	*w: desired width of child window; adjusted width is returned
 *	*h: desired height of child window; adjusted height is returned
 *	*sw: step-sized width is returned when non-NULL
 *	*sh: step-sized height is returned when non-NULL
 *	flag: bitwise-or combination of ADJUST_WIDTH and ADJUST_HEIGHT
 */

#define ADJUST_WIDTH 0x1
#define ADJUST_HEIGHT 0x2

static void adjust(heliwm_window *hw, int *w, int *h, int *sw, int *sh,
                   int flag) {
  int i;

  if ((flag & ADJUST_WIDTH) && w) {
    if (*w <= hw->base_w) {
      if ((*w = hw->base_w) == 0)
        (*w)++;
    } else if ((hw->flags & MAX_SIZE_DEFINED) && *w > hw->max_w)
      *w = hw->max_w;

    i = (*w - hw->base_w) / hw->inc_w;
    *w = hw->base_w + hw->inc_w * i;
    if (sw)
      *sw = i;
  }

  if ((flag & ADJUST_HEIGHT) && h) {
    if (*h <= hw->base_h) {
      if ((*h = hw->base_h) == 0)
        (*h)++;
    } else if ((hw->flags & MAX_SIZE_DEFINED) && *h > hw->max_h)
      *h = hw->max_h;

    i = (*h - hw->base_h) / hw->inc_h;
    *h = hw->base_h + hw->inc_h * i;
    if (sh)
      *sh = i;
  }
}

static int grab_input_devices() {
  if (XGrabPointer(d, rootwin, False, ButtonReleaseMask | PointerMotionMask,
                   GrabModeAsync, GrabModeAsync, None, fleur,
                   CurrentTime) != GrabSuccess)
    return -1;

  if (XGrabKeyboard(d, rootwin, True, GrabModeAsync, GrabModeAsync,
                    CurrentTime) != GrabSuccess) {
    XUngrabPointer(d, CurrentTime);
    return -1;
  }

  return 0;
}

static void ungrab_input_devices() {
  XUngrabKeyboard(d, CurrentTime);
  XUngrabPointer(d, CurrentTime);
}

static void set_band_values(heliwm_window *hw, int child_or_icon, int *band_x,
                            int *band_y, int *band_w, int *band_h) {
  if (child_or_icon == ICON) {
    *band_x = hw->icon_x;
    *band_y = hw->icon_y;
    *band_w = hw->icon_w + hw->icon_bw * 2;
    *band_h = hw->icon_h + hw->icon_bw * 2;
    return;
  }

  *band_x = hw->x;
  *band_y = hw->y;
#ifdef FRAME
  *band_w = hw->w;
  *band_h = hw->h + ((hw->flags & SHAPED) ? hw->child_h + hw->child_bw * 2 : 0);
#else
  *band_w = hw->child_w + hw->child_bw * 2;
  *band_h = hw->child_h + hw->child_bw * 2;
#endif
}

/*
 * get_input()
 *
 * Get input from keyboard or pointing device.
 */

#define NO_CHANGE 0
#define SETTLED 1
#define CANCELLED 2
#define KB_INPUT 3 /* operation done by using a keyboard */
#define PD_INPUT 4 /* operation done by using a pointing device */

#ifdef VERBOSE
char *input_type[] = {"NO_CHANGE", "SETTLED", "CANCELLED", "KB_INPUT",
                      "PD_INPUT"};
#endif

static int get_input(int *delta_x, int *delta_y) {
  static int scale = DEF_SCALE_NORM;
  static unsigned int state = None;
  XEvent e;

  *delta_x = *delta_y = 0;

  XNextEvent(d, &e);
#ifdef VERBOSE
  fprintf(ERR, "\n\t%s", events[e.type]);
#endif
  switch (e.type) {
  case ButtonRelease:
    return SETTLED;
  case MotionNotify:
#ifdef VERBOSE
    /*
     * This fills up output buffer upon too many MotionNotify
     * before a settlement.  Be careful when using VERBOSE.
     */
    fprintf(ERR, " (%d, %d)", e.xmotion.x, e.xmotion.y);
#endif
    *delta_x = e.xmotion.x;
    *delta_y = e.xmotion.y;
    return PD_INPUT;
  case KeyPress:
    break;
  default:
    return NO_CHANGE;
  }

  if (e.xkey.state != state) {
    state = e.xkey.state;
    scale = (state & Mod1Mask) ? DEF_SCALE_MOD1 : 0;
    if (state & ControlMask)
      scale += DEF_SCALE_CTRL;
    if (state & ShiftMask)
      scale += DEF_SCALE_SHIFT;
    if (scale == 0)
      scale = DEF_SCALE_NORM;
  }

  switch (XLookupKeysym(&e.xkey, 0)) {
  case XK_Down:
  case XK_j:
  case XK_n:
    *delta_y = scale;
    break;
  case XK_Up:
  case XK_k:
  case XK_p:
    *delta_y = -1 * scale;
    break;
  case XK_Left:
  case XK_h:
  case XK_b:
    *delta_x = -1 * scale;
    break;
  case XK_Right:
  case XK_l:
  case XK_f:
    *delta_x = scale;
    break;
  case XK_Return:
    return SETTLED;
  case XK_Escape:
    return CANCELLED;
  default:
    return NO_CHANGE;
  }

  return KB_INPUT;
}

void move(heliwm_window *hw, int child_or_icon) {
  int i, j, in, ox, oy, px, py, band_x, band_y, band_w, band_h;
  unsigned int mask;
  Window root, w;

  if (grab_input_devices() != 0)
    return;

  set_band_values(hw, child_or_icon, &band_x, &band_y, &band_w, &band_h);

  XGrabServer(d);
  XMapRaised(d, indicator);

  /* from now on, px and py holds the latest pointer coordinates */
  XQueryPointer(d, rootwin, &root, &w, &px, &py, &i, &j, &mask);

  ox = px; /* keep original cursor position for cancellation */
  oy = py;

  while (1) {
    int delta_x, delta_y;

    update_indicator(OP_MOVE, band_x, band_y);
    draw_band(band_x, band_y, band_w, band_h);

    while ((in = get_input(&delta_x, &delta_y)) == NO_CHANGE)
      ;
    draw_band(band_x, band_y, band_w, band_h);
#ifdef VERBOSE
    fprintf(ERR, "\nget_input() = %s (%d)", input_type[in], in);
#endif
    if (in == SETTLED || in == CANCELLED)
      break;
    if (in == PD_INPUT) {
      delta_x -= px;
      delta_y -= py;
    }
#ifdef VERBOSE
    fprintf(ERR, "\npos: %d+%d,%d+%d", px, delta_x, py, delta_y);
#endif
    i = px;
    j = py;
    if (delta_x != 0) {
      if ((px += delta_x) < 0)
        px = 0;
      else if (px >= scr_w)
        px = scr_w - 1;
      band_x += px - i;
    }
    if (delta_y != 0) {
      if ((py += delta_y) < 0)
        py = 0;
      else if (py >= scr_h)
        py = scr_h - 1;
      band_y += py - j;
    }
#ifdef VERBOSE
    fprintf(ERR, " -> %d,%d", px, py);
#endif
    if (in == KB_INPUT)
      XWarpPointer(d, None, rootwin, 0, 0, 0, 0, px, py);
  }

  XUnmapWindow(d, indicator);

  if (in == SETTLED) {
    if (child_or_icon == CHILD) {
      hw->x = band_x;
      hw->y = band_y;
      real_move(hw);
    } else {
      XMoveWindow(d, hw->icon, band_x, band_y);
      hw->icon_x = band_x;
      hw->icon_y = band_y;
    }
  } else
    XWarpPointer(d, None, rootwin, 0, 0, 0, 0, ox, oy);

  XUngrabServer(d);
  ungrab_input_devices();
}

static void resize(heliwm_window *hw, int child_or_icon) {
  int ow, oh, sw, sh, cw, ch, ox, oy, px, py, band_x, band_y, band_w, band_h,
      delta_x, delta_y, in;
  unsigned int mask;
  Window root, win;

  if (grab_input_devices() != 0)
    return;

  set_band_values(hw, child_or_icon, &band_x, &band_y, &band_w, &band_h);

  sw = ((ow = cw = hw->child_w) - hw->base_w) / hw->inc_w;
  sh = ((oh = ch = hw->child_h) - hw->base_h) / hw->inc_h;

  XGrabServer(d);
  XQueryPointer(d, rootwin, &root, &win, &px, &py, &ox, &oy, &mask);
  ox = px; /* keep original cursor position for cancellation */
  oy = py;

  XMapRaised(d, indicator);

  while (1) {
    update_indicator(OP_RESIZE, sw, sh);
    draw_band(band_x, band_y, band_w, band_h);
    while ((in = get_input(&delta_x, &delta_y)) == NO_CHANGE)
      ;
    draw_band(band_x, band_y, band_w, band_h);
    if (in == SETTLED || in == CANCELLED)
      break;
    if (in == PD_INPUT) {
      cw = ow + (delta_x - ox);
      ch = oh + (delta_y - oy);
    } else {
      if (delta_x > 0) {
        if (delta_x < hw->inc_w)
          delta_x = hw->inc_w;
      } else if (delta_x < 0) {
        if (delta_x > hw->inc_w * -1)
          delta_x = hw->inc_w * -1;
      }
      if (delta_y > 0) {
        if (delta_y < hw->inc_h)
          delta_y = hw->inc_h;
      } else if (delta_y < 0) {
        if (delta_y > hw->inc_h * -1)
          delta_y = hw->inc_h * -1;
      }
      cw += delta_x;
      ch += delta_y;
    }

    if (delta_x != 0) {
      if (cw < 0)
        cw = 0;
      adjust(hw, &cw, NULL, &sw, NULL, ADJUST_WIDTH);
      band_w = cw + 2 *
#ifdef FRAME
                        (hw->child_bw + frame_w);
#else
                        hw->child_bw;
#endif
    }
    if (delta_y != 0) {
      if (ch < 0)
        ch = 0;
      adjust(hw, NULL, &ch, NULL, &sh, ADJUST_HEIGHT);
      band_h = ch +
               2 *
#ifdef FRAME
                   (hw->child_bw + frame_w) +
               title_h;
#else
                   hw->child_bw;
#endif
    }
  }

  XUnmapWindow(d, indicator);

  if (in == CANCELLED)
    XWarpPointer(d, None, rootwin, 0, 0, 0, 0, ox, oy);
  else if (cw != ow || ch != oh) {
    hw->child_w = cw;
    hw->child_h = ch;
#ifdef MAXIMIZE
    hw->flags &= ~(MAXIMIZED_HORIZONTALLY | MAXIMIZED_VERTICALLY);
#endif
    XResizeWindow(d, hw->child, hw->child_w, hw->child_h);
#ifdef VERBOSE
    fprintf(ERR, " to %dx%d", hw->child_w, hw->child_h);
#endif
#ifdef FRAME
    hw->w = hw->child_w + (hw->child_bw + frame_w) * 2;
    hw->h = recalculate_heliwm_window_height(hw);
    XResizeWindow(d, hw->frame, hw->w, hw->h);
#endif
  }

  XUngrabServer(d);
  ungrab_input_devices();
}

static void prepare_icon(heliwm_window *hw) {
  unsigned long mask = CWBorderPixel | CWEventMask | CWCursor;
  unsigned int w, h;
  int created = 0;
  XSetWindowAttributes attr;

  attr.border_pixel = bdc;
  attr.event_mask = ButtonPressMask | ExposureMask;
  attr.cursor = cursor;

  if (hw->icon_pm != None) {
    int x, y, depth;
    Window win;

    XGetGeometry(d, hw->icon_pm, &win, &x, &y, &w, &h,
                 (unsigned int *)&hw->icon_bw, (unsigned int *)&depth);
#ifdef VERBOSE
    fprintf(ERR, " - pm: %ux%u+%d+%d-%d/%d on 0x%lx", w, h, x, y, hw->icon_bw,
            depth, win);
#endif

    if (depth != DefaultDepth(d, scr)) {
      attr.background_pixmap =
          XCreatePixmap(d, rootwin, w, h, DefaultDepth(d, scr));
      XCopyPlane(d, hw->icon_pm, attr.background_pixmap, text_gc, 0, 0, w, h, 0,
                 0, 1);
      created++;
    } else
      attr.background_pixmap = hw->icon_pm;

    mask |= CWBackPixmap;
  } else {
    w = H_OFFSET * 2 +
#ifdef NLS
        XmbTextEscapement(fontset, hw->icon_name, strlen(hw->icon_name));
#else
        XTextWidth(font_info, hw->icon_name, strlen(hw->icon_name));
#endif
    h = title_h;
    attr.background_pixel = bgc;
    mask |= CWBackPixel;
    hw->flags |= NEED_TO_DRAW_ICON;
  }

  hw->icon = XCreateWindow(d, rootwin, hw->x, hw->y, w, h, bdw, CopyFromParent,
                           InputOutput, CopyFromParent, mask, &attr);
  hw->icon_w = w;
  hw->icon_h = h;
  hw->icon_bw = bdw;
  hw->flags |= CREATED_ICON;

  if (created)
    XFreePixmap(d, attr.background_pixmap);
#ifdef VERBOSE
  fputs(" Created icon.", ERR);
#endif
}

void iconify(heliwm_window *hw) {
  if (hw->icon == None)
    prepare_icon(hw);

#ifdef FRAME
  XUnmapWindow(d, hw->frame);
  if (hw->flags & SHAPED) {
    XUnmapWindow(d, hw->child);
    hw->flags |= IGNORE_UNMAP;
  }
#else
  XUnmapWindow(d, hw->child);
  hw->flags |= IGNORE_UNMAP;
#endif

  if (!(hw->flags & ICON_POS_DEFINED)) {
    if (hw->x < 0)
      hw->icon_x = 0;
    else
      hw->icon_x = (hw->x >= scr_w) ? scr_w - 1 : hw->x;

    if (hw->y < 0)
      hw->icon_y = 0;
    else
      hw->icon_y = (hw->y >= scr_h) ? scr_h - 1 : hw->y;

    XWarpPointer(d, None, rootwin, 0, 0, 0, 0, hw->icon_x, hw->icon_y);
    if (options & INTERACTIVE_PLACEMENT)
      move(hw, ICON);
    else
      XMoveWindow(d, hw->icon, hw->icon_x, hw->icon_y);
    hw->flags |= ICON_POS_DEFINED;
  }

  XMapRaised(d, hw->icon);
  hw->flags |= ICONIFIED;
}

#ifdef MAXIMIZE

static void maximize(heliwm_window *hw, int ho, int ve) {
  int w, h;

  if (ho == 0 && ve == 0)
    return;

#ifdef FRAME
  w = ho ? (scr_w - 2 * (hw->child_bw + frame_w)) : hw->child_w;
  h = ve ? (scr_h - 2 * (hw->child_bw + frame_w) - title_h) : hw->child_h;
#else
  w = ho ? (scr_w - 2 * hw->child_bw) : hw->child_w;
  h = ve ? (scr_h - 2 * hw->child_bw) : hw->child_h;
#endif

#ifdef VERBOSE
  fprintf(ERR, " goal=%dx%d", w, h);
#endif

  if (ho) {
    adjust(hw, &w, NULL, NULL, NULL, ADJUST_WIDTH);
    hw->flags |= MAXIMIZED_HORIZONTALLY;
    hw->ow = hw->child_w;
    hw->ox = hw->x;
    hw->child_w = w;
    hw->x = 0;
  }

  if (ve) {
    adjust(hw, NULL, &h, NULL, NULL, ADJUST_HEIGHT);
    hw->flags |= MAXIMIZED_VERTICALLY;
    hw->oh = hw->child_h;
    hw->oy = hw->y;
    hw->child_h = h;
    hw->y = 0;
  }

#ifdef VERBOSE
  fprintf(ERR, "->%dx%d", w, h);
#endif

  real_move(hw);
  XResizeWindow(d, hw->child, (unsigned int)w, (unsigned int)h);

#ifdef FRAME
  hw->w = (hw->child_bw + frame_w) * 2 + w;
  hw->h = recalculate_heliwm_window_height(hw);
  XResizeWindow(d, hw->frame, hw->w, hw->h);
#endif
#ifdef VERBOSE
  fputs(" maximized.", ERR);
#endif
}

static void unmaximize(heliwm_window *hw, int ho, int ve) {
#ifdef VERBOSE
  fputs(" unmaximized", ERR);
#endif

  if (ho) {
    hw->flags &= ~MAXIMIZED_HORIZONTALLY;
    hw->x = hw->ox;
    hw->child_w = hw->ow;
  }

  if (ve) {
    hw->flags &= ~MAXIMIZED_VERTICALLY;
    hw->y = hw->oy;
    hw->child_h = hw->oh;
  }

  real_move(hw);
  XResizeWindow(d, hw->child, (unsigned int)hw->child_w,
                (unsigned int)hw->child_h);

#ifdef VERBOSE
  fprintf(ERR, " to %dx%d", hw->child_w, hw->child_h);
#endif

#ifdef FRAME
  hw->w = (hw->child_bw + frame_w) * 2 + hw->child_w;
  hw->h = recalculate_heliwm_window_height(hw);
  XResizeWindow(d, hw->frame, (unsigned int)hw->w, (unsigned int)hw->h);
#endif
}

#endif /* MAXIMIZE */

void exec_operation(Window win, int op_code) {
  Window w;
  heliwm_window *hw;
  int rel = 0;
#ifdef VERBOSE
  static char *rels[] = {"undefined", "child", "icon"};
  static char *ops[] = {"move",
                        "resize",
                        "iconify",
                        "next",
                        "prev",
                        "raise",
                        "lower",
                        "delete",
                        "terminate",
                        "maximize",
                        "info",
                        "exec",
                        "reorder",
                        "maximize horizontally",
                        "maximize_vertically"};
  static char flags[15] = "0123456789abcd";

  fputs(ops[op_code], ERR);
#endif

  switch (op_code) {
#ifdef VERBOSE
  case OP_INFO:
#endif
  case OP_NEXT:
  case OP_PREV:
    break;
  default:
    if (win == None || win == rootwin) {
#ifdef VERBOSE
      fputs(" None/root.", ERR);
#endif
      return;
    }
    rel = find_window(win, &hw);
#ifdef VERBOSE
    fprintf(ERR, " %s", rels[rel]);
#endif
    if (rel == UNDEFINED)
      return;
    break;
  }

  switch (op_code) {
  case OP_MOVE:
    move(hw, rel);
    break;
  case OP_RESIZE:
    if (rel == CHILD)
      resize(hw, CHILD);
    break;
  case OP_DELETE:
    if ((hw->flags & DELETE_WINDOW_IS_SET) && rel == CHILD)
      send_cmessage(hw->child, wm_delete_window);
    break;
  case OP_ICONIFY:
    if (rel == CHILD)
      iconify(hw);
    else if (rel == ICON) {
      XUnmapWindow(d, hw->icon);
#ifdef FRAME
#ifdef SHAPE
      if (hw->flags & SHAPED)
        XMapRaised(d, hw->child);
#endif
      XMapRaised(d, hw->frame);
#else
      XMapRaised(d, hw->child);
#endif
      hw->flags &= ~ICONIFIED;
    }
    if (focus == hw)
      set_focus(focus);
    break;
  case OP_NEXT:
  case OP_PREV:
    if (list == NULL)
      break;
    if (op_code == OP_NEXT) {
      if (pointer == NULL || (pointer = pointer->next) == NULL)
        pointer = list;
    } else if (pointer == NULL || (pointer = pointer->prev) == NULL)
      pointer = last;
    hw = pointer;
    if (op_code == OP_NEXT)
      while (!(pointer->flags & MAPPED)) {
        if ((pointer = pointer->next) == NULL)
          pointer = list;
        if (pointer == hw)
          break;
      }
    else
      while (!(pointer->flags & MAPPED)) {
        if ((pointer = pointer->prev) == NULL)
          pointer = last;
        if (pointer == hw)
          break;
      }
    if (pointer->flags & MAPPED) {
      int x, y;

      if (pointer->flags & ICONIFIED) {
        w = pointer->icon;
        x = pointer->icon_x;
        y = pointer->icon_y;
      } else {
#ifdef FRAME
        w = pointer->frame;
#else
        w = pointer->child;
#endif
        x = pointer->x;
        y = pointer->y;
      }
      XWarpPointer(d, None, rootwin, 0, 0, 0, 0, x, y);
      if ((options & FOCUS_WHEN_CIRCULATED) && focus != pointer)
        set_focus(pointer);
      if (options & RAISE_WHEN_CIRCULATED) {
        hw = pointer;
        goto raise_heliwm_window;
      }
    } else {
      pointer = NULL;
      set_focus(NULL);
    }
    break;

  case OP_RAISE:
  case OP_LOWER:
#ifdef FRAME
    w = (rel == ICON) ? hw->icon : hw->frame;
#else
    w = (rel == ICON) ? hw->icon : hw->child;
#endif
    if (op_code == OP_RAISE) {
    raise_heliwm_window:
      XRaiseWindow(d, w);
#ifdef SHAPE
      if (hw->flags & SHAPED)
        XRaiseWindow(d, hw->child);
#endif
      if ((options & FOCUS_WHEN_RAISED) && focus != hw)
        set_focus(hw);
      break;
    }

    XLowerWindow(d, w);
#ifdef SHAPE
    if (hw->flags & SHAPED)
      XLowerWindow(d, hw->child);
#endif
    break;

#ifdef REORDER
  case OP_REORDER:
    if (hw == list)
      break;
    hw->prev->next = hw->next;
    if (hw == last)
      last = hw->prev;
    else
      hw->next->prev = hw->prev;
    hw->next = list;
    hw->prev = NULL;
    list->prev = hw;
    list = hw;
    break;
#endif

#ifdef MAXIMIZE
  case OP_MAXIMIZE:
    if (rel != CHILD)
      break;
    XGrabServer(d);

    if ((hw->flags & MAXIMIZED_HORIZONTALLY) &&
        (hw->flags & MAXIMIZED_VERTICALLY))
      unmaximize(hw, 1, 1);
    else if (hw->flags & MAXIMIZED_HORIZONTALLY)
      maximize(hw, 0, 1);
    else if (hw->flags & MAXIMIZED_VERTICALLY)
      maximize(hw, 1, 0);
    else
      maximize(hw, 1, 1);

    XUngrabServer(d);
    break;

  case OP_MAXIMIZE_H:
    if (rel != CHILD)
      break;
    XGrabServer(d);

    if (hw->flags & MAXIMIZED_HORIZONTALLY)
      unmaximize(hw, 1, 0);
    else
      maximize(hw, 1, 0);

    XUngrabServer(d);
    break;

  case OP_MAXIMIZE_V:
    if (rel != CHILD)
      break;
    XGrabServer(d);

    if (hw->flags & MAXIMIZED_VERTICALLY)
      unmaximize(hw, 0, 1);
    else
      maximize(hw, 0, 1);

    XUngrabServer(d);
    break;

#endif

#ifdef VERBOSE
  case OP_INFO:
    fputs("\nN2R MxD IcD CrI N2D mpd icd Ign Inp Shp Fcs Del mxh mxv", ERR);
    for (hw = list; hw; hw = hw->next) {
      strcpy(flags, "--------------");
      if (hw->flags & NEED_TO_REDRAW)
        flags[0] = 'N';
      if (hw->flags & MAX_SIZE_DEFINED)
        flags[1] = 'M';
      if (hw->flags & ICON_POS_DEFINED)
        flags[2] = 'I';
      if (hw->flags & CREATED_ICON)
        flags[3] = 'C';
      if (hw->flags & NEED_TO_DRAW_ICON)
        flags[4] = 'N';
      if (hw->flags & MAPPED)
        flags[5] = 'm';
      if (hw->flags & ICONIFIED)
        flags[6] = 'i';
      if (hw->flags & IGNORE_UNMAP)
        flags[7] = 'g';
      if (hw->flags & INPUT)
        flags[8] = 'I';
      if (hw->flags & SHAPED)
        flags[9] = 'S';
      if (hw->flags & TAKE_FOCUS_IS_SET)
        flags[10] = 'F';
      if (hw->flags & DELETE_WINDOW_IS_SET)
        flags[11] = 'D';
#ifdef MAXIMIZE
      if (hw->flags & MAXIMIZED_HORIZONTALLY)
        flags[12] = 'H';
      if (hw->flags & MAXIMIZED_VERTICALLY)
        flags[13] = 'V';
#endif /*M*/
      fprintf(ERR,
              "\n%s %lx\t%lx\t%lx %dx%d %d/%d %dx%d\n"
              "%lx\t%4d %4d %4hd %4hd %hd %s\n"
              "%lx\t%4d %4d %4hd %4hd %hd"
#ifdef FRAME
              " %s\n%lx\t%4d %4d"
#endif /*F*/
              ,
              flags, (unsigned long)hw, hw->icon_pm, hw->cmap, hw->base_w,
              hw->base_h, hw->inc_w, hw->inc_h, hw->max_w, hw->max_h, hw->icon,
              hw->icon_w, hw->icon_h, hw->icon_x, hw->icon_y, hw->icon_bw,
              hw->icon_name, hw->child, hw->child_w, hw->child_h, hw->x, hw->y,
              hw->child_bw
#ifdef FRAME
              ,
              hw->name, hw->frame, hw->w, hw->h
#endif /*F*/
      );
    }
    fprintf(ERR, "\nlist=%lx last=%lx focus=%lx pointer=%lx\n",
            (unsigned long)list, (unsigned long)last, (unsigned long)focus,
            (unsigned long)pointer);
    break;
#endif
  }
}
