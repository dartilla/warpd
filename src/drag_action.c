/*
 * warpd - A modal keyboard-driven pointing system.
 *
 * © 2019 Raheman Vaiya (see: LICENSE).
 */

#include "warpd.h"

void init_drag_action_holder(struct drag_action_holder *dah)
{
	dah->nr = 0;
	for (int i = 0; i < MAX_DRAG_ACTIONS; i++) {
		struct drag_action *da = &dah->drag_actions[i];
		snprintf(da->trigger_cl, sizeof da->trigger_cl, "drag_action%d", i + 1);
		snprintf(da->button_cl, sizeof da->button_cl, "%s_button", da->trigger_cl);
		snprintf(da->modifiers_cl, sizeof da->modifiers_cl, "%s_button_modifiers", da->trigger_cl);

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

static uint8_t parse_drag_modifiers(const char *s)
{
	uint8_t mod = 0;
	for (; *s; s++) {
		switch (*s) {
		case 'A': mod |= PLATFORM_MOD_ALT; break;
		case 'C': mod |= PLATFORM_MOD_CONTROL; break;
		case '-': break;
		default: break;
		}
	}
	return mod;
}

static void stop_other_drag_actions(struct drag_action_holder *dah, int skip_pos)
{
	for (int i = 0; i < dah->nr; i++) {
		if (i == skip_pos) continue;

		struct drag_action *da = &dah->drag_actions[i];
		da->is_dragging = 0;
		platform->unpress_modifier(parse_drag_modifiers(config_get(da->modifiers_cl)));
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
			platform->unpress_modifier(parse_drag_modifiers(config_get(da->modifiers_cl)));
			platform->mouse_up(config_get_int(da->button_cl));
		}
	}
}

int handle_drag_action(struct input_event *ev, struct drag_action_holder *dah)
{
	for (int i = 0; i < dah->nr; i++) {
		struct drag_action *da = &dah->drag_actions[i];
		if (config_input_match(ev, da->trigger_cl)) {
			uint8_t mod = parse_drag_modifiers(config_get(da->modifiers_cl));
			da->is_dragging = !da->is_dragging;
			if (da->is_dragging) {
				stop_other_drag_actions(dah, i);
				platform->press_modifier(mod);
				platform->mouse_down(config_get_int(da->button_cl));
			} else {
				platform->unpress_modifier(mod);
				platform->mouse_up(config_get_int(da->button_cl));
			}
			return 1;
		}
	}
	return 0;
}