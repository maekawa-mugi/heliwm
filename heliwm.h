/*
 * heliwm - Highly Essential Light Itsy-bitsy Window Manager
 *
 * heliwm.h - main header file
 * Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005 Hidetoshi Ohtomo
 */

#define VERSION "1.13"

/* Alpha particle version doesn't need shape extension */
#ifndef FRAME
#ifdef SHAPE
#undef SHAPE
#endif
#endif

/* Values used in move_or_resize() and find_window() */
#define UNDEFINED	0
#define CHILD		1
#define ICON		2

/*
 * Operation codes.  Used in "op" member of keys[], and also as "operation"
 * argument of move_or_resize() and update_indicator().
 */

#define OP_MOVE		0
#define OP_RESIZE	1
#define OP_ICONIFY	2
#define OP_NEXT		3
#define OP_PREV		4
#define OP_RAISE	5
#define OP_LOWER	6
#define OP_DELETE	7
#define OP_TERMINATE	8
#define OP_MAXIMIZE	9
#define OP_INFO		10
#define OP_EXEC		11
#define OP_REORDER	12
#define OP_MAXIMIZE_H	13
#define OP_MAXIMIZE_V	14

/*
 * Note: the order of OP_SUB_KEYS must be the same as that of operation codes,
 * because they are used as indexes for OP_SUB_KEYS.
 */

#define OP_SUB_KEYS	"mrinpaldtxfeohv"
#define NUM_OP_SUB_KEYS	15

#ifdef VERBOSE
#define ERR stdout	/* target of VERBOSE messages */
#endif

/* Flags: used in `flags' member of heliwm_window. */
#define NEED_TO_REDRAW		(1<<0)
#define MAX_SIZE_DEFINED	(1<<1)
#define ICON_POS_DEFINED	(1<<2)
#define CREATED_ICON		(1<<3)
#define NEED_TO_DRAW_ICON	(1<<4)
#define MAPPED			(1<<5)
#define ICONIFIED		(1<<6)
#define IGNORE_UNMAP		(1<<7)
#define INPUT			(1<<8)
#define SHAPED			(1<<9)
#define TAKE_FOCUS_IS_SET	(1<<10)
#define DELETE_WINDOW_IS_SET	(1<<11)
#ifdef MAXIMIZE
#define MAXIMIZED_HORIZONTALLY	(1<<12)
#define MAXIMIZED_VERTICALLY	(1<<13)
#endif

/* option flags */
#define FOCUS_WHEN_RAISED		(1<<0)
#define FOCUS_WHEN_CIRCULATED		(1<<1)
#define FOCUS_WHEN_MAPPED		(1<<2)
#define RAISE_WHEN_CIRCULATED		(1<<3)
#define WARP_POINTER_WHEN_FOCUSED	(1<<4)
#define SET_ORIGINAL_BORDER_TO_ZERO	(1<<5)
#define REDRAW_ACTIVE_WINDOW_ONLY	(1<<6)
#define INTERACTIVE_PLACEMENT		(1<<7)

typedef struct _heliwm_window {
	struct _heliwm_window *next, *prev;
	Window child, icon;
#ifdef FRAME
	Window frame;
#endif
	Colormap cmap;
	Pixmap icon_pm;
	short x, y;		/* position of frame or child */
	short icon_x, icon_y;	/* position of icon */
#ifdef FRAME
	int w, h;		/* width and height of frame */
#endif
	int child_w, child_h;	/* those of child */
	int icon_w, icon_h;	/* those of icon */
	int child_bw;		/* border width of child */
	int icon_bw;		/* that of icon */
	int base_w, base_h;	/* base width and height */
	int inc_w, inc_h;	/* "arithmetic progression of sizes" */
	int max_w, max_h;	/* maximum width and height */
#ifdef MAXIMIZE
	int ow, oh;		/* width & height of child before maximizing */
	short ox, oy;		/* position before maximizing */
#endif
#ifdef FRAME
	char *name;		/* window name */
#endif
	char *icon_name;	/* icon name */
	unsigned short flags;	/* flags */
} heliwm_window;

/* key operation structure */
struct keyop {
	KeyCode keycode;	/* keycode */
	unsigned int state;	/* modifier keys */
	KeySym keysym;		/* key name */
	int op;			/* operation code - see above */
};

/* mouse button operation structure */
struct buttonop {
	unsigned int state;	/* modifier keys */
	unsigned int button;	/* button number */
	int op;			/* operation code - see above */
};

extern Display *d;
extern Window rootwin, indicator;
extern Cursor cursor, fleur;
extern Atom wm_delete_window;
extern Colormap cmap;
extern GC text_gc, band_gc;
extern int scr, scr_w, scr_h, ind_w, bdw, title_h;
extern int defined_keys, defined_buttons, options;
extern unsigned long bgc, bdc;
extern struct keyop keys[];
extern struct buttonop buttons[];
extern heliwm_window *list, *last, *focus, *pointer;

#ifdef FRAME
extern GC frame0_gc, frame1_gc;
extern int shade_w, frame_w;
#endif

#ifdef NLS
extern XFontSet fontset;
extern XFontSetExtents *extents;
#else
extern XFontStruct *font_info;
#endif

#ifdef VERBOSE
extern char *events[];
#endif

extern int configure_heliwm(char *);
extern int find_window(Window, heliwm_window **);
extern int recalculate_heliwm_window_height(heliwm_window *);
extern void set_focus(heliwm_window *);
extern void send_cmessage(Window, Atom);
extern void move(heliwm_window *, int);
extern void iconify(heliwm_window *);
extern void exec_operation(Window, int);
