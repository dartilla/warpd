/*
 * warpd - A modal keyboard-driven pointing system.
 *
 * © 2019 Raheman Vaiya (see: LICENSE).
 */

#include "warpd.h"
#include "ctype.h"

struct hint *hints;
struct hint matched[MAX_HINTS];

static size_t nr_hints;
static size_t nr_matched;

char last_selected_hint[32];

int is_hint_overrides_cursor(struct hint hint, int x, int y)
{
	return hint.x < x && hint.x + hint.w > x
		&& hint.y < y && hint.y + hint.h > y;
}

static char uppercase_hint_char(char c)
{
	const char *exclude = config_get("hint_chars_uppercase_exclude");

	if (config_get_int("hint_chars_uppercase") && !strchr(exclude, c))
		return toupper((unsigned char)c);

	return c;
}

static void filter(screen_t scr, const char *s)
{
	size_t i;

	char *upper = NULL;
	const char *key = s;

	if (config_get_int("hint_chars_uppercase")) {
		upper = strdup(s);
		for (int j = 0; upper[j]; j++) {
			upper[j] = uppercase_hint_char(upper[j]);
		}
		key = upper;
	}

	nr_matched = 0;
	int x, y;
	platform->mouse_get_position(&scr, &x, &y);
	for (i = 0; i < nr_hints; i++) {
		if (strstr(hints[i].label, key) == hints[i].label) {
			if (config_get_int("hint_overriding_cursor_hide")) {
				if (!is_hint_overrides_cursor(hints[i], x, y)) {
					matched[nr_matched++] = hints[i];
				}
			} else {
				matched[nr_matched++] = hints[i];
			}
		}
	}

	free(upper);
	platform->screen_clear(scr);
	draw_cursor_current(scr);
	platform->hint_draw(scr, matched, nr_matched);
	platform->commit();
}

static void get_hint_size_by_key(screen_t scr, int *w, int *h, const char *hint_size_key)
{
	int sw, sh;

	platform->screen_get_dimensions(scr, &sw, &sh);

	if (sw < sh) {
		int tmp = sw;
		sw = sh;
		sh = tmp;
	}

	*w = (sw * config_get_int(hint_size_key)) / 1000;
	*h = (sh * config_get_int(hint_size_key)) / 1000;
}

static void get_hint_size(screen_t scr, int *w, int *h)
{
	return get_hint_size_by_key(scr, w, h, "hint_size");
}

static int is_skip_element(int i, int j, const int single_row_or_column_mode,
	const int alignToScreenEdge)
{
	if (alignToScreenEdge) {
		return 0; // do not skip
	}

	if (single_row_or_column_mode) {
		return (i == 0 && j == 0); // skip first element
	} else {
		return (i == 0 || j == 0); // skip first row and column
	}
}

static int checkAlignScreenEdgeForbidden(int row_count, int column_count,
					   int cursor_x, int cursor_y,
					   int hint_width,
					   int hint_height, const int colGap,
					   const int rowGap, int x_offset,
					   int y_offset, int directionX,
					   int directionY, int alignVertical)
{
	int i, j;
	int isAlignToScreenEdgeForbidden = 0;
	int x = x_offset;
	int y = y_offset;

	for (i = 0; i < column_count; i++) {
		for (j = 0; j < row_count; j++) {
			if (directionY > 0) {
				y += rowGap + hint_height;
			} else {
				y -= rowGap + hint_height;
			}
		}
		if (directionX > 0) {
			x += colGap + hint_width;
		} else {
			x -= colGap + hint_width;
		}
	}
	if (alignVertical) {
		if (directionY > 0) {
			isAlignToScreenEdgeForbidden = cursor_y < y;
		} else if (directionY < 0) {
			isAlignToScreenEdgeForbidden = cursor_y > y;
		}
	} else {
		if (directionX > 0) {
			isAlignToScreenEdgeForbidden = cursor_x < x;
		} else if (directionX < 0) {
			isAlignToScreenEdgeForbidden = cursor_x > x;
		}
	}
	return isAlignToScreenEdgeForbidden;
}

