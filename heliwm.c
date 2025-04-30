/*
 * heliwm - Highly Essential Light Itsy-bitsy Window Manager
 *
 * heliwm.c - main module
 * Copyright (C) 2000, 2001, 2002, 2003, 2004 Hidetoshi Ohtomo
 */

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef SHAPE
#include <X11/extensions/shape.h>
#endif
#ifdef NLS
#include <X11/Xlocale.h>
#endif

#include "config.h"
#include "heliwm.h"

Display *d;
Window rootwin, indicator;

int scr;                         /* screen number */
int scr_w, scr_h;                /* screen width and height */
int ind_w;                       /* width of indicator window */
int title_h;                     /* height of title bar */
int bdw = DEF_BORDER_WIDTH;      /* border width for icons & indicator window */
int defined_keys = 0;            /* number of defined keys */
int defined_buttons = 0;         /* number of defined buttons */
int options = DEF_OTHER_OPTIONS; /* other options */

#ifdef FRAME
int shade_w = DEF_SHADE_WIDTH; /* width of shade */
int frame_w;                   /* frame width */
GC frame0_gc, frame1_gc;
#endif

GC text_gc, band_gc;
unsigned long bgc, bdc; /* background and border colors */

#ifdef NLS
XFontSet fontset; /* font for title of windows and icons */
XFontSetExtents *extents;
#else
XFontStruct *font_info;
#endif

Cursor cursor, fleur;     /* normal and move/resize operation cursors */
Colormap cmap;            /* default colormap */
static Colormap cur_cmap; /* current colormap */
static Time ev_time;

Atom wm_delete_window;
static Atom wm_protocols, wm_take_focus;

heliwm_window *list = NULL;    /* top of child management list */
heliwm_window *last = NULL;    /* last entry of the list */
heliwm_window *focus = NULL;   /* entry with focus */
heliwm_window *pointer = NULL; /* entry with circulation pointer */

static char no_name[] = "No name"; /* default window title */

#ifdef VERBOSE

char *events[] = {"",    "",    "KPr", "KRe", "BPr", "BRe", "Mot", "", "",
                  "",    "",    "",    "Exp", "",    "NEx", "",    "", "Des",
                  "Ump", "Map", "MpR", "Rep", "Cfg", "CfR", "",    "", "",
                  "",    "Pro", "",    "",    "",    "Cmp", "",    ""};
static char *grav[] = {"FU", "NW", "N", "NE", "W", "C",
                       "E",  "SW", "S", "SE", "St"};

#endif

#ifdef FRAME

int recalculate_heliwm_window_height(heliwm_window *hw) {
  return title_h + frame_w * 2 +
         ((hw->flags & SHAPED) ? 0 : (hw->child_h + hw->child_bw * 2));
}

#endif /* FRAME */

/*
 * get_wm_name()
 *
 * Get the name of a window or an icon.
 */

static char *get_wm_name(Window w, int child_or_icon) {
  XTextProperty prop;

  if (child_or_icon == ICON) {
    if (XGetWMIconName(d, w, &prop) == 0)
      return NULL;
  } else if (XGetWMName(d, w, &prop) == 0)
    return NULL;

#ifdef VERBOSE
  fprintf(ERR, " %lu/%dx%lu <%s>", prop.encoding, prop.format, prop.nitems,
          prop.value);
#endif
  return (char *)prop.value;
}

/*
 * get_wm_hints()
 *
 * Call XGetWMHints(3X) on a window, get attributes and return its initial
 * state, which is one of the following: WithdrawnState, NormalState,
 * IconicState.
 */

static int get_wm_hints(Window win, heliwm_window *hw) {
  XWMHints *hints;
  int ini_state = NormalState;

  if ((hints = XGetWMHints(d, win)) == NULL)
    return NormalState;

#ifdef VERBOSE
  if (hints->flags & InputHint)
    fprintf(ERR, " in=%c", (hints->input == True) ? 'T' : 'F');
  if (hints->flags & StateHint) {
    char *states[] = {"Withdrawn", "Normal", "", "Iconic"};
    fprintf(ERR, " st=%s", states[hints->initial_state]);
  }
  if (hints->flags & IconWindowHint)
    fprintf(ERR, " icon=%lx", hints->icon_window);
  if (hints->flags & IconPixmapHint)
    fprintf(ERR, " ipm=%lx", hints->icon_pixmap);
  if (hints->flags & IconMaskHint)
    fprintf(ERR, " imsk=%lx", hints->icon_mask);
#endif

  if ((hints->flags & InputHint) && hints->input == True)
    hw->flags |= INPUT;

  if (hw->icon == None) {
    if (hints->flags & IconWindowHint) {
      Window root;
      unsigned int depth;

      hw->icon = hints->icon_window;
      XSelectInput(d, hw->icon, ButtonPressMask);
      XDefineCursor(d, hw->icon, cursor);
      XGetGeometry(d, hw->icon, &root, (int *)&hw->icon_x, (int *)&hw->icon_y,
                   (unsigned int *)&hw->icon_w, (unsigned int *)&hw->icon_h,
                   (unsigned int *)&hw->icon_bw, &depth);
    } else if (hints->flags & IconPixmapHint)
      hw->icon_pm = hints->icon_pixmap;
  }

  if (hints->flags & StateHint)
    ini_state = hints->initial_state;

  XFree(hints);
  return ini_state;
}

