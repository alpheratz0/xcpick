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

	 ________________________________
	( pipe xcpick output to xclip =) )
	 --------------------------------
	    o               ,-----._
	  .  o         .  ,'        `-.__,------._
	 //   o      __\\'                        `-.
	((    _____-'___))                           |
	 `:='/     (alf_/                            |
	 `.=|      |='                               |
	    |)   O |                                  \
	    |      |                               /\  \
	    |     /                          .    /  \  \
	    |    .-..__            ___   .--' \  |\   \  |
	   |o o  |     ``--.___.  /   `-'      \  \\   \ |
	    `--''        '  .' / /             |  | |   | \
	                 |  | / /              |  | |   mmm
	                 |  ||  |              | /| |
	                 ( .' \ \              || | |
	                 | |   \ \            // / /
	                 | |    \ \          || |_|
	                /  |    |_/         /_|
	               /__/


*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

#define XC_GOBBLER (54)
#define XCB_PLANES_ALL_PLANES ((uint32_t)(~0UL))

static void
die(const char *err)
{
	fprintf(stderr, "xcpick: %s\n", err);
	exit(1);
}

static void
dief(const char *fmt, ...)
{
	va_list args;

	fputs("xcpick: ", stderr);
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fputc('\n', stderr);
	exit(1);
}

static const char *
enotnull(const char *str, const char *name)
{
	if (NULL == str) {
		dief("%s cannot be null", name);
	}

	return str;
}

static void
usage(void)
{
	puts("usage: xcpick [-hHv] [-p prefix]");
	exit(0);
}

static void
version(void)
{
	puts("xcpick version "VERSION);
	exit(0);
}

static xcb_point_t
get_pointer_position(xcb_connection_t *conn, xcb_window_t window)
{
	xcb_point_t position;
	xcb_query_pointer_cookie_t cookie;
	xcb_query_pointer_reply_t *reply;
	xcb_generic_error_t *error;

	error = NULL;
	cookie = xcb_query_pointer(conn, window);
	reply = xcb_query_pointer_reply(conn, cookie, &error);

	if (NULL != error) {
		dief("xcb_query_pointer failed with error code: %d",
				(int)(error->error_code));
	}

	position.x = reply->root_x;
	position.y = reply->root_y;

	free(reply);

	return position;
}

static xcb_cursor_t
load_cursor(xcb_connection_t *conn, int16_t id)
{
	xcb_font_t font;
	xcb_cursor_t cursor;
	xcb_void_cookie_t cookie;
	xcb_generic_error_t *error;

	font = xcb_generate_id(conn);
	cursor = xcb_generate_id(conn);
	cookie = xcb_open_font_checked(conn, font, strlen("cursor"), "cursor");
	error = xcb_request_check(conn, cookie);

	if (NULL != error) {
		dief("xcb_open_font failed with error code: %d",
				(int)(error->error_code));
	}

	cookie = xcb_create_glyph_cursor_checked(
		conn, cursor, font, font,
		id, id + 1, 0xffff, 0xffff, 0xffff, 0, 0, 0
	);

	error = xcb_request_check(conn, cookie);

	if (NULL != error) {
		dief("xcb_create_glyph_cursor failed with error code: %d",
				(int)(error->error_code));
	}

	xcb_close_font(conn, font);

	return cursor;
}

static uint32_t
get_color_at(xcb_connection_t *conn,
             xcb_window_t window,
             int16_t x,
             int16_t y)
{
	xcb_get_image_reply_t *reply;
	xcb_get_image_cookie_t cookie;
	xcb_generic_error_t *error;
	uint32_t color;
	int data_length;
	uint8_t *data;

	error = NULL;
	cookie = xcb_get_image(
		conn, XCB_IMAGE_FORMAT_Z_PIXMAP,
		window, x, y, 1, 1, XCB_PLANES_ALL_PLANES
	);

	reply = xcb_get_image_reply(conn, cookie, &error);

	if (NULL != error) {
		dief("xcb_get_image failed with error code: %d",
				(int)(error->error_code));
	}

	data = xcb_get_image_data(reply);
	data_length = xcb_get_image_data_length(reply);

	if (data_length != 4) {
		dief("invalid pixel format received, expected: 32bpp got: %dbpp",
				data_length * 8);
	}

	color = data[2] << 16 | data[1] << 8 | data[0];

	free(reply);

	return color;
}

int
main(int argc, char **argv)
{
	xcb_connection_t *conn;
	xcb_grab_pointer_reply_t *gpr;
	xcb_screen_t *screen;
	xcb_window_t window;
	xcb_cursor_t cursor;
	xcb_generic_event_t *ev;
	xcb_motion_notify_event_t *mnev;
	xcb_button_press_event_t *bpev;
	uint32_t fill_color, border_color;
	xcb_point_t pointer_position;
	int print_newline;
	int exit_status;
	const char *prefix = "";

	if (++argv, --argc > 0) {
		if (!strcmp(*argv, "-h")) usage();
		else if (!strcmp(*argv, "-v")) version();
		else if (!strcmp(*argv, "-p")) --argc, prefix = enotnull(*++argv, "prefix");
		else if (!strcmp(*argv, "-H")) prefix = "#";
		else if (**argv == '-') dief("invalid option %s", *argv);
		else dief("unexpected argument: %s", *argv);
	}

	if (xcb_connection_has_error(conn = xcb_connect(NULL, NULL))) {
		die("can't open display");
	}

	if (NULL == (screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data)) {
		xcb_disconnect(conn);
		die("can't get default screen");
	}

	cursor = load_cursor(conn, XC_GOBBLER);

	gpr = xcb_grab_pointer_reply(
		conn,
		xcb_grab_pointer_unchecked(
			conn, 0, screen->root,
			XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_BUTTON_PRESS,
			XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, XCB_NONE,
			cursor, XCB_CURRENT_TIME
		),
		NULL
	);

	if (gpr->status != XCB_GRAB_STATUS_SUCCESS) {
		die("can't grab pointer");
	}

	free(gpr);

	window = xcb_generate_id(conn);
	exit_status = 0;
	print_newline = isatty(STDOUT_FILENO);
	pointer_position = get_pointer_position(conn, screen->root);
	fill_color = get_color_at(conn, screen->root, pointer_position.x, pointer_position.y);
	border_color = 0xffffff;

	xcb_create_window(
		conn, XCB_COPY_FROM_PARENT,
		window, screen->root, pointer_position.x, pointer_position.y + 25,
		44, 44, 3, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual,
		XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_OVERRIDE_REDIRECT,
		(const uint32_t[3]) {
			fill_color,
			border_color,
			1
		}
	);

	xcb_change_property(
		conn, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_NAME,
		XCB_ATOM_STRING, 8, strlen("xcpick"), "xcpick"
	);

	xcb_map_window(conn, window);
	xcb_flush(conn);

	while ((ev = xcb_wait_for_event(conn))) {
		switch (ev->response_type & ~0x80) {
			case XCB_MOTION_NOTIFY:
				mnev = (xcb_motion_notify_event_t *)(ev);

				fill_color = get_color_at(
					conn, screen->root,
					mnev->event_x, mnev->event_y
				);

				xcb_change_window_attributes(
					conn, window,
					XCB_CW_BACK_PIXEL, &fill_color
				);

				xcb_clear_area(conn, 0, window, 0, 0, 44, 44);

				xcb_configure_window(
					conn, window,
					XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
					(const uint32_t[2]) {
						mnev->event_x < 25 ?
							25 :
							(mnev->event_x >= screen->width_in_pixels - 75 ?
								screen->width_in_pixels - 75 :
								mnev->event_x
							),
						mnev->event_y >= screen->height_in_pixels - 75 ?
							mnev->event_y - 75 :
							mnev->event_y + 25,
					}
				);

				xcb_flush(conn);
				break;
			case XCB_BUTTON_PRESS:
				bpev = (xcb_button_press_event_t *)(ev);

				switch (bpev->detail) {
					case XCB_BUTTON_INDEX_1:
						printf("%s%06x%s", prefix, fill_color, print_newline ? "\n" : "");
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
		}

		free(ev);
	}

end:
	xcb_free_cursor(conn, cursor);
	xcb_disconnect(conn);

	return exit_status;
}