static size_t generate_hints_near_cursor(screen_t scr, struct hint *hints,
	int mode, int row_count, int column_count, int alignScreenEdge)
{
	int screen_width, screen_height, cursor_x, cursor_y;
	int hint_width, hint_height;
	int i, j;
	size_t n = 0;

	const char *chars = config_get("hint_near_chars");
	get_hint_size_by_key(scr, &hint_width, &hint_height, "hint_near_size");
	platform->screen_get_dimensions(scr, &screen_width, &screen_height);
	platform->mouse_get_position(&scr, &cursor_x, &cursor_y);


	const int colGap = hint_width / 4;
	const int rowGap = hint_height / 4;
	const int cursor_size = config_get_int("cursor_size");

	int x_offset = cursor_x - hint_width / 2 + cursor_size / 2 + 2;
	int y_offset = cursor_y - hint_height / 2;

	// mode == BOTTOM_RIGHT
	int directionX = 1;
	int directionY = 1;
	if (mode == TOP_LEFT) {
		directionY = -1;
		directionX = -1;
	} else if (mode == TOP_RIGHT) {
		directionY = -1;
		directionX = 1;
	} else if (mode == BOTTOM_LEFT) {
		directionY = 1;
		directionX = -1;
	}

	if (alignScreenEdge) {
		const int x_offset_bak = x_offset;
		const int y_offset_bak = y_offset;
		const int directionX_bak = directionX;
		const int directionY_bak = directionY;
		const int column_count_bak = column_count;
		const int row_count_bak = row_count;

		const int alignVertical = (column_count == 1);

		if (alignVertical) {
			if (mode == TOP_LEFT || mode == TOP_RIGHT) {
				// max top position
				y_offset = 0;
				directionY = 1;
			} else {
				// max bottom position
				y_offset = screen_height - hint_height;
				directionY = -1;
			}
			row_count--;
		} else {
			if (mode == TOP_RIGHT || mode == BOTTOM_RIGHT) {
				// max right position
				x_offset = screen_width - hint_width;
				directionX = -1;
			} else {
				// max left position
				x_offset = 0;
				directionX = 1;
			}
			column_count--;
		}

		const int isAlignScreenEdgeForbidden = checkAlignScreenEdgeForbidden(
		    row_count, column_count, cursor_x, cursor_y, hint_width,
		    hint_height, colGap, rowGap, x_offset, y_offset, directionX,
		    directionY, alignVertical);

		if (isAlignScreenEdgeForbidden) {
			x_offset = x_offset_bak;
			y_offset = y_offset_bak;
			directionX = directionX_bak;
			directionY = directionY_bak;
			column_count = column_count_bak;
			row_count = row_count_bak;
			alignScreenEdge = 0;
		}
	}

	int x = x_offset;
	int y = y_offset;

	int k = 0;
	int single_row_or_column_mode;
	if (row_count == 1 || column_count == 1) {
		single_row_or_column_mode = 1;
	} else {
		single_row_or_column_mode = 0;
		// to move elements next row/columns
		column_count = column_count + 1;
		row_count = row_count + 1;
	}

	// total_count used with alignScreenEdge, so it is column_count or row_count
	int total_count = column_count * row_count;
	for (i = 0; i < column_count; i++) {
		for (j = 0; j < row_count; j++) {
			if (!is_skip_element(i, j, single_row_or_column_mode, alignScreenEdge)) {
				struct hint *hint = &hints[n++];

				hint->x = x;
				hint->y = y;

				hint->w = hint_width;
				hint->h = hint_height;

				int label_k = (alignScreenEdge) ? (total_count - 1 - k) : k;
				hint->label[0] = uppercase_hint_char(chars[label_k]);
				hint->label[1] = 0;
				hint->label[2] = 0;
				k++;
			}

			if (directionY > 0) {
				y += rowGap + hint_height;
			} else {
				y -= rowGap + hint_height;
			}
		}

		y = y_offset;
		if (directionX > 0) {
			x += colGap + hint_width;
		} else {
			x -= colGap + hint_width;
		}
	}

	return n;
}