/*
 * get_wm_normal_hints()
 *
 * Call XGetWMNormalHints(3X) on a window, get size hints, set *userpos to True
 * if it is non-NULL and USPosition is set, and finally return win_gravity if
 * compiled with FRAME option.
 */

static
#ifdef FRAME
    int
#else
    void
#endif
    get_wm_normal_hints(Window win, heliwm_window *hw, Bool *userpos) {
  XSizeHints hints;
  long supplied;
  int min_w, min_h;

  if (XGetWMNormalHints(d, win, &hints, &supplied) == 0)
    hints.flags = 0;

#ifdef VERBOSE
  if (hints.flags & PWinGravity)
    fprintf(ERR, " gv=%s", grav[hints.win_gravity]);
  if (hints.flags & PMinSize)
    fprintf(ERR, " mi=%dx%d", hints.min_width, hints.min_height);
  if (hints.flags & PMaxSize)
    fprintf(ERR, " mx=%dx%d", hints.max_width, hints.max_height);
  if (hints.flags & PResizeInc)
    fprintf(ERR, " in=%d/%d", hints.width_inc, hints.height_inc);
  if (hints.flags & PBaseSize)
    fprintf(ERR, " bs=%dx%d", hints.base_width, hints.base_height);
  {
    char ignored[32] = {'\0'};
    if (hints.flags & USSize)
      strcat(ignored, " USSize");
    if (hints.flags & PPosition)
      strcat(ignored, " PPosition");
    if (hints.flags & PSize)
      strcat(ignored, " PSize");
    if (hints.flags & PAspect) {
      strcat(ignored, " PAspect");
      fprintf(ERR, "\n PAspect IS DEFINED: amin=%d/%d amax=%d/%d",
              hints.min_aspect.x, hints.min_aspect.y, hints.max_aspect.x,
              hints.max_aspect.y);
    }
    if (ignored[0] != '\0')
      fprintf(ERR, " -%s", ignored);
  }
#endif

  if (userpos)
    *userpos = (hints.flags & USPosition) ? True : False;

  if (hints.flags & PResizeInc) {
    hw->inc_w = hints.width_inc;
    hw->inc_h = hints.height_inc;
  } else
    hw->inc_w = hw->inc_h = 1;

  if (hints.flags & PMinSize) {
    min_w = hints.min_width;
    min_h = hints.min_height;
  } else
    min_w = min_h = 0;

  if (hints.flags & PBaseSize) {
    hw->base_w = hints.base_width;
    hw->base_h = hints.base_height;
  } else {
    hw->base_w = min_w;
    hw->base_h = min_h;
  }

  if ((hints.flags & PMaxSize) && hints.max_width >= hw->base_w &&
      hints.max_height >= hw->base_h) {
    hw->max_w = hints.max_width;
    hw->max_h = hints.max_height;
    hw->flags |= MAX_SIZE_DEFINED;
  }
#ifdef VERBOSE
  else
    hw->max_w = hw->max_h = 0;
#endif

#ifdef FRAME
  return ((hints.flags & PWinGravity) ? hints.win_gravity : NorthWestGravity);
#endif
}

/*
 * get_wm_protocols()
 *
 * Get window manager protocols of a window.
 */

static void get_wm_protocols(Window w, heliwm_window *hw) {
  Atom *protocols;
  int i, n;

  if (XGetWMProtocols(d, w, &protocols, &n) == 0)
    return;

  for (i = 0; i < n; i++) {
#ifdef VERBOSE
    fprintf(ERR, " %lu/", protocols[i]);
#endif

    if (protocols[i] == wm_take_focus) {
#ifdef VERBOSE
      fputs("TAKE_FOCUS", ERR);
#endif
      hw->flags |= TAKE_FOCUS_IS_SET;
    } else if (protocols[i] == wm_delete_window) {
#ifdef VERBOSE
      fputs("DELETE_WINDOW", ERR);
#endif
      hw->flags |= DELETE_WINDOW_IS_SET;
    }
  }

  XFree(protocols);
}

/*
 * send_cmessage()
 *
 * Send a client message to a window.
 */

void send_cmessage(Window w, Atom atom) {
  XClientMessageEvent ev;

  ev.type = ClientMessage;
  ev.window = w;
  ev.message_type = wm_protocols;
  ev.format = 32;
  ev.data.l[0] = atom;

  if (atom == wm_take_focus) {
#ifdef VERBOSE
    fprintf(ERR, " XSendEvent time=%lu", ev_time);
#endif
    ev.data.l[1] = ev_time;
  }

  XSendEvent(d, w, False, 0, (XEvent *)&ev);
}

/*
 * set_focus()
 *
 * Set the pointer focus to a window.
 */

