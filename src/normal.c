/*
 * warpd - A modal keyboard-driven pointing system.
 *
 * © 2019 Raheman Vaiya (see: LICENSE).
 */

#include "warpd.h"

void draw_cross_around_mouse_cursor(screen_t scr, int x, int y, int sw,
				       int sh, const char *curcol, const int cursz)
{
	const int cross_line = cursz * 5;
	const int left_width = x < cross_line ? x : cross_line;
	platform->screen_draw_box(scr, 0, y - cursz / 2, left_width, cursz, curcol);

	const int rightX = x > sw - cross_line ? x : sw - cross_line;
	platform->screen_draw_box(scr, rightX, y - cursz / 2, cross_line, cursz, curcol);

	const int top_height = y < cross_line ? y : cross_line;
	platform->screen_draw_box(scr, x + 1, 0, cursz, top_height > 0 ? top_height : 1, curcol);

	if (y > sh - cross_line) {
		platform->screen_draw_box(scr, x + 1, y, cursz, sh - y, curcol);
	} else {
		platform->screen_draw_box(scr, x + 1, sh - cross_line, cursz, cross_line, curcol);
	}

	if (x < cross_line || x > sw - cross_line ||
		y < cross_line || y > sh - cross_line) {
		const char *crossed_color = config_get("cursor_color_crossed");

		platform->screen_draw_box(scr, x+1, y-cursz/2,
			cursz, cursz, crossed_color);
	}
}

void draw_cursor_current(screen_t scr)
{
	int sw, sh, x, y;
	platform->screen_get_dimensions(scr, &sw, &sh);
	platform->mouse_get_position(&scr, &x, &y);
	draw_cursor(scr, x, y, sw, sh, 0);
}

void draw_cursor(screen_t scr, int x, int y, int sw, int sh, int hide_cursor)
{
	const char *curcol = active_drag_cursor_color ? active_drag_cursor_color : config_get("cursor_color");
	const int cursz = config_get_int("cursor_size");
	if (!hide_cursor) {
		platform->screen_draw_box(scr, x+1, y-cursz/2,
				cursz, cursz, curcol);
		if (config_get_int("normal_cursor_cross")) {
			draw_cross_around_mouse_cursor(scr, x, y, sw, sh, curcol, cursz);
		}
	}
}


static void redraw(screen_t scr, int x, int y, int hide_cursor)
{
	int sw, sh;

	platform->screen_get_dimensions(scr, &sw, &sh);

	const int gap = 10;
	const int indicator_size = (config_get_int("indicator_size") * sh) / 1080;
	const char *indicator_color = config_get("indicator_color");
	const char *indicator = config_get("indicator");

	platform->screen_clear(scr);

	draw_cursor(scr, x, y, sw, sh, hide_cursor);

	if (!strcmp(indicator, "bottomleft"))
		platform->screen_draw_box(scr, gap, sh-indicator_size-gap, indicator_size, indicator_size, indicator_color);
	else if (!strcmp(indicator, "topleft"))
		platform->screen_draw_box(scr, gap, gap, indicator_size, indicator_size, indicator_color);
	else if (!strcmp(indicator, "topright"))
		platform->screen_draw_box(scr, sw-indicator_size-gap, gap, indicator_size, indicator_size, indicator_color);
	else if (!strcmp(indicator, "bottomright"))
		platform->screen_draw_box(scr, sw-indicator_size-gap, sh-indicator_size-gap, indicator_size, indicator_size, indicator_color);

	platform->commit();
}

static void move(screen_t scr, int x, int y, int hide_cursor)
{
	platform->mouse_move(scr, x, y);
	redraw(scr, x, y, hide_cursor);
}

static int join_static_keys_with_others(const char **keys, const char *static_keys[], size_t static_sz,
		      const struct drag_action_holder *das)
{
	int nstatic = static_sz / sizeof static_keys[0];
	int nkeys = 0;
	for (int i = 0; i < nstatic; i++)
		keys[nkeys++] = static_keys[i];
	for (int i = 0; i < das->nr; i++)
		keys[nkeys++] = das->drag_actions[i].trigger_cl;
	return nkeys;
}

struct input_event *normal_mode(struct input_event *start_ev, int oneshot, struct drag_action_holder *dah)
{
	const int cursz = config_get_int("cursor_size");
	const int system_cursor = config_get_int("normal_system_cursor");
	const char *blink_interval = config_get("normal_blink_interval");