static size_t generate_fullscreen_hints(screen_t scr, struct hint *hints)
{
	int sw, sh;
	int w, h;
	int i, j;
	size_t n = 0;

	const char *chars = config_get("hint_chars");
	get_hint_size(scr, &w, &h);
	platform->screen_get_dimensions(scr, &sw, &sh);

	const int nr = strlen(chars);
	const int nc = strlen(chars);


	const int colgap = sw / nc - w;
	const int rowgap = sh / nr - h;

	const int x_offset = (sw - nc * w - (nc - 1) * colgap) / 2;
	const int y_offset = (sh - nr * h - (nr - 1) * rowgap) / 2;

	int x = x_offset;
	int y = y_offset;

	get_hint_size(scr, &w, &h);

	for (i = 0; i < nc; i++) {
		for (j = 0; j < nr; j++) {
			struct hint *hint = &hints[n++];

			hint->x = x;
			hint->y = y;

			hint->w = w;
			hint->h = h;

			hint->label[0] = uppercase_hint_char(chars[i]);
			hint->label[1] = uppercase_hint_char(chars[j]);
			hint->label[2] = 0;

			y += rowgap + h;
		}

		y = y_offset;
		x += colgap + w;
	}

	return n;
}

static int hint_selection(screen_t scr, struct hint *_hints, size_t _nr_hints)
{
	hints = _hints;
	nr_hints = _nr_hints;

	filter(scr, "");

	int rc = 0;
	char buf[32] = {0};
	platform->input_grab_keyboard();

	platform->mouse_hide();

	const char *keys[] = {
		"hint_exit",
		"hint_undo_all",
		"hint_undo",
		"hint_normal",
		"hint_near_top_left",
		"hint_near_top_right",
		"hint_near_bottom_left",
		"hint_near_bottom_right",
		"hint_near_vertical_up",
		"hint_near_vertical_down",
		"hint_near_vertical_up_align_screen_edge",
		"hint_near_vertical_down_align_screen_edge",
		"hint_near_horizon_left",
		"hint_near_horizon_left_align_screen_edge",
		"hint_near_horizon_right",
		"hint_near_horizon_right_align_screen_edge"
	};

	config_input_whitelist(keys, sizeof keys / sizeof keys[0]);

	while (1) {
		struct input_event *ev;
		ssize_t len;

		ev = platform->input_next_event(0);

		if (!ev->pressed)
			continue;

		len = strlen(buf);

		if (config_input_match(ev, "hint_exit")) {
			rc = -1;
			break;
		} else if (config_input_match(ev, "hint_near_top_left")) {
			platform->screen_clear(scr);
			rc = hint_near_cursor_mode(TOP_LEFT);
			break;
		} else if (config_input_match(ev, "hint_near_top_right")) {
			platform->screen_clear(scr);
			rc = hint_near_cursor_mode(TOP_RIGHT);
			break;
		} else if (config_input_match(ev, "hint_near_bottom_left")) {
			platform->screen_clear(scr);
			rc = hint_near_cursor_mode(BOTTOM_LEFT);
			break;
		} else if (config_input_match(ev, "hint_near_bottom_right")) {
			platform->screen_clear(scr);
			rc = hint_near_cursor_mode(BOTTOM_RIGHT);
			break;
		} else if (config_input_match(ev, "hint_near_vertical_up")) {
			platform->screen_clear(scr);
			rc = hint_vertical_cursor_mode(TOP_LEFT, 0);
			break;
		} else if (config_input_match(ev, "hint_near_vertical_down")) {
			platform->screen_clear(scr);
			rc = hint_vertical_cursor_mode(BOTTOM_LEFT, 0);
			break;
		} else if (config_input_match(ev, "hint_near_vertical_up_align_screen_edge")) {
			platform->screen_clear(scr);
			rc = hint_vertical_cursor_mode(TOP_LEFT, 1);
			break;
		} else if (config_input_match(ev, "hint_near_vertical_down_align_screen_edge")) {
			platform->screen_clear(scr);
			rc = hint_vertical_cursor_mode(BOTTOM_LEFT, 1);
			break;
		} else if (config_input_match(ev, "hint_near_horizon_left")) {
			platform->screen_clear(scr);
			rc = hint_horizon_cursor_mode(TOP_LEFT, 0);
			break;
		} else if (config_input_match(ev, "hint_near_horizon_left_align_screen_edge")) {
			platform->screen_clear(scr);
			rc = hint_horizon_cursor_mode(TOP_LEFT, 1);
			break;
		} else if (config_input_match(ev, "hint_near_horizon_right")) {
			platform->screen_clear(scr);
			rc = hint_horizon_cursor_mode(TOP_RIGHT, 0);
			break;
		} else if (config_input_match(ev, "hint_near_horizon_right_align_screen_edge")) {
			platform->screen_clear(scr);
			rc = hint_horizon_cursor_mode(TOP_RIGHT, 1);
			break;
		} else if (config_input_match(ev, "hint_normal")) {
			remove_oneshot_flag();
			rc = 0;
			break;
		} else if (config_input_match(ev, "hint_undo_all")) {
			buf[0] = 0;
		} else if (config_input_match(ev, "hint_undo")) {
			if (len)
				buf[len - 1] = 0;
		} else {
			const char *name = input_event_tostr(ev);

			if (!name || name[1])
				continue;

			buf[len++] = name[0];
		}

		filter(scr, buf);

		if (nr_matched == 1) {
			int nx, ny;
			struct hint *h = &matched[0];

			platform->screen_clear(scr);

			nx = h->x + h->w / 2;
			ny = h->y + h->h / 2;

			/*
			 * Wiggle the cursor a single pixel to accommodate
			 * text selection widgets which don't like spontaneous
			 * cursor warping.
			 */
			platform->mouse_move(scr, nx+1, ny+1);

			platform->mouse_move(scr, nx, ny);
			strcpy(last_selected_hint, buf);
			break;
		} else if (nr_matched == 0) {
			break;
		}
	}

	platform->input_ungrab_keyboard();
	platform->screen_clear(scr);
	platform->mouse_show();

	platform->commit();
	return rc;
}

