/*
 * warpd - A modal keyboard-driven pointing system.
 *
 * © 2019 Raheman Vaiya (see: LICENSE).
 */

#ifndef DRAG_ACTION_H
#define DRAG_ACTION_H

#include "platform.h"

// 'drag' means mouse button pressed constantly
struct drag_action {
	int is_dragging; // is currently dragging
	char trigger_cl[32]; // key to start drag action config label
	char button_cl[32]; // mouse button pressed while dragging config label
	char modifiers_cl[32]; // modifiers to press while dragging config label
	char cursor_color_cl[48]; // cursor color while dragging config label
};

struct drag_action_holder {
	struct drag_action drag_actions[MAX_DRAG_ACTIONS];
	int nr;
};

extern const char *active_drag_cursor_color;

void init_drag_action_holder(struct drag_action_holder *dah);
int handle_drag_action(struct input_event *ev, struct drag_action_holder *dah, screen_t *scr);
void stop_all_drag_actions(struct drag_action_holder *dah, screen_t *scr);
void pause_active_drag_action(struct drag_action_holder *dah);
void resume_active_drag_action(struct drag_action_holder *dah);

#endif
