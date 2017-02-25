/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2017 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_topbar/space_topbar.c
 *  \ingroup sptopbar
 */


#include <string.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "ED_screen.h"
#include "ED_space_api.h"

#include "WM_api.h"
#include "WM_types.h"


/* ******************** default callbacks for topbar space ***************** */

static SpaceLink *topbar_new(const bContext *UNUSED(C))
{
	ARegion *ar;
	SpaceTopBar *stopbar;

	stopbar = MEM_callocN(sizeof(*stopbar), "init topbar");
	stopbar->spacetype = SPACE_TOPBAR;

	/* header */
	ar = MEM_callocN(sizeof(ARegion), "header for topbar");

	BLI_addtail(&stopbar->regionbase, ar);
	ar->regiontype = RGN_TYPE_HEADER;
	ar->alignment = RGN_ALIGN_BOTTOM;

	/* main region */
	ar = MEM_callocN(sizeof(ARegion), "main region for topbar");

	BLI_addtail(&stopbar->regionbase, ar);
	ar->regiontype = RGN_TYPE_WINDOW;

	return (SpaceLink *)stopbar;
}

/* not spacelink itself */
static void topbar_free(SpaceLink *UNUSED(sl))
{
	
}


/* spacetype; init callback */
static void topbar_init(struct wmWindowManager *UNUSED(wm), ScrArea *UNUSED(sa))
{
	
}

static SpaceLink *topbar_duplicate(SpaceLink *sl)
{
	SpaceTopBar *stopbarn = MEM_dupallocN(sl);

	/* clear or remove stuff from old */

	return (SpaceLink *)stopbarn;
}



/* add handlers, stuff you only do once or on area/region changes */
static void topbar_main_region_init(wmWindowManager *wm, ARegion *ar)
{
	ar->v2d.scroll = V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HIDE;

	ED_region_panels_init(wm, ar);
}

static void topbar_main_region_draw(const bContext *C, ARegion *ar)
{
	ED_region_panels(C, ar, NULL, -1, true);
}

static void topbar_operatortypes(void)
{
	
}

static void topbar_keymap(struct wmKeyConfig *UNUSED(keyconf))
{
	
}

/* add handlers, stuff you only do once or on area/region changes */
static void topbar_header_region_init(wmWindowManager *UNUSED(wm), ARegion *ar)
{
	ED_region_header_init(ar);
}

static void topbar_header_region_draw(const bContext *C, ARegion *ar)
{
	ED_region_header(C, ar);
}

static void topbar_main_region_listener(bScreen *UNUSED(sc), ScrArea *UNUSED(sa), ARegion *UNUSED(ar),
                                        wmNotifier *UNUSED(wmn), const Scene *UNUSED(scene))
{
	/* context changes */
}

static void topbar_header_listener(bScreen *UNUSED(sc), ScrArea *UNUSED(sa), ARegion *UNUSED(ar),
                                   wmNotifier *UNUSED(wmn), const Scene *UNUSED(scene))
{
	/* context changes */
#if 0
	switch (wmn->category) {
		default:
			break;
	}
#endif
}

/* only called once, from space/spacetypes.c */
void ED_spacetype_topbar(void)
{
	SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype topbar");
	ARegionType *art;

	st->spaceid = SPACE_TOPBAR;
	strncpy(st->name, "Top Bar", BKE_ST_MAXNAME);

	st->new = topbar_new;
	st->free = topbar_free;
	st->init = topbar_init;
	st->duplicate = topbar_duplicate;
	st->operatortypes = topbar_operatortypes;
	st->keymap = topbar_keymap;

	/* regions: main window */
	art = MEM_callocN(sizeof(ARegionType), "spacetype topbar main region");
	art->regionid = RGN_TYPE_WINDOW;
	art->init = topbar_main_region_init;
	art->draw = topbar_main_region_draw;
	art->listener = topbar_main_region_listener;
	art->keymapflag = ED_KEYMAP_UI;

	BLI_addhead(&st->regiontypes, art);

	/* regions: header */
	art = MEM_callocN(sizeof(ARegionType), "spacetype topbar header region");
	art->regionid = RGN_TYPE_HEADER;
	art->prefsizey = HEADERY;
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;
	art->listener = topbar_header_listener;
	art->init = topbar_header_region_init;
	art->draw = topbar_header_region_draw;

	BLI_addhead(&st->regiontypes, art);


	BKE_spacetype_register(st);
}
