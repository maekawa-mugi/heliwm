/* config.c
 * Copyright (C) 2000, 2001, 2002, 2003 Hidetoshi Ohtomo */

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>

#include "heliwm.h"
#include "config.h"
#include "keydefs.h"

#define TEXT 0
#define BACKGROUND 1
#define BORDER 2
#define SHADE_UPPER_LEFT 3
#define SHADE_LOWER_RIGHT 4
#define BAND 5
#define FONT 6
#define NUM_VALUES (FONT + 1)

#define NAME(x) ((given[x] != NULL) ? given[x] : def[x])

#ifdef RCPARSER

#define COLOR_SUB_KEYS "tfbula"
#define NUM_COLOR_SUB_KEYS 6

#define COLOR_KEY 'c'
#define FONT_KEY 'f'
#define KEY_OP_KEY 'k'
#define BUTTON_OP_KEY 'b'
#define WIDTH_KEY 'w'
#define BORDER_WIDTH_SUB_KEY 'b'
#define FRAME_WIDTH_SUB_KEY 'f'
#define CURSOR_KEY 'C'
#define OTHER_OPTION_KEY 'o'

static void store_value(char *src, char **dest)
{
	if (*dest != NULL || (*dest = malloc(strlen(src) + 1)) == NULL)
		return;
	strcpy(*dest, src);
}

static void read_config_file(FILE *config, char **strings, int *frame_width,
	unsigned int *cursor_number)
{
	char line[384], *p;
	int i, op, len;
	unsigned int state, xbuttons[] = {Button1, Button2, Button3, Button4,
		Button5};

	while (fgets(line, sizeof(line), config) != NULL) {
		len = strlen(line) - 1;
		if (len < 3)
			continue;
		line[len] = '\0';
		switch (line[0]) {
		case COLOR_KEY:
			if (len < 4)
				break;
			for (i = 0; i < NUM_COLOR_SUB_KEYS; i++)
				if (COLOR_SUB_KEYS[i] == line[1])
					break;
			if (i == NUM_COLOR_SUB_KEYS)
				break;
			store_value(&line[3], &strings[i]);
			continue;
		case KEY_OP_KEY:
		case BUTTON_OP_KEY:
			if (len < 6)
				break;
			if (line[0] == KEY_OP_KEY) {
				if (defined_keys == MAX_KEY_DEFINITIONS)
					break;
			} else if (defined_buttons == MAX_BUTTON_DEFINITIONS)
				break;
			for (op = 0; op < NUM_OP_SUB_KEYS; op++)
				if (OP_SUB_KEYS[op] == line[1])
					break;
			if (op == NUM_OP_SUB_KEYS)
				break;
			state = 0;
			i = atoi(&line[3]);
			if (i & 1)
				state |= ShiftMask;
			if (i & 2)
				state |= ControlMask;
			if (i & 4)
				state |= Mod1Mask;
			if (i & 8)
				state |= Mod2Mask;
			p = &line[4];
			while (*p != ' ' && *p != '\t' && *p != '\0')
				p++;
			if (p == '\0')
				break;
			p++;
			if (line[0] == KEY_OP_KEY) {
				if ((keys[defined_keys].keycode =
					XKeysymToKeycode(d,
					XStringToKeysym(p))) == 0)
					break;
				keys[defined_keys].op = op;
				keys[defined_keys].state = state;
				defined_keys++;
			} else {
				if ((i = atoi(p)) < 1 || i > 5)
					break;
				buttons[defined_buttons].button = xbuttons[--i];
				buttons[defined_buttons].op = op;
				buttons[defined_buttons].state = state;
				defined_buttons++;
			}
#ifdef VERBOSE
			fprintf(ERR, "Registered %u+%u (%s) %c.\n", state,
				(line[0] == KEY_OP_KEY) ?
				keys[defined_keys - 1].keycode :
				buttons[defined_buttons - 1].button, p,
				OP_SUB_KEYS[op]);
#endif
			continue;
		case FONT_KEY:
			store_value(&line[2], &strings[FONT]);
			continue;
		case WIDTH_KEY:
			switch (line[1]) {
			case BORDER_WIDTH_SUB_KEY:
				if ((bdw = atoi(&line[3])) < 0)
					bdw = DEF_BORDER_WIDTH;
#ifdef VERBOSE
				fprintf(ERR, "bdw=%d\n", bdw);
#endif
				continue;
#ifdef FRAME
			case FRAME_WIDTH_SUB_KEY:
				if ((*frame_width = atoi(&line[3])) < 0)
					*frame_width = DEF_FRAME_WIDTH;
#ifdef VERBOSE
				fprintf(ERR, "frame_width=%d\n", *frame_width);
#endif
				continue;
#endif
			}
			break;
		case OTHER_OPTION_KEY:
			options = atoi(&line[2]);
#ifdef VERBOSE
			fprintf(ERR, "Options=%d (0x%x)\n"
				" %c 0x%x FOCUS_WHEN_RAISED\n"
				" %c 0x%x FOCUS_WHEN_CIRCULATED\n"
				" %c 0x%x FOCUS_WHEN_MAPPED\n"
				" %c 0x%x RAISE_WHEN_CIRCULATED\n"
				" %c 0x%x WARP_POINTER_WHEN_FOCUSED\n"
				" %c 0x%x SET_ORIGINAL_BORDER_TO_ZERO\n"
				" %c 0x%x REDRAW_ACTIVE_WINDOW_ONLY\n"
				" %c 0x%x INTERACTIVE_PLACEMENT\n",
				options, options,
				((options & FOCUS_WHEN_RAISED) ? '+' : ' '),
				FOCUS_WHEN_RAISED,
				((options & FOCUS_WHEN_CIRCULATED) ? '+' : ' '),
				FOCUS_WHEN_CIRCULATED,
				((options & FOCUS_WHEN_MAPPED) ? '+' : ' '),
				FOCUS_WHEN_MAPPED,
				((options & RAISE_WHEN_CIRCULATED) ? '+' : ' '),
				RAISE_WHEN_CIRCULATED,
				((options & WARP_POINTER_WHEN_FOCUSED) ? '+' :
				' '), WARP_POINTER_WHEN_FOCUSED,
				((options & SET_ORIGINAL_BORDER_TO_ZERO) ? '+'
				: ' '), SET_ORIGINAL_BORDER_TO_ZERO,
				((options & REDRAW_ACTIVE_WINDOW_ONLY) ? '+' :
				' '), REDRAW_ACTIVE_WINDOW_ONLY,
				((options & INTERACTIVE_PLACEMENT) ? '+' : ' '),
				INTERACTIVE_PLACEMENT);
#endif
			continue;
		case CURSOR_KEY:
			*cursor_number = atoi(&line[2]);
#ifdef VERBOSE
			fprintf(ERR, "cursor=%u\n", *cursor_number);
#endif
			continue;
		}
#ifdef VERBOSE
		fprintf(ERR, "- %s\n", line);
#endif
	}
}