void set_focus(heliwm_window *new_focus) {
  Colormap new;
  Window w = None;

  if ((focus = new_focus) == NULL) {
    new = cmap;
    w = PointerRoot;
  } else {
    if (focus->flags & ICONIFIED) {
      new = cmap;
      w = focus->icon;
    } else {
      new = focus->cmap;
      if (focus->flags & TAKE_FOCUS_IS_SET)
        send_cmessage(focus->child, wm_take_focus);
      if (focus->flags & INPUT)
        w = focus->child;
#ifdef FRAME
      if ((options & REDRAW_ACTIVE_WINDOW_ONLY) &&
          (focus->flags & NEED_TO_REDRAW))
        XClearArea(d, focus->frame, 0, 0, 1, 1, True);
#endif
    }

    if (options & WARP_POINTER_WHEN_FOCUSED) {
      int x, y;

      if (focus->flags & ICONIFIED)
        x = focus->icon_x, y = focus->icon_y;
      else
        x = focus->x, y = focus->y;
      XWarpPointer(d, None, rootwin, 0, 0, 0, 0, x, y);
      pointer = focus;
    }
  }

  if (w != None)
    XSetInputFocus(d, w, RevertToParent, ev_time);

  if (cur_cmap != new)
    XInstallColormap(d, cur_cmap = new);
}

/*
 * add_child()
 *
 * Add a child of the root window to the management list if necessary, map it
 * when needed, and return a pointer to the new entry.  "map_request" is "True"
 * when called from MapRequest event, "False" when called from initialization
 * routine.
 */

static heliwm_window *add_child(Window win, Bool map_request) {
  heliwm_window *hw;
  XWindowAttributes attr;
#ifdef FRAME
  XSetWindowAttributes sattr;
  int win_gravity;
#endif
  Window w;
  Status s;
  Bool pos_set;
  int ini_state, child_or_icon;

  s = XGetWindowAttributes(d, win, &attr);

#ifdef VERBOSE
  fprintf(ERR, "\n%lx I%c ovrd=%c bg=%s wg=%s d=%x %lx %d %dx%d+%d+%d", win,
          ((attr.class == InputOutput) ? 'O' : 'n'),
          ((attr.override_redirect == True) ? 'T' : 'F'),
          grav[attr.bit_gravity], grav[attr.win_gravity], attr.depth,
          attr.colormap, attr.border_width, attr.width, attr.height, attr.x,
          attr.y);
#endif

  if (s == BadWindow || s == BadDrawable || attr.override_redirect == True ||
      attr.class == InputOnly ||
      (map_request == False && attr.map_state == IsUnmapped))
    return NULL;

  if ((hw = malloc(sizeof(heliwm_window))) == NULL)
    return NULL; /* I hope that this does never occur to you. */

  if ((hw->next = list))
    list->prev = hw;
  else
    last = hw;

  list = hw;
  hw->prev = NULL;
  hw->child = win;
  hw->icon = None;
  hw->icon_pm = None;
  hw->flags = MAPPED;
  hw->cmap = attr.colormap;
  hw->x = attr.x;
  hw->y = attr.y;
#ifdef VERBOSE
  hw->icon_x = hw->icon_y = hw->icon_w = hw->icon_h = hw->icon_bw = 0;
#endif

#ifdef FRAME
#ifdef VERBOSE
  fputc('\n', ERR);
#endif
  if ((hw->name = get_wm_name(win, CHILD)) == NULL)
    hw->name = no_name;
#endif

#ifdef VERBOSE
  fputc('\n', ERR);
#endif

  if ((hw->icon_name = get_wm_name(win, ICON)) == NULL)
    hw->icon_name =
#ifdef FRAME
        hw->name;
#else
        no_name;
#endif
#ifdef VERBOSE
  fputc('\n', ERR);
#endif

  get_wm_protocols(win, hw);
#ifdef VERBOSE
  fputc('\n', ERR);
#endif

  ini_state = get_wm_hints(win, hw);
#ifdef VERBOSE
  fputc('\n', ERR);
#endif

#ifdef FRAME
  win_gravity =
#endif
      get_wm_normal_hints(win, hw, &pos_set);

#ifdef FRAME
  if (win_gravity > NorthWestGravity) {
    char m[9][3] = {{1, 0, 0}, {2, 0, 0}, {0, 1, 1}, {1, 1, 1}, {2, 1, 1},
                    {0, 2, 2}, {1, 2, 2}, {2, 2, 2}, {1, 1, 2}};

    win_gravity -= 2;
    hw->x -= frame_w * m[win_gravity][0];
    hw->y -= frame_w * m[win_gravity][1] + (title_h * m[win_gravity][2]) / 2;
  }
#endif

  if (XGetTransientForHint(d, win, &w) != 0) {
#ifdef VERBOSE
    fputs("\n TransientFor", ERR);
#endif
    pos_set = True;
  }

#ifdef SHAPE
  {
    int b_shaped, c_shaped, xbs, ybs, xcs, ycs;
    unsigned int wbs, hbs, wcs, hcs;

    XShapeQueryExtents(d, win, &b_shaped, &xbs, &ybs, &wbs, &hbs, &c_shaped,
                       &xcs, &ycs, &wcs, &hcs);

    if (b_shaped) {
#ifdef VERBOSE
      fprintf(ERR,
              "\n b_shaped=%d x/y/w/h=%d/%d/%u/%u "
              "c_shaped=%d %d/%d/%u/%u ",
              b_shaped, xbs, ybs, wbs, hbs, c_shaped, xcs, ycs, wcs, hcs);
#endif
      hw->flags |= SHAPED;
    }
  }
#endif

  hw->child_w = attr.width;
  hw->child_h = attr.height;

  if ((options & SET_ORIGINAL_BORDER_TO_ZERO) && attr.border_width) {
    hw->child_bw = 0;
    XSetWindowBorderWidth(d, win, 0);
  } else
    hw->child_bw = attr.border_width;

  XSelectInput(d, win,
#ifdef FRAME
#ifdef SHAPE
               ((hw->flags & SHAPED) ? StructureNotifyMask : 0) |
#endif
#else
               StructureNotifyMask |
#endif /* FRAME */
                   PropertyChangeMask | ColormapChangeMask);

  XAddToSaveSet(d, hw->child);

#ifdef FRAME
  hw->w = hw->child_w + (hw->child_bw + frame_w) * 2;
  hw->h = recalculate_heliwm_window_height(hw);

  sattr.background_pixel = bgc;
  sattr.border_pixel = bdc;
  sattr.cursor = cursor;
  sattr.event_mask =
      ButtonPressMask | ExposureMask |
#ifdef SHAPE
      ((hw->flags & SHAPED)
           ? 0
           : (SubstructureNotifyMask | SubstructureRedirectMask));
#else
      SubstructureNotifyMask | SubstructureRedirectMask;
#endif

  hw->frame = XCreateWindow(
      d, rootwin, hw->x, hw->y, (unsigned int)hw->w, (unsigned int)hw->h, 0,
      CopyFromParent, InputOutput, CopyFromParent,
      CWBackPixel | CWBorderPixel | CWEventMask | CWCursor, &sattr);

#ifdef SHAPE
  if (hw->flags & SHAPED)
    XMoveWindow(d, win, hw->x + frame_w, hw->y + frame_w * 2 + title_h);
  else
#endif
    XReparentWindow(d, win, hw->frame, frame_w, frame_w + title_h);

#endif /* FRAME */

  if (ini_state == IconicState) {
    if (!(hw->flags & ICON_POS_DEFINED)) {
      hw->icon_x = hw->x;
      hw->icon_y = hw->y;
      hw->flags |= ICON_POS_DEFINED;
    }
    iconify(hw);
    child_or_icon = ICON;
    w = hw->icon;
  } else {
    child_or_icon = CHILD;
#ifdef FRAME
    w = hw->frame;
#else
    w = hw->child;
#endif
  }

  if (pos_set == False && map_request == True &&
      (options & INTERACTIVE_PLACEMENT))
    move(hw, child_or_icon);

#ifdef FRAME
#ifdef SHAPE
  if (!(hw->flags & SHAPED) || !(hw->flags & ICONIFIED))
#endif
    XMapRaised(d, win);
#endif /* FRAME */
  XMapRaised(d, w);
  return hw;
}