static int sift()
{
	int gap = config_get_int("hint2_gap_size");
	int hint_sz = config_get_int("hint2_size");

	const char *chars = config_get("hint2_chars");
	size_t chars_len= strlen(chars);

	int grid_sz = config_get_int("hint2_grid_size");

	int x, y;
	int sh, sw;

	int col;
	int row;
	size_t n = 0;
	screen_t scr;

	struct hint hints[MAX_HINTS];

	platform->mouse_get_position(&scr, &x, &y);
	platform->screen_get_dimensions(scr, &sw, &sh);

	gap = (gap * sh) / 1000;
	hint_sz = (hint_sz * sh) / 1000;

	x -= ((hint_sz + (gap - 1)) * grid_sz) / 2;
	y -= ((hint_sz + (gap - 1)) * grid_sz) / 2;

	for (col = 0; col < grid_sz; col++)
		for (row = 0; row < grid_sz; row++) {
			size_t idx = (row * grid_sz) + col;

			if (idx < chars_len) {
				hints[n].x = x + (hint_sz + gap) * col;
				hints[n].y = y + (hint_sz + gap) * row;

				hints[n].w = hint_sz;
				hints[n].h = hint_sz;
				hints[n].label[0] = chars[idx];
				hints[n].label[1] = 0;

				n++;
			}
	}

	return hint_selection(scr, hints, n);
}

void init_hints()
{
	platform->init_hint(config_get("hint_bgcolor"),
			    config_get("hint_fgcolor"),
			    config_get_int("hint_border_radius"),
			    config_get("hint_font"));
}

int hintspec_mode()
{
	screen_t scr;
	int sw, sh;
	int w, h;

	int n = 0;
	struct hint hints[MAX_HINTS];

	platform->mouse_get_position(&scr, NULL, NULL);
	platform->screen_get_dimensions(scr, &sw, &sh);

	get_hint_size(scr, &w, &h);

	while (scanf("%15s %d %d",
		hints[n].label,
		&hints[n].x,
		&hints[n].y) == 3) {

		hints[n].w = w;
		hints[n].h = h;
		hints[n].x -= w/2;
		hints[n].y -= h/2;

		n++;
	}

	return hint_selection(scr, hints, n);
}