#endif /* RCPARSER */

static int alloc_color(XColor *col, char *name)
{
	if (XParseColor(d, cmap, name, col) && XAllocColor(d, cmap, col))
		return 0;
	fprintf(stderr, "Failed to parse or allocate color %s.\n", name);
	return 1;
}

int configure_heliwm(char *config_file_name)
{
	XColor col;
	XGCValues val;
	int i = 0;
#ifdef RCPARSER
	int frame_width = DEF_FRAME_WIDTH;
#else
#ifdef FRAME
	int frame_width = DEF_FRAME_WIDTH;
#endif
#endif
	unsigned int cursor_num = DEF_CURSOR;
	unsigned long white;
	char *given[NUM_VALUES] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL},
		*def[NUM_VALUES] = {DEF_TEXT_COLOR, DEF_FRAME_COLOR,
		DEF_BORDER_COLOR, DEF_SHADE0_COLOR, DEF_SHADE1_COLOR,
		DEF_BAND_COLOR, DEF_FONT};
#ifdef RCPARSER
	FILE *config;
	char *name;
	int specified = 0;

	if (config_file_name == NULL)
		config_file_name = HOME_CF;
	else
		specified++;

	if (strncmp(config_file_name, "~/", 2) == 0) {
		char *home;

		i++;
		if ((home = getenv("HOME")) == NULL)
			home = ".";
		if ((name = malloc(strlen(home) + strlen(config_file_name))) ==
			NULL)
			return 1;	/* I hope that this will never occur */
		sprintf(name, "%s/%s", home, &config_file_name[2]);
	} else
		name = config_file_name;

	config = fopen(name, "r");
	if (i)
		free(name);
	if (config == NULL) {
		if (specified) {
			fprintf(stderr, "Can't open configuration file %s.\n",
				config_file_name);
			return 1;
		}
		config = fopen(SYSTEM_CF, "r");
	}
	if (config) {
		read_config_file(config, given, &frame_width, &cursor_num);
		fclose(config);
	}