/*
 * find_window()
 *
 * Look for the specified window in the list and return one of the following:
 *	CHILD when the window is a heliwm child or a heliwm frame window;
 *	ICON when it is an associated icon window;
 *	UNDEFINED when it is not in the list.
 */

int find_window(register Window w, register heliwm_window **hw) {
  if (focus) {
#ifdef FRAME
    if (w == focus->child || w == focus->frame)
#else
    if (w == focus->child)
#endif
    {
      *hw = focus;
      return CHILD;
    } else if (w == focus->icon) {
      *hw = focus;
      return ICON;
    }
  }

  if (pointer) {
#ifdef FRAME
    if (w == pointer->child || w == pointer->frame)
#else
    if (w == pointer->child)
#endif
    {
      *hw = pointer;
      return CHILD;
    } else if (w == pointer->icon) {
      *hw = pointer;
      return ICON;
    }
  }

  for (*hw = list; *hw; *hw = (*hw)->next)
#ifdef FRAME
    if (w == (*hw)->child || w == (*hw)->frame)
#else
    if (w == (*hw)->child)
#endif
      return CHILD;
    else if (w == (*hw)->icon)
      return ICON;

  return UNDEFINED;
}

/*
 * configure()
 *
 * Process XConfigureRequestEvent(3X).
 */

