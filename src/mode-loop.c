#include "warpd.h"

static int oneshotRemoved = 0;

int is_oneshot_removed()
{
	return oneshotRemoved;
}

void remove_oneshot_flag()
{
	oneshotRemoved = 1;
}

int mode_loop(int initial_mode, int oneshot, int record_history)
{
	int mode = initial_mode;
	int nearMode = BOTTOM_RIGHT;
	int rc = 0;
	struct input_event *ev = NULL;

	struct drag_action_holder dah = {0};
	init_drag_action_holder(&dah);

	while (1) {
		int btn = 0;
		config_input_whitelist(NULL, 0);

		switch (mode) {
		case MODE_HISTORY:
			if (history_hint_mode() < 0)
				goto exit;

			ev = NULL;
			mode = MODE_NORMAL;
			break;
		case MODE_HINTSPEC:
			hintspec_mode();
			break;
		case MODE_NORMAL:
			ev = normal_mode(ev, oneshot, dah);

			if (config_input_match(ev, "history"))
				mode = MODE_HISTORY;
			else if (config_input_match(ev, "hint"))
				mode = MODE_HINT;
			else if (config_input_match(ev, "hint2"))
				mode = MODE_HINT2;
			else if (config_input_match(ev, "hint_near_top_left")) {
				mode = MODE_HINT_NEAR;
				nearMode = TOP_LEFT;
			} else if (config_input_match(ev, "hint_near_top_right")) {
				mode = MODE_HINT_NEAR;
				nearMode = TOP_RIGHT;
			} else if (config_input_match(ev, "hint_near_bottom_left")) {
				mode = MODE_HINT_NEAR;
				nearMode = BOTTOM_LEFT;
			} else if (config_input_match(ev, "hint_near_bottom_right")) {
				mode = MODE_HINT_NEAR;
				nearMode = BOTTOM_RIGHT;
			} else if (config_input_match(ev, "hint_near_vertical_up")) {
				mode = MODE_HINT_VERTICAL;
				nearMode = TOP_LEFT; // it could be TOP_RIGH
			} else if (config_input_match(ev, "hint_near_vertical_down")) {
				mode = MODE_HINT_VERTICAL;
				nearMode = BOTTOM_LEFT; // it could be BOTTOM_LEFT
			} else if (config_input_match(ev, "grid"))
				mode = MODE_GRID;
			else if (config_input_match(ev, "screen"))
				mode = MODE_SCREEN_SELECTION;
			else if ((rc = config_input_match(ev, "oneshot_buttons")) || !ev) {
				goto exit;
			}
			else if (config_input_match(ev, "exit") || !ev) {
				rc = 0;
				goto exit;
			}

			break;
		case MODE_HINT2:
		case MODE_HINT:
			if (full_hint_mode(mode == MODE_HINT2) < 0)
				goto exit;

			ev = NULL;
			mode = MODE_NORMAL;
			break;
		case MODE_HINT_NEAR:
			if (hint_near_cursor_mode(nearMode) < 0)
				goto exit;

			ev = NULL;
			mode = MODE_NORMAL;
			break;
		case MODE_HINT_VERTICAL:
			if (hint_vertical_cursor_mode(nearMode) < 0)
				goto exit;

			ev = NULL;
			mode = MODE_NORMAL;
			break;
		case MODE_GRID:
			ev = grid_mode();
			if (config_input_match(ev, "grid_exit"))
				ev = NULL;
			mode = MODE_NORMAL;
			break;
		case MODE_SCREEN_SELECTION:
			screen_selection_mode();
			mode = MODE_NORMAL;
			ev = NULL;
			break;
		}

		if (!oneshotRemoved && oneshot && (initial_mode != MODE_NORMAL || (btn = config_input_match(ev, "buttons")))) {
			int x, y;
			screen_t scr;

			platform->mouse_get_position(&scr, NULL, NULL);
			platform->mouse_get_position(NULL, &x, &y);

			if (record_history)
				histfile_add(x, y);

			if (mode == MODE_HINTSPEC)
				printf("%d %d %s\n", x, y, last_selected_hint);
			else
				printf("%d %d\n", x, y);

			return btn;
		}
	}

exit:
	return rc;
}