#endif /* RCPARSER */

	if (defined_keys == 0) {
		for (i = 0; i < predefined_keys; i++) {
#ifdef VERBOSE
			fprintf(ERR, "Registering %u+%u (%s) %c.\n",
				keys[i].state, (unsigned int) keys[i].keysym,
				XKeysymToString(keys[i].keysym),
				OP_SUB_KEYS[keys[i].op]);
#endif
			keys[i].keycode = XKeysymToKeycode(d, keys[i].keysym);
		}
		defined_keys = predefined_keys;
	}
	if (defined_buttons == 0)
		defined_buttons = predefined_buttons;

#ifdef FRAME
	frame_w = frame_width + shade_w * 2;
#endif

#ifdef NLS
	{
		int nmissing;
		char **missing, *defstr;

		fontset = XCreateFontSet(d, NAME(FONT), &missing, &nmissing,
			&defstr);
		if (nmissing > 0) {
			fprintf(stderr, "%d missing charsets:", nmissing);
			for (i = 0; i < nmissing; i++)
				fprintf(stderr, " %s", missing[i]);
			XFreeStringList(missing);
			return 1;
		}
		extents = XExtentsOfFontSet(fontset);
	}
	title_h = extents->max_ink_extent.height + V_OFFSET * 2;
#else
	if ((font_info = XLoadQueryFont(d, NAME(FONT))) == NULL) {
		fprintf(stderr, "Can't open font %s.\n", NAME(FONT));
		return 1;
	}
	title_h = font_info->ascent + font_info->descent + V_OFFSET * 2;
#endif
#ifdef FRAME
	title_h += shade_w * 2;
#endif
	cursor = XCreateFontCursor(d, cursor_num);
	white = WhitePixel(d, scr);
	bdc = alloc_color(&col, NAME(BORDER)) ? white : col.pixel;
	bgc = alloc_color(&col, NAME(BACKGROUND)) ? BlackPixel(d, scr) :
		col.pixel;
	val.foreground = alloc_color(&col, NAME(TEXT)) ? white : col.pixel;
	val.background = bgc;
#ifndef NLS
	val.font = font_info->fid;
#endif

	text_gc = XCreateGC(d, rootwin,
#ifndef NLS
		GCFont |
#endif
		GCForeground | GCBackground, &val);

	val.function = GXxor;
	val.line_width = 0;
	val.line_style = LineSolid;
	val.cap_style = CapButt;
	val.join_style = JoinMiter;
	val.foreground = alloc_color(&col, NAME(BAND)) ? white : col.pixel;
	val.subwindow_mode = IncludeInferiors;
	band_gc = XCreateGC(d, rootwin, GCFunction | GCForeground | GCLineWidth
		| GCLineStyle | GCCapStyle | GCJoinStyle | GCSubwindowMode,
		&val);

#ifdef FRAME
	val.foreground = alloc_color(&col, NAME(SHADE_UPPER_LEFT)) ? white :
		col.pixel;
	frame0_gc = XCreateGC(d, rootwin, GCForeground | GCLineWidth |
		GCLineStyle | GCCapStyle | GCJoinStyle, &val);
	val.foreground = alloc_color(&col, NAME(SHADE_LOWER_RIGHT)) ? white :
		col.pixel;
	frame1_gc = XCreateGC(d, rootwin, GCForeground | GCLineWidth |
		GCLineStyle | GCCapStyle | GCJoinStyle, &val);
#endif
#ifdef RCPARSER
	for (i = 0; i < NUM_VALUES; i++)
		if (given[i] != NULL)
			free(given[i]);
#endif
	return 0;
}