static void configure(XConfigureRequestEvent *e) {
  XWindowChanges wc;
  heliwm_window *hw;
  unsigned int mask = 0;

#ifdef VERBOSE
  if (e->value_mask & CWSibling) {
    fputs(" sibling=", ERR);
    if (e->above == Above)
      fputs("Above", ERR);
    else
      fprintf(ERR, "%lx", e->above);
  }
  if (e->value_mask & CWStackMode) {
    char *mode[] = {"Above", "Below", "TopIf", "BottomIf", "Opposite"};
    fprintf(ERR, " stackmode=%s", mode[e->detail]);
  }
  if (e->value_mask & CWX)
    fprintf(ERR, " x=%d", e->x);
  if (e->value_mask & CWY)
    fprintf(ERR, " y=%d", e->y);
  if (e->value_mask & CWWidth)
    fprintf(ERR, " w=%d", e->width);
  if (e->value_mask & CWHeight)
    fprintf(ERR, " h=%d", e->height);
  if (e->value_mask & CWBorderWidth)
    fprintf(ERR, " bw=%d", e->border_width);
#endif

  if (e->value_mask & CWStackMode) {
    mask |= CWStackMode;
    wc.stack_mode = e->detail;
  }

  if (find_window(e->window, &hw) != CHILD) {
    wc.x = e->x;
    wc.y = e->y;
    wc.width = e->width;
    wc.height = e->height;
    wc.border_width = e->border_width;
    wc.sibling = e->above;
    XConfigureWindow(d, e->window, (unsigned int)e->value_mask, &wc);
    return;
  }

  if (e->value_mask & CWSibling) {
#ifdef FRAME
    heliwm_window *sibling;

    wc.sibling = (find_window(e->above, &sibling) == UNDEFINED)
                     ? e->above
                     : sibling->frame;
#else
    wc.sibling = e->above;
#endif
    mask |= CWSibling;
  }

  if (e->value_mask & CWBorderWidth) {
    if (hw->child_bw != e->border_width) {
      hw->child_bw = e->border_width;
#ifdef FRAME
      XSetWindowBorderWidth(d, hw->child, e->border_width);
#else
      wc.border_width = e->border_width;
      mask |= CWBorderWidth;
#endif
    }
    e->x -= e->border_width;
    e->y -= e->border_width;
  }

  if ((e->value_mask & CWX) && hw->x != e->x) {
    mask |= CWX;
    wc.x = e->x;
    hw->x = e->x;
  }

  if ((e->value_mask & CWY) && hw->y != e->y) {
    mask |= CWY;
    wc.y = e->y;
    hw->y = e->y;
  }

  if ((e->value_mask & CWWidth) && hw->child_w != e->width) {
    mask |= CWWidth;
    hw->child_w = e->width;
#ifdef FRAME
    wc.width = hw->w = hw->child_w + (hw->child_bw + frame_w) * 2;
#else
    wc.width = e->width;
#endif
  }

  if ((e->value_mask & CWHeight) && hw->child_h != e->height) {
    mask |= CWHeight;
    hw->child_h = e->height;
#ifdef FRAME
    wc.height = hw->h = recalculate_heliwm_window_height(hw);
#else
    wc.height = e->height;
#endif /* FRAME */
  }

#ifdef FRAME

  if (mask & (CWWidth | CWHeight)) {
#ifdef VERBOSE
    fputs(" XResizeWindow", ERR);
#endif
    XResizeWindow(d, hw->child, hw->child_w, hw->child_h);
#ifdef MAXIMIZE
    hw->flags &= ~(MAXIMIZED_HORIZONTALLY | MAXIMIZED_VERTICALLY);
#endif
  }
  if (mask)
#ifdef VERBOSE
    fputs(" XConfigureWindow", ERR),
#endif
        XConfigureWindow(d, hw->frame, mask, &wc);

#else /* FRAME */

  XConfigureWindow(d, hw->child, mask, &wc);

#endif /* FRAME */
}

/*
 * destroy()
 *
 * Destroy frame window and icon window of a heliwm_window.
 */

static void destroy(heliwm_window *hw) {
#ifdef FRAME
  XDestroyWindow(d, hw->frame);
#endif
  if (hw->icon != None && (hw->flags & CREATED_ICON))
    XDestroyWindow(d, hw->icon);
}

/*
 * terminate()
 *
 * Put back windows to the root window and finish up.
 */

static void terminate() {
  heliwm_window *hw;

  XGrabServer(d);
  set_focus(NULL);

  for (hw = list; hw; hw = hw->next) {
#ifdef FRAME
    XReparentWindow(d, hw->child, rootwin, hw->x, hw->y);
#endif
    XRemoveFromSaveSet(d, hw->child);
    if (hw->flags & ICONIFIED) {
      XUnmapWindow(d, hw->icon);
      XMapWindow(d, hw->child);
    }
    destroy(hw);
  }

  XUngrabServer(d);
  XCloseDisplay(d);
#ifdef VERBOSE
  fputs("terminated.\n", ERR),
#endif
      exit(0);
}

/*
 * init()
 *
 * Initialize global variables and do some setups.  It is called only once from
 * main(), with "display" and "rc" set from argv[].
 */

