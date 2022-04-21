/*
	Copyright (C) 2022 <alpheratz99@protonmail.com>

	This program is free software; you can redistribute it and/or modify it under
	the terms of the GNU General Public License version 2 as published by the
	Free Software Foundation.

	This program is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
	FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along with
	this program; if not, write to the Free Software Foundation, Inc., 59 Temple
	Place, Suite 330, Boston, MA 02111-1307 USA

*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include "numdef.h"
#include "debug.h"
#include "cursorfont.h"

static bool
match_opt(const char *in, const char *sh, const char *lo) {
	return (strcmp(in, sh) == 0) ||
		   (strcmp(in, lo) == 0);
}

static void
usage(void) {
	puts("Usage: xcpick [ -hv ]");
	puts("Options are:");
	puts("     -h | --help                    display this message and exit");
	puts("     -v | --version                 display the program version");
	exit(0);
}

static void
version(void) {
	puts("xcpick version "VERSION);
	exit(0);
}

static u32
xcb_get_cursor_position(xcb_connection_t *connection, xcb_window_t window) {
	u32 position;
	xcb_query_pointer_reply_t *reply;

	reply = xcb_query_pointer_reply(
		connection,
		xcb_query_pointer_unchecked(
			connection,
			window
		),
		NULL
	);

	position = (u16)(reply->root_x) << 16 | (u16)(reply->root_y);

	free(reply);

	return position;
}

static xcb_cursor_t
xcb_get_cursor(xcb_connection_t *connection, i16 id) {
	xcb_font_t font;
	xcb_cursor_t cursor;

	font = xcb_generate_id(connection);
	cursor = xcb_generate_id(connection);

	xcb_open_font(connection, font, strlen("cursor"), "cursor");
	xcb_create_glyph_cursor(connection, cursor, font, font, id, id + 1, 0xffff, 0xffff, 0xffff, 0, 0, 0);
	xcb_close_font(connection, font);

	return cursor;
}

static u32
xcb_get_color_at(xcb_connection_t *connection, xcb_window_t window, i16 x, i16 y) {
	xcb_get_image_reply_t *reply;
	u32 color;
	u8 *data;

	reply = xcb_get_image_reply(
		connection,
		xcb_get_image_unchecked(
			connection, XCB_IMAGE_FORMAT_Z_PIXMAP,
			window, x, y, 1, 1,
			(u32)(~0UL)
		),
		NULL
	);

	data = xcb_get_image_data(reply);
	color = data[2] << 16 | data[1] << 8 | data[0];

	free(reply);

	return color;
}

int
main(int argc, char **argv) {
	/* skip program name */
	--argc; ++argv;

	if (argc > 0) {
		if (match_opt(*argv, "-h", "--help")) usage();
		else if (match_opt(*argv, "-v", "--version")) version();
		else dief("invalid option %s", *argv);
	}

	xcb_connection_t *connection;
	xcb_screen_t *screen;
	xcb_window_t window;
	xcb_gcontext_t fill, border;
	xcb_cursor_t cursor;
	xcb_generic_event_t *ev;
	xcb_motion_notify_event_t *mnev;
	xcb_button_press_event_t *bpev;
	u32 fill_color, border_color, cursor_position;
	int exit_status;

	exit_status = 0;

	if (xcb_connection_has_error(connection = xcb_connect(NULL, NULL))) {
		die("can't open display");
	}

	if (!(screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data)) {
		xcb_disconnect(connection);
		die("can't get default screen");
	}

	window = xcb_generate_id(connection);
	fill = xcb_generate_id(connection);
	border = xcb_generate_id(connection);
	cursor = xcb_get_cursor(connection, XC_GOBBLER);

	xcb_grab_pointer(
		connection, 0, screen->root,
		XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_BUTTON_PRESS,
		XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, XCB_NONE,
		cursor, XCB_CURRENT_TIME
	);

	cursor_position = xcb_get_cursor_position(connection, screen->root);
	fill_color = xcb_get_color_at(connection, screen->root, cursor_position >> 16, (cursor_position & 0xffff));
	border_color = 0xffffff;

	xcb_create_window(
		connection, XCB_COPY_FROM_PARENT,
		window, screen->root, cursor_position >> 16, (cursor_position & 0xffff) + 25, 50, 50, 0,
		XCB_WINDOW_CLASS_INPUT_OUTPUT,
		screen->root_visual, XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
		(const u32[2]) {
			fill_color,
			XCB_EVENT_MASK_EXPOSURE
		}
	);

	xcb_create_gc(connection, fill, window, XCB_GC_FOREGROUND, &fill_color);
	xcb_create_gc(connection, border, window, XCB_GC_FOREGROUND, &border_color);

	xcb_change_property(
		connection, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_NAME,
		XCB_ATOM_STRING, 8, strlen("xcpick"), "xcpick"
	);

	xcb_change_window_attributes(connection, window, XCB_CW_OVERRIDE_REDIRECT, (const u32[1]) { 0x1 });

	xcb_map_window(connection, window);
	xcb_flush(connection);

	while ((ev = xcb_wait_for_event(connection))) {
		switch (ev->response_type & ~0x80) {
			case XCB_EXPOSE:
				xcb_poly_fill_rectangle(connection, window, border, 1, (const xcb_rectangle_t[1]) {{ 0, 0, 50, 50 }});
				xcb_poly_fill_rectangle(connection, window, fill, 1, (const xcb_rectangle_t[1]) {{ 3, 3, 44, 44 }});
				xcb_flush(connection);
				break;
			case XCB_MOTION_NOTIFY:
				mnev = (xcb_motion_notify_event_t *)(ev);
				fill_color = xcb_get_color_at(connection, screen->root, mnev->event_x, mnev->event_y);

				xcb_change_gc(connection, fill, XCB_GC_FOREGROUND, &fill_color);
				xcb_clear_area(connection, 1, window, 0, 0, 1, 1);

				xcb_configure_window(
					connection, window,
					XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
					(const u32[2]) {
						mnev->event_x < 25 ? 25 : (mnev->event_x >= screen->width_in_pixels - 75 ? screen->width_in_pixels - 75 : mnev->event_x),
						mnev->event_y >= screen->height_in_pixels - 75 ? mnev->event_y - 75 : mnev->event_y + 25,
					}
				);

				xcb_flush(connection);
				break;
			case XCB_BUTTON_PRESS:
				bpev = (xcb_button_press_event_t *)(ev);

				switch (bpev->detail) {
					case XCB_BUTTON_INDEX_1:
						printf("%06x\n", fill_color);
						free(ev);
						goto end;
						break;
					case XCB_BUTTON_INDEX_2:
					case XCB_BUTTON_INDEX_3:
						exit_status = 2;
						free(ev);
						goto end;
						break;
				}

				break;
			default:
				break;
		}

		free(ev);
	}

end:
	xcb_free_cursor(connection, cursor);
	xcb_free_gc(connection, border);
	xcb_free_gc(connection, fill);
	xcb_free_cursor(connection, cursor);
	xcb_disconnect(connection);

	return exit_status;
}
