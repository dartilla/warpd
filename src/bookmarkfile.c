#include "warpd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct {
	int sz;
	struct bookmark_ent ents[MAX_BOOKMARK_ENTS];
} bookmarks;

size_t bookmarkfile_read(const char *path, struct bookmark_ent **entries)
{
	FILE *fh;
	char line[256];

	bookmarks.sz = 0;

	fh = fopen(path, "r");
	if (!fh) {
		*entries = bookmarks.ents;
		return 0;
	}

	while (fgets(line, sizeof(line), fh) && bookmarks.sz < MAX_BOOKMARK_ENTS) {
		char label[16];
		int x, y;

		if (line[0] == '#' || line[0] == '\n')
			continue;

		if (sscanf(line, "%15s %d %d", label, &x, &y) == 3) {
			strncpy(bookmarks.ents[bookmarks.sz].label, label,
				sizeof(bookmarks.ents[bookmarks.sz].label) - 1);
			bookmarks.ents[bookmarks.sz].label[sizeof(bookmarks.ents[bookmarks.sz].label) - 1] = 0;
			bookmarks.ents[bookmarks.sz].x = x;
			bookmarks.ents[bookmarks.sz].y = y;
			bookmarks.sz++;
		}
	}

	fclose(fh);

	*entries = bookmarks.ents;
	return bookmarks.sz;
}