static int init(char *display, char *rc) {
  Window root, w, *children;
  int i;
  unsigned int nchildren;

#ifdef NLS
  if (setlocale(LC_ALL, "") == NULL) {
    fputs("heliwm: can't set locale.\n", stderr);
    return 1;
  }
#endif

  if ((d = XOpenDisplay(display)) == NULL) {
    fputs("heliwm: can't open display.\n", stderr);
    return 1;
  }

#ifdef NLS
  if (XSupportsLocale() == False) {
    fputs("heliwm: X doesn't support locale.\n", stderr);
    return 1;
  }
#endif
  scr = DefaultScreen(d);
  rootwin = RootWindow(d, scr);
  cur_cmap = cmap = DefaultColormap(d, scr);

  if (configure_heliwm(rc) != 0) {
    XCloseDisplay(d);
    return 1;
  }

  signal(SIGINT, terminate);
  signal(SIGHUP, terminate);
  signal(SIGQUIT, terminate);
  signal(SIGTERM, terminate);

  {
    char *names[] = {"WM_PROTOCOLS", "WM_TAKE_FOCUS", "WM_DELETE_WINDOW"};
    Atom atoms[3];

    XInternAtoms(d, names, 3, False, atoms);
    wm_protocols = atoms[0];
    wm_take_focus = atoms[1];
    wm_delete_window = atoms[2];
  }

  scr_w = DisplayWidth(d, scr);
  scr_h = DisplayHeight(d, scr);
  fleur = XCreateFontCursor(d, XC_fleur);

#ifdef NLS
  ind_w = H_OFFSET * 2 + XmbTextEscapement(fontset, "8888 x 8888", 11);
#else
  ind_w = H_OFFSET * 2 + XTextWidth(font_info, "8888 x 8888", 11);
#endif

  indicator =
      XCreateSimpleWindow(d, rootwin, (scr_w - ind_w) / 2,
                          (scr_h - title_h) / 2, ind_w, title_h, bdw, bdc, bgc);

  XGrabServer(d);
  XSelectInput(d, rootwin,
               KeyPressMask | ButtonPressMask | SubstructureRedirectMask);

  if (XQueryTree(d, rootwin, &root, &w, &children, &nchildren) != 0) {
    int j;
    XWMHints *hints;

    /* look for and mark icon windows in the list */
    for (i = 0; i < nchildren; i++) {
      if (children[i] == None)
        continue;
      hints = XGetWMHints(d, children[i]);
      if (hints == NULL)
        continue;
      if (hints->flags & IconWindowHint)
        for (j = 0; j < nchildren; j++)
          if (children[j] == hints->icon_window) {
            children[j] = None;
            break;
          }
      XFree(hints);
    }

    for (i = 0; i < nchildren; i++)
      if (children[i] != None)
        add_child(children[i], False);
  }

  XUngrabServer(d);
  if (children)
    XFree(children);

  for (i = 0; i < defined_keys; i++)
    XGrabKey(d, keys[i].keycode, keys[i].state, rootwin, True, GrabModeAsync,
             GrabModeAsync);

  return 0;
}

/*
 * version()
 *
 * Print out version number with compile time options.
 */

static void version(FILE *out) {
  fprintf(out, "heliwm " VERSION
#ifndef FRAME
               " \"Alpha Particle\""
#else
               " +FRAME"
#endif
#ifdef VERBOSE
               " +VERBOSE"
#endif
#ifdef RCPARSER
               " +RCPARSER"
#endif
#ifdef SHAPE
               " +SHAPE"
#endif
#ifdef NLS
               " +NLS"
#endif
#ifdef MAXIMIZE
               " +MAXIMIZE"
#endif
#ifdef REORDER
               " +REORDER"
#endif
               "\n");
}

/*
 * process_events()
 *
 * Process X events.
 */