	int on_time, off_time;
	struct input_event *ev;
	screen_t scr;
	int sh, sw;
	int mx, my;
	int dragging = 0;
	int show_cursor = !system_cursor;

	int n = sscanf(blink_interval, "%d %d", &on_time, &off_time);
	assert(n > 0);
	if (n == 1)
		off_time = on_time;

	const char *static_keys[] = {
		"accelerator",
		"bottom",
		"buttons",
		"copy_and_exit",
		"decelerator",
		"down",
		"drag",
		"end",
		"exit",
		"grid",
		"hint",
		"hint2",
		"hint_near_top_left",
		"hint_near_top_right",
		"hint_near_bottom_left",
		"hint_near_bottom_right",
		"hint_near_vertical_up",
		"hint_near_vertical_down",
		"hint_near_horizon_left",
		"hint_near_horizon_right",
		"hist_back",
		"hist_forward",
		"history",
		"left",
		"middle",
		"oneshot_buttons",
		"print",
		"right",
		"screen",
		"scroll_down",
		"scroll_up",
		"scroll_right",
		"scroll_left",
		"start",
		"top",
		"up",
	};

	const char *keys[sizeof static_keys / sizeof static_keys[0] + MAX_DRAG_ACTIONS];
	int nkeys = join_static_keys_with_others(
	    keys, static_keys, sizeof static_keys, dah);

	platform->input_grab_keyboard();

	platform->mouse_get_position(&scr, &mx, &my);
	platform->screen_get_dimensions(scr, &sw, &sh);

	if (!system_cursor)
		platform->mouse_hide();

	mouse_reset();
	redraw(scr, mx, my, !show_cursor);

