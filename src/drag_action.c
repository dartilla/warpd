/*
 * warpd - A modal keyboard-driven pointing system.
 *
 * © 2019 Raheman Vaiya (see: LICENSE).
 */

#include "warpd.h"

const char *active_drag_cursor_color = NULL;

void init_drag_action_holder(struct drag_action_holder *dah)
{
	dah->nr = 0;
	for (int i = 0; i < MAX_DRAG_ACTIONS; i++) {
		struct drag_action *da = &dah->drag_actions[i];
		snprintf(da->trigger_cl, sizeof da->trigger_cl, "drag_action%d", i + 1);
		snprintf(da->button_cl, sizeof da->button_cl, "%s_button", da->trigger_cl);
		snprintf(da->modifiers_cl, sizeof da->modifiers_cl, "%s_button_modifiers", da->trigger_cl);
		snprintf(da->cursor_color_cl, sizeof da->cursor_color_cl, "%s_cursor_color", da->trigger_cl);

		int found = 0;
		struct config_entry *ent;
		for (ent = config; ent; ent = ent->next)
			if (!strcmp(ent->key, da->trigger_cl)) {
				found = 1;
				break;
			}
		if (!found)
			break;

		dah->nr++;
	}
}

static void stop_other_drag_actions(struct drag_action_holder *dah, int skip_pos)
{
	for (int i = 0; i < dah->nr; i++) {
		if (i == skip_pos) continue;

		struct drag_action *da = &dah->drag_actions[i];
		da->is_dragging = 0;
		platform->unpress_modifier(parse_modifiers(config_get(da->modifiers_cl)));
		platform->mouse_up(config_get_int(da->button_cl));
	}
}

void stop_all_drag_actions(struct drag_action_holder *dah)
{
	active_drag_cursor_color = NULL;
	for (int i = 0; i < dah->nr; i++) {
		if (dah->drag_actions[i].is_dragging) {
			struct drag_action *da = &dah->drag_actions[i];
			da->is_dragging = 0;
			platform->unpress_modifier(parse_modifiers(config_get(da->modifiers_cl)));
			platform->mouse_up(config_get_int(da->button_cl));
		}
	}
}

int handle_drag_action(struct input_event *ev, struct drag_action_holder *dah, screen_t *scr)
{
	for (int i = 0; i < dah->nr; i++) {
		struct drag_action *da = &dah->drag_actions[i];
		if (config_input_match(ev, da->trigger_cl)) {
			uint8_t mod = parse_modifiers(config_get(da->modifiers_cl));
			da->is_dragging = !da->is_dragging;
			char *dragging_now_cursor_color = NULL;
			if (da->is_dragging) {
				stop_other_drag_actions(dah, i);
				if (config_is_key_exist(da->cursor_color_cl)) {
					dragging_now_cursor_color = config_get(da->cursor_color_cl);
				} else {
					dragging_now_cursor_color = config_get("drag_action_cursor_color");
				}
				platform->press_modifier(mod);
				platform->mouse_down(config_get_int(da->button_cl));
			} else {
				active_drag_cursor_color = NULL;
				platform->unpress_modifier(mod);
				platform->mouse_up(config_get_int(da->button_cl));
			}
			if (dragging_now_cursor_color != NULL) {
				active_drag_cursor_color = dragging_now_cursor_color;
			}
			int mx, my;
			platform->mouse_get_position(scr, &mx, &my);
			hist_add(mx, my);
			histfile_add(mx, my);
			return 1;
		}
	}
	return 0;
}