int full_hint_mode(int second_pass)
{
	int mx, my;
	screen_t scr;
	struct hint hints[MAX_HINTS];

	platform->mouse_get_position(&scr, &mx, &my);
	hist_add(mx, my);

	nr_hints = generate_fullscreen_hints(scr, hints);

	if (hint_selection(scr, hints, nr_hints))
		return -1;

	if (second_pass)
		return sift();
	else
		return 0;
}

int hint_near_cursor_mode_full(int mode, int row_count, int column_count, int alignScreenEdge)
{
	int mx, my;
	screen_t scr;
	struct hint hints[MAX_HINTS];

	platform->mouse_get_position(&scr, &mx, &my);
	hist_add(mx, my);

	nr_hints = generate_hints_near_cursor(scr, hints, mode, row_count, column_count, alignScreenEdge);

	if (hint_selection(scr, hints, nr_hints))
		return -1;

	return 0;
}

int hint_near_cursor_mode(int mode)
{
	return hint_near_cursor_mode_full(mode,
		config_get_int("hint_near_row_count"),
		config_get_int("hint_near_column_count"), 0);
}

int hint_vertical_cursor_mode(int mode, int alignScreenEdge)
{
	return hint_near_cursor_mode_full(mode,
		config_get_int("hint_near_vertical_row_count"), 1, alignScreenEdge);
}

int hint_horizon_cursor_mode(int mode, int alignScreenEdge)
{
	return hint_near_cursor_mode_full(mode, 1,
		config_get_int("hint_near_horizon_column_count"), alignScreenEdge);
}

int history_hint_mode()
{
	struct hint hints[MAX_HINTS];
	struct histfile_ent *ents;
	screen_t scr;
	int w, h;
	int sw, sh;
	size_t n, i;

	platform->mouse_get_position(&scr, NULL, NULL);
	platform->screen_get_dimensions(scr, &sw, &sh);

	n = histfile_read(&ents);

	get_hint_size(scr, &w, &h);

	for (i = 0; i < n; i++) {
		hints[i].w = w;
		hints[i].h = h;

		hints[i].x = ents[i].x - w/2;
		hints[i].y = ents[i].y - h/2;

		hints[i].label[0] = uppercase_hint_char('a'+i);

		hints[i].label[1] = 0;
	}

	return hint_selection(scr, hints, n);
}

static const char *get_bookmark_path()
{
	static char buf[PATH_MAX];
	const char *script = config_get("bookmarkfile_choose_script");
	FILE *fp;
	size_t len;

	if (script[0]) {
		fp = popen(script, "r");
		if (!fp)
			return get_data_path("bookmark");

		if (!fgets(buf, sizeof(buf), fp)) {
			pclose(fp);
			return get_data_path("bookmark");
		}

		pclose(fp);

		len = strlen(buf);
		while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r'))
			buf[--len] = 0;

		if (len == 0)
			return get_data_path("bookmark");

		return get_data_path(buf);
	}

	return get_data_path("bookmark");
}

int bookmark_hint_mode()
{
	struct hint hints[MAX_HINTS];
	struct bookmark_ent *ents;
	screen_t scr;
	int w, h;
	int sw, sh;
	size_t n, i;
	const char *path;

	platform->mouse_get_position(&scr, NULL, NULL);
	platform->screen_get_dimensions(scr, &sw, &sh);

	path = get_bookmark_path();
	n = bookmarkfile_read(path, &ents);

	get_hint_size(scr, &w, &h);

	for (i = 0; i < n; i++) {
		hints[i].w = w;
		hints[i].h = h;

		hints[i].x = ents[i].x - w/2;
		hints[i].y = ents[i].y - h/2;

		strncpy(hints[i].label, ents[i].label, sizeof(hints[i].label) - 1);
		hints[i].label[sizeof(hints[i].label) - 1] = 0;
	}

	return hint_selection(scr, hints, n);
}