	int prev_mx = mx, prev_my = my;
	uint64_t time = 0;
	uint64_t last_blink_update = 0;
	while (1) {
		config_input_whitelist(keys, nkeys);
		if (start_ev == NULL) {
			ev = platform->input_next_event(10);
			time += 10;
		} else {
			ev = start_ev;
			start_ev = NULL;
		}

		platform->mouse_get_position(&scr, &mx, &my);

		if (!system_cursor && on_time) {
			if (show_cursor && (time - last_blink_update) >= on_time) {
				show_cursor = 0;
				redraw(scr, mx, my, !show_cursor);
				last_blink_update = time;
			} else if (!show_cursor && (time - last_blink_update) >= off_time) {
				show_cursor = 1;
				redraw(scr, mx, my, !show_cursor);
				last_blink_update = time;
			}
		}

		scroll_tick();
		if (mouse_process_key(ev, "up", "down", "left", "right")) {
			redraw(scr, mx, my, !show_cursor);
			continue;
		}

		if (!ev)  {
			goto next;
		} else if (config_input_match(ev, "scroll_down")) {
			if (ev->pressed) {
				scroll_stop();
				scroll_accelerate(SCROLL_DOWN);
			} else {
				scroll_decelerate();
			}
			redraw(scr, mx, my, 0);
		} else if (config_input_match(ev, "scroll_up")) {
			if (ev->pressed) {
				scroll_stop();
				scroll_accelerate(SCROLL_UP);
			} else {
				scroll_decelerate();
			}
			redraw(scr, mx, my, 0);
		} else if (config_input_match(ev, "scroll_right")) {
			if (ev->pressed) {
				scroll_stop();
				scroll_accelerate(SCROLL_RIGHT);
			} else {
				scroll_decelerate();
			}
			redraw(scr, mx, my, 0);
		} else if (config_input_match(ev, "scroll_left")) {
			if (ev->pressed) {
				scroll_stop();
				scroll_accelerate(SCROLL_LEFT);
			} else {
				scroll_decelerate();
			}
			redraw(scr, mx, my, 0);
		} else if (config_input_match(ev, "accelerator")) {
			if (ev->pressed)
				mouse_fast();
			else
				mouse_normal();
		} else if (config_input_match(ev, "decelerator")) {
			if (ev->pressed)
				mouse_slow();
			else
				mouse_normal();
		} else if (!ev->pressed) {
			goto next;
		}

		if (config_input_match(ev, "top"))
			move(scr, mx, cursz / 2, !show_cursor);
		else if (config_input_match(ev, "bottom"))
			move(scr, mx, sh - cursz / 2, !show_cursor);
		else if (config_input_match(ev, "middle"))
			move(scr, mx, sh / 2, !show_cursor);
		else if (config_input_match(ev, "start"))
			move(scr, 1, my, !show_cursor);
		else if (config_input_match(ev, "end"))
			move(scr, sw - cursz, my, !show_cursor);
		else if (config_input_match(ev, "hist_back")) {
			hist_add(mx, my);
			hist_prev();
			hist_get(&mx, &my);

			move(scr, mx, my, !show_cursor);
		} else if (config_input_match(ev, "hist_forward")) {
			hist_next();
			hist_get(&mx, &my);

			move(scr, mx, my, !show_cursor);
		} else if (config_input_match(ev, "drag")) {
			dragging = !dragging;
			if (dragging)
				platform->mouse_down(config_get_int("drag_button"));
			else
				platform->mouse_up(config_get_int("drag_button"));
		} else {
			if (handle_drag_action(ev, dah, &scr)) {
				redraw(scr, mx, my, !show_cursor);
			} else {
		if (config_input_match(ev, "copy_and_exit")) {
			platform->mouse_up(config_get_int("drag_button"));
			platform->copy_selection();
			ev = NULL;
			goto exit;
		} else if (config_input_match(ev, "exit") ||
			   config_input_match(ev, "grid") ||
			   config_input_match(ev, "screen") ||
			   config_input_match(ev, "history") ||
			   config_input_match(ev, "hint2") ||
			   config_input_match(ev, "hint_near_top_left") ||
			   config_input_match(ev, "hint_near_top_right") ||
			   config_input_match(ev, "hint_near_bottom_left") ||
			   config_input_match(ev, "hint_near_bottom_right") ||
			   config_input_match(ev, "hint_near_vertical_up") ||
			   config_input_match(ev, "hint_near_vertical_down") ||
			   config_input_match(ev, "hint_near_horizon_left") ||
			   config_input_match(ev, "hint_near_horizon_right") ||
			   config_input_match(ev, "hint")) {
			goto exit;
		} else if (config_input_match(ev, "print")) {
			printf("%d %d %s\n", mx, my, input_event_tostr(ev));
			fflush(stdout);
		} else { /* Mouse Buttons. */
			int btn;

			if ((btn = config_input_match(ev, "buttons"))) {
				if (oneshot && !is_oneshot_removed()) {
					printf("%d %d\n", mx, my);
					exit(btn);
				}

				hist_add(mx, my);
				histfile_add(mx, my);
				platform->mouse_click(btn);
				stop_all_drag_actions(dah);
				draw_cursor_current(scr);
			} else if ((btn = config_input_match(ev, "oneshot_buttons"))) {
				hist_add(mx, my);
				platform->mouse_click(btn);
				stop_all_drag_actions(dah);
				draw_cursor_current(scr);

				const int timeout = config_get_int("oneshot_timeout");

				while (1) {
					struct input_event *ev = platform->input_next_event(timeout);

					if (!ev)
						break;

					if (ev && ev->pressed &&
						config_input_match(ev, "oneshot_buttons")) {
						platform->mouse_click(btn);
					}
				}

				goto exit;
			}
		}}}
	next:
		platform->mouse_get_position(&scr, &mx, &my);

		if ((mx != prev_mx || my != prev_my))
			redraw(scr, mx, my, !show_cursor);

		prev_mx = mx;
		prev_my = my;
		platform->commit();
	}

exit:
	platform->mouse_show();
	platform->screen_clear(scr);

	platform->input_ungrab_keyboard();
	if (dragging) {
		platform->mouse_up(config_get_int("drag_button"));
	}
	if (!config_input_match(ev, "history") &&
	    !config_input_match(ev, "hint2") &&
	    !config_input_match(ev, "hint_near_top_left") &&
	    !config_input_match(ev, "hint_near_top_right") &&
	    !config_input_match(ev, "hint_near_bottom_left") &&
	    !config_input_match(ev, "hint_near_bottom_right") &&
	    !config_input_match(ev, "hint_near_vertical_up") &&
	    !config_input_match(ev, "hint_near_vertical_down") &&
	    !config_input_match(ev, "hint_near_horizon_left") &&
	    !config_input_match(ev, "hint_near_horizon_right") &&
	    !config_input_match(ev, "hint")) {
		stop_all_drag_actions(dah);
	}

	platform->commit();
	return ev;
}