static void process_events() {
  int i;
  heliwm_window *hw;
  XEvent ev;
#ifdef NLS
  XRectangle inkbox;
#endif
#ifdef FRAME
  XSegment seg[6];
#endif
#ifdef VERBOSE
  char *anames[] = {"", "",   "", "",   "",   "", "", "",   "", "", "", "",
                    "", "",   "", "",   "",   "", "", "",   "", "", "", "",
                    "", "",   "", "",   "",   "", "", "",   "", "", "", "hi",
                    "", "ic", "", "nm", "nh", "", "", "",   "", "", "", "",
                    "", "",   "", "",   "",   "", "", "",   "", "", "", "",
                    "", "",   "", "",   "",   "", "", "cl", ""};
#endif

  while (1) {
    XNextEvent(d, &ev);
    ev_time = CurrentTime;
#ifdef VERBOSE
    if (ev.type == KeyRelease)
      continue;
    fprintf(ERR, "\n%lx\t%s ", ev.xany.window, events[ev.type]);
#endif

    switch (ev.type) {
    case KeyPress:
#ifdef VERBOSE
      fprintf(ERR, "%lx/%lx %u+%u ", ev.xkey.root, ev.xkey.subwindow,
              ev.xkey.state, ev.xkey.keycode);
#endif
      for (i = 0; i < defined_keys; i++)
        if (keys[i].keycode == ev.xkey.keycode &&
            keys[i].state == ev.xkey.state)
          break;
      if (i == defined_keys)
        continue;
      if (keys[i].op == OP_TERMINATE)
        terminate();
      ev_time = ev.xkey.time;
      exec_operation(ev.xkey.subwindow, keys[i].op);
      continue;
    case ButtonPress:
#ifdef VERBOSE
      fprintf(ERR, "%u+%u ", ev.xbutton.state, ev.xbutton.button);
#endif
      for (i = 0; i < defined_buttons; i++)
        if (buttons[i].button == ev.xbutton.button &&
            buttons[i].state == ev.xbutton.state)
          break;

      if (i == defined_buttons) {
        /* Clicking the root window sets the focus to
         * PointerRoot. */
        if (ev.xbutton.window == rootwin && focus != NULL)
          set_focus(NULL);
        continue;
      }

      if (buttons[i].op == OP_TERMINATE)
        terminate();
      ev_time = ev.xbutton.time;
      exec_operation(ev.xbutton.window, buttons[i].op);
      continue;
    case Expose:
#ifdef VERBOSE
      fprintf(ERR, "%d %dx%d+%d+%d", ev.xexpose.count, ev.xexpose.width,
              ev.xexpose.height, ev.xexpose.x, ev.xexpose.y);
#endif
      if (ev.xexpose.count != 0)
        continue;

      if ((i = find_window(ev.xexpose.window, &hw)) == ICON &&
          (hw->flags & NEED_TO_DRAW_ICON)) {
#ifdef NLS
        XmbTextExtents(fontset, hw->icon_name, strlen(hw->icon_name), NULL,
                       &inkbox);
        XmbDrawString(d, hw->icon, fontset, text_gc, H_OFFSET,
                      (title_h - inkbox.height) / 2 - inkbox.y, hw->icon_name,
                      strlen(hw->icon_name));
#else
        XDrawString(d, hw->icon, text_gc, H_OFFSET,
                    font_info->ascent + V_OFFSET, hw->icon_name,
                    strlen(hw->icon_name));
#endif
        continue;
      }
#ifdef FRAME
      if (i != CHILD)
        continue;

      if (options & REDRAW_ACTIVE_WINDOW_ONLY) {
        if (focus && hw != focus) {
          hw->flags |= NEED_TO_REDRAW;
          continue;
        }
        hw->flags &= ~NEED_TO_REDRAW;
      }

      seg[0].x1 = 0;
      seg[0].y1 = hw->h - 1;
      seg[0].x2 = seg[1].x1 = 0;
      seg[0].y2 = seg[1].y1 = 0;
      seg[1].x2 = hw->w - 1;
      seg[1].y2 = 0;
      seg[2].x1 = frame_w - 1;
      seg[2].y1 = hw->h - frame_w;
      seg[2].x2 = seg[3].x1 = seg[3].x2 = hw->w - frame_w;
      seg[2].y2 = seg[3].y1 = seg[2].y1;
      seg[3].y2 = frame_w - 1;
      seg[4].x1 = seg[4].x2 = seg[5].x1 = frame_w;
      seg[4].y2 = seg[5].y1 = seg[5].y2 = frame_w;
      seg[4].y1 = seg[4].y2 + title_h - 1;
      seg[5].x2 = seg[2].x2 - 1;
      XDrawSegments(d, hw->frame, frame0_gc, seg, 6);

      seg[0].x2 = seg[1].x1 = seg[1].x2;
      seg[0].y2 = seg[1].y1 = seg[0].y1;
      seg[2].x2 = seg[3].x1 = seg[2].y2 = seg[3].y1 = seg[2].x1;
      seg[4].x2 = seg[5].x1 = seg[5].x2;
      seg[4].y2 = seg[5].y1 = seg[4].y1;
      XDrawSegments(d, hw->frame, frame1_gc, seg, 6);

#ifdef NLS
      XmbTextExtents(fontset, hw->name, strlen(hw->name), NULL, &inkbox);
      XmbDrawString(d, hw->frame, fontset, text_gc,
                    (hw->w - inkbox.width) / 2 - inkbox.x,
                    (title_h - inkbox.height) / 2 - inkbox.y + frame_w,
                    hw->name, strlen(hw->name));
#ifdef VERBOSE
      fprintf(ERR, " %hux%hu+%hd+%hd", inkbox.width, inkbox.height, inkbox.x,
              inkbox.y);
#endif
#else  /* NLS */
      XDrawString(
          d, hw->frame, text_gc,
          (hw->w - XTextWidth(font_info, hw->name, strlen(hw->name))) / 2,
          frame_w + font_info->ascent + V_OFFSET, hw->name, strlen(hw->name));
#endif /* NLS */
#endif /* FRAME */
      continue;
    case DestroyNotify:
      if (find_window(ev.xdestroywindow.window, &hw) != CHILD)
        continue;
      if (pointer == hw)
        pointer = NULL;
      if (focus == hw)
        set_focus(NULL);
      destroy(hw);
      if (hw == list)
        list = hw->next;
      else
        hw->prev->next = hw->next;
      if (hw == last)
        last = hw->prev;
      else
        hw->next->prev = hw->prev;
      free(hw);
      continue;
    case UnmapNotify:
#ifdef VERBOSE
      fprintf(ERR, "%lx", ev.xunmap.window);
#endif
      if (find_window(ev.xunmap.window, &hw) == UNDEFINED)
        continue;
      if (hw->flags & IGNORE_UNMAP) {
        hw->flags &= ~IGNORE_UNMAP;
        continue;
      }
#ifdef FRAME
      XUnmapWindow(d, (hw->flags & ICONIFIED) ? hw->icon : hw->frame);
#else
      if (hw->flags & ICONIFIED)
        XUnmapWindow(d, hw->icon);
#endif
      hw->flags &= ~MAPPED;
      if (pointer == hw)
        pointer = NULL;
      if (focus == hw)
        set_focus(NULL);
      continue;
    case MapRequest:
#ifdef VERBOSE
      fprintf(ERR, "%lx", ev.xmaprequest.window);
#endif
      if ((i = find_window(ev.xmaprequest.window, &hw)) == CHILD) {
        if (hw->flags & ICONIFIED) {
          XUnmapWindow(d, hw->icon);
          hw->flags &= ~ICONIFIED;
        }
        XMapRaised(d, hw->child);
#ifdef FRAME
        XMapRaised(d, hw->frame);
#endif
        hw->flags |= MAPPED;
      } else if (i == UNDEFINED) {
        XGrabServer(d);
        hw = add_child(ev.xmaprequest.window, True);
        XUngrabServer(d);
      } else
        continue;
      if ((options & FOCUS_WHEN_MAPPED) && hw)
        set_focus(hw);
      continue;
    case ConfigureRequest:
#ifdef VERBOSE
      fprintf(ERR, "%lx", ev.xconfigurerequest.window);
#endif
      configure(&ev.xconfigurerequest);
      continue;
    case PropertyNotify:
#ifdef VERBOSE
      fprintf(ERR, "%c %lx ",
              (ev.xproperty.state == PropertyNewValue) ? 'n' : 'd',
              ev.xproperty.atom);
      if (ev.xproperty.atom <= XA_LAST_PREDEFINED)
        fputs(anames[ev.xproperty.atom], ERR);
      else {
        char *aname;
        aname = XGetAtomName(d, ev.xproperty.atom);
        fputs(aname, ERR);
        XFree(aname);
      }
#endif
      if (ev.xproperty.state == PropertyDelete ||
          find_window(ev.xproperty.window, &hw) != CHILD)
        continue;
      switch (ev.xproperty.atom) {
#ifdef FRAME
      case XA_WM_NAME:
        if (hw->name != no_name && hw->name != hw->icon_name)
          XFree(hw->name);
        if ((hw->name = get_wm_name(ev.xproperty.window, CHILD)) == NULL)
          hw->name = no_name;
        if (!(hw->flags & ICONIFIED))
          XClearArea(d, hw->frame, 0, frame_w + V_OFFSET + shade_w, hw->w,
#ifdef NLS
                     extents->max_ink_extent.height
#else
                     font_info->ascent + font_info->descent
#endif
                     ,
                     True);
        continue;
#endif
      case XA_WM_ICON_NAME:
        if (hw->icon_name != no_name
#ifdef FRAME
            && hw->icon_name != hw->name
#endif
        )
          XFree(hw->icon_name);
        if ((hw->icon_name = get_wm_name(ev.xproperty.window, ICON)) == NULL)
          hw->icon_name =
#ifdef FRAME
              hw->name;
#else
              no_name;
#endif
        if (hw->flags & NEED_TO_DRAW_ICON) {
          hw->icon_w =
              H_OFFSET * 2 +
#ifdef NLS
              XmbTextEscapement(fontset, hw->icon_name, strlen(hw->icon_name));
#else
              XTextWidth(font_info, hw->icon_name, strlen(hw->icon_name));
#endif
          XResizeWindow(d, hw->icon, (unsigned int)hw->icon_w,
                        (unsigned int)hw->icon_h);
          if (hw->flags & ICONIFIED)
            XClearArea(d, hw->icon, 0, 0, 0, 0, True);
        }
        continue;
      case XA_WM_HINTS:
        get_wm_hints(ev.xproperty.window, hw);
        continue;
      case XA_WM_NORMAL_HINTS:
        get_wm_normal_hints(ev.xproperty.window, hw, NULL);
        continue;
      default:
        if (ev.xproperty.atom == wm_protocols)
          get_wm_protocols(ev.xproperty.window, hw);
        continue;
      }
      continue;
    case ColormapNotify:
#ifdef VERBOSE
      fprintf(ERR, "%snstalled cmap %lx, new=%s",
              (ev.xcolormap.state == ColormapInstalled) ? "I" : "Uni",
              ev.xcolormap.colormap,
              (ev.xcolormap.new == True) ? "True" : "False");
#endif
      if (ev.xcolormap.new == True &&
          find_window(ev.xcolormap.window, &hw) != UNDEFINED) {
        hw->cmap = ev.xcolormap.colormap;
        if (focus == hw)
          XInstallColormap(d, cur_cmap = hw->cmap);
      }
      continue;
    default:
#ifdef VERBOSE
      fputs("-", ERR);
#endif
      continue;
    }
  }
}

int main(int argc, char *argv[]) {
  int i;
  char *display = NULL, *rc = NULL;

  for (i = 1; i < argc; i++)
    if (argv[i][0] != '-' || ++i >= argc) {
      version(stdout);
      return 1;
    } else {
      switch (argv[i - 1][1]) {
      case 'd':
        display = argv[i];
        break;
      case 'f':
        rc = argv[i];
        break;
      default:
        version(stdout);
        return 1;
      }
    }

#ifdef VERBOSE
  version(ERR);
#endif
  if (init(display, rc) != 0)
    return 1;

  process_events();
  return 0; /* though it never gets here... */
}
