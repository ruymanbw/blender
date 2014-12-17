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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */


#include "DNA_object_types.h"
#include "DNA_mesh_types.h"

#include "WM_types.h"
#include "WM_api.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_editmesh.h"
#include "BKE_object.h"
#include "BKE_object_deform.h"

#include "BKE_depsgraph.h"

#include "BLI_utildefines.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_listbase.h"

#include "BLF_translation.h"

#include "MEM_guardedalloc.h"

#include "ED_mesh.h"
#include "ED_object.h"

#include "RNA_define.h"
#include "RNA_access.h"

#include <string.h>

#include "object_intern.h"


static bool fmap_unique_check(void *arg, const char *name)
{
	struct {Object *ob; void *fm; } *data = arg;
	
	bFaceMap *fmap;

	for (fmap = data->ob->fmaps.first; fmap; fmap = fmap->next) {
		if (data->fm != fmap) {
			if (!strcmp(fmap->name, name)) {
				return true;
			}
		}
	}
	
	return false;
}

static bFaceMap *fmap_duplicate(bFaceMap *infmap)
{
	bFaceMap *outfmap;

	if (!infmap)
		return NULL;

	outfmap = MEM_callocN(sizeof(bFaceMap), "copy facemap");

	/* For now, just copy everything over. */
	memcpy(outfmap, infmap, sizeof(bFaceMap));

	outfmap->next = outfmap->prev = NULL;

	return outfmap;
}

void fmap_copy_list(ListBase *outbase, ListBase *inbase)
{
	bFaceMap *fmap, *fmapn;

	BLI_listbase_clear(outbase);

	for (fmap = inbase->first; fmap; fmap = fmap->next) {
		fmapn = fmap_duplicate(fmap);
		BLI_addtail(outbase, fmapn);
	}
}

void fmap_unique_name(bFaceMap *fmap, Object *ob)
{
	struct {Object *ob; void *fmap; } data;
	data.ob = ob;
	data.fmap = fmap;

	BLI_uniquename_cb(fmap_unique_check, &data, DATA_("Group"), '.', fmap->name, sizeof(fmap->name));
}


/* called while not in editmode */
void ED_fmap_face_add(Object *ob, bFaceMap *fmap, int facenum)
{
	int fmap_nr;
	if (GS(((ID *)ob->data)->name) != ID_ME)
		return;
	
	/* get the face map number, exit if it can't be found */
	fmap_nr = BLI_findindex(&ob->fmaps, fmap);

	if (fmap_nr != -1) {
		int *facemap;
		Mesh *me = ob->data;
		
		/* if there's is no facemap layer then create one */
		if ((facemap = CustomData_get_layer(&me->pdata, CD_FACEMAP)) == NULL)
			facemap = CustomData_add_layer(&me->pdata, CD_FACEMAP, CD_DEFAULT, NULL, me->totpoly);

		facemap[facenum] = fmap_nr;
	}
}

/* called while not in editmode */
void ED_fmap_face_remove(Object *ob, bFaceMap *fmap, int facenum)
{
	int fmap_nr;
	if (GS(((ID *)ob->data)->name) != ID_ME)
		return;
	
	/* get the face map number, exit if it can't be found */
	fmap_nr = BLI_findindex(&ob->fmaps, fmap);

	if (fmap_nr != -1) {
		int *facemap;
		Mesh *me = ob->data;
		
		/* if there's is no facemap layer then create one */
		if ((facemap = CustomData_get_layer(&me->pdata, CD_FACEMAP)) == NULL)
			return;
		
		facemap[facenum] = -1;
	}
}

bFaceMap *BKE_object_facemap_add_name(Object *ob, const char *name)
{
	bFaceMap *fmap;
	
	if (!ob || ob->type != OB_MESH)
		return NULL;
	
	fmap = MEM_callocN(sizeof(bFaceMap), __func__);

	BLI_strncpy(fmap->name, name, sizeof(fmap->name));

	BLI_addtail(&ob->fmaps, fmap);
	
	ob->actfmap = BLI_listbase_count(&ob->fmaps);
	
	fmap_unique_name(fmap, ob);

	return fmap;
}

bFaceMap *BKE_object_facemap_add(Object *ob)
{
	return BKE_object_facemap_add_name(ob, DATA_("FaceMap"));
}

static void object_fmap_swap_edit_mode(Object *ob, int num1, int num2)
{
	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;
		
		if (me->edit_btmesh) {
			BMEditMesh *em = me->edit_btmesh;
			const int cd_fmap_offset = CustomData_get_offset(&em->bm->pdata, CD_FACEMAP);
			
			if (cd_fmap_offset != -1) {
				BMFace *efa;
				BMIter iter;
				int *map;
				
				BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
					map = BM_ELEM_CD_GET_VOID_P(efa, cd_fmap_offset);
					
					if (map) {
						if (*map == num1 && num1 != -1)
							*map = num2;
						if (*map == num2 && num2 != -1)
							*map = num1;
					}
				}
			}
		}
	}
}

static void object_fmap_swap_object_mode(Object *ob, int num1, int num2)
{
	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;
		
		if (me->mface) {
			int *map = CustomData_get_layer(&me->pdata, CD_FACEMAP);
			int i;
			
			if (map) {
				for (i = 0; i < me->totpoly; i++) {
					if (map[i] == num1 && num1 != -1)
						map[i] = num2;
					if (map[i]== num2 && num2 != -1)
						map[i] = num1;
				}
			}
		}
	}
}

static void object_facemap_swap(Object *ob, int num1, int num2)
{
	if (BKE_object_is_in_editmode(ob))
		object_fmap_swap_edit_mode(ob, num1, num2);
	else
		object_fmap_swap_object_mode(ob, num1, num2);
}

static void object_fmap_remove_edit_mode(Object *ob, bFaceMap *fmap, bool do_selected)
{
	const int fmap_nr = BLI_findindex(&ob->fmaps, fmap);
	
	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;
		
		if (me->edit_btmesh) {
			BMEditMesh *em = me->edit_btmesh;
			const int cd_fmap_offset = CustomData_get_offset(&em->bm->pdata, CD_FACEMAP);
			
			if (cd_fmap_offset != -1) {
				BMFace *efa;
				BMIter iter;
				int *map;
				
				BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
					map = BM_ELEM_CD_GET_VOID_P(efa, cd_fmap_offset);
					
					if (map && *map == fmap_nr && (!do_selected || BM_elem_flag_test(efa, BM_ELEM_SELECT))) {
						*map = -1;
					}
				}
			}

			if (ob->actfmap == BLI_listbase_count(&ob->fmaps))
				ob->actfmap--;
			
			BLI_remlink(&ob->fmaps, fmap);
			MEM_freeN(fmap);
		}
	}
}

static void object_fmap_remove_object_mode(Object *ob, bFaceMap *fmap)
{
	const int fmap_nr = BLI_findindex(&ob->fmaps, fmap);
	
	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;
		
		if (me->mface) {
			int *map = CustomData_get_layer(&me->pdata, CD_FACEMAP);
			int i;
			
			if (map) {
				for (i = 0; i < me->totpoly; i++) {
					if (map[i] == fmap_nr)
						map[i] = -1;
				}
			}
		}

		if (ob->actfmap == BLI_listbase_count(&ob->fmaps))
			ob->actfmap--;
		
		BLI_remlink(&ob->fmaps, fmap);
		MEM_freeN(fmap);
	}
}

void BKE_object_facemap_remove(Object *ob, bFaceMap *fmap)
{
	if (BKE_object_is_in_editmode(ob))
		object_fmap_remove_edit_mode(ob, fmap, false);
	else
		object_fmap_remove_object_mode(ob, fmap);
}

void BKE_object_fmap_remove_all(Object *ob)
{
	bFaceMap *fmap = (bFaceMap *)ob->fmaps.first;

	if (fmap) {
		const bool edit_mode = BKE_object_is_in_editmode_vgroup(ob);

		while (fmap) {
			bFaceMap *next_fmap = fmap->next;

			if (edit_mode)
				object_fmap_remove_edit_mode(ob, fmap, false);
			else
				object_fmap_remove_object_mode(ob, fmap);

			fmap = next_fmap;
		}
	}
	/* remove all dverts */
	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;
		CustomData_free_layer(&me->pdata, CD_FACEMAP, me->totpoly, 0);
	}
	ob->actfmap = 0;
}

int fmap_name_index(Object *ob, const char *name)
{
	return (name) ? BLI_findstringindex(&ob->fmaps, name, offsetof(bFaceMap, name)) : -1;
}

bFaceMap *fmap_find_name(Object *ob, const char *name)
{
	return BLI_findstring(&ob->fmaps, name, offsetof(bFaceMap, name));
}


static int face_map_supported_poll(bContext *C)
{
	Object *ob = ED_object_context(C);
	ID *data = (ob) ? ob->data : NULL;
	return (ob && !ob->id.lib && ob->type == OB_MESH && data && !data->lib);
}

static int face_map_supported_edit_mode_poll(bContext *C)
{
	Object *ob = ED_object_context(C);
	ID *data = (ob) ? ob->data : NULL;
	return (ob && !ob->id.lib && ob->type == OB_MESH && data && !data->lib && ob->mode == OB_MODE_EDIT);
}

static int face_map_add_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);

	BKE_object_facemap_add(ob);
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_face_map_add(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Face Map";
	ot->idname = "OBJECT_OT_face_map_add";
	ot->description = "Add a new face map to the active object";
	
	/* api callbacks */
	ot->poll = face_map_supported_poll;
	ot->exec = face_map_add_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int face_map_remove_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);
	bFaceMap *fmap = BLI_findlink(&ob->fmaps, ob->actfmap - 1);
	
	if (fmap) {
		BKE_object_facemap_remove(ob, fmap);
		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
		WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	}
	return OPERATOR_FINISHED;
}

void OBJECT_OT_face_map_remove(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Face Map";
	ot->idname = "OBJECT_OT_face_map_remove";
	ot->description = "Remove a new face map to the active object";
	
	/* api callbacks */
	ot->poll = face_map_supported_poll;
	ot->exec = face_map_remove_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int face_map_assign_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);
	bFaceMap *fmap = BLI_findlink(&ob->fmaps, ob->actfmap - 1);
	
	if (fmap) {
		Mesh *me = ob->data;
		BMEditMesh *em = me->edit_btmesh;
		BMFace *efa;
		BMIter iter;
		int *map;
		int cd_fmap_offset;
		
		if (!CustomData_has_layer(&em->bm->pdata, CD_FACEMAP))
			BM_data_layer_add(em->bm, &em->bm->pdata, CD_FACEMAP);
		
		cd_fmap_offset = CustomData_get_offset(&em->bm->pdata, CD_FACEMAP);
		
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			map = BM_ELEM_CD_GET_VOID_P(efa, cd_fmap_offset);
			
			if (BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
				*map = ob->actfmap - 1;
			}
		}
		
		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
		WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	}
	return OPERATOR_FINISHED;
}

void OBJECT_OT_face_map_assign(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Assign Face Map";
	ot->idname = "OBJECT_OT_face_map_assign";
	ot->description = "Assign faces to a face map";
	
	/* api callbacks */
	ot->poll = face_map_supported_edit_mode_poll;
	ot->exec = face_map_assign_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int face_map_remove_from_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);
	bFaceMap *fmap = BLI_findlink(&ob->fmaps, ob->actfmap - 1);
	
	if (fmap) {
		Mesh *me = ob->data;
		BMEditMesh *em = me->edit_btmesh;
		BMFace *efa;
		BMIter iter;
		int *map;
		int cd_fmap_offset;
		int mapindex = ob->actfmap - 1;
		
		if (!CustomData_has_layer(&em->bm->pdata, CD_FACEMAP))
			return OPERATOR_CANCELLED;
		
		cd_fmap_offset = CustomData_get_offset(&em->bm->pdata, CD_FACEMAP);
		
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			map = BM_ELEM_CD_GET_VOID_P(efa, cd_fmap_offset);
			
			if (BM_elem_flag_test(efa, BM_ELEM_SELECT) && *map == mapindex) {
				*map = -1;
			}
		}
		
		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
		WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	}
	return OPERATOR_FINISHED;
}

void OBJECT_OT_face_map_remove_from(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove From Face Map";
	ot->idname = "OBJECT_OT_face_map_remove_from";
	ot->description = "Remove faces from a face map";
	
	/* api callbacks */
	ot->poll = face_map_supported_edit_mode_poll;
	ot->exec = face_map_remove_from_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static void fmap_select(Object *ob, bool select)
{
	Mesh *me = ob->data;
	BMEditMesh *em = me->edit_btmesh;
	BMFace *efa;
	BMIter iter;
	int *map;
	int cd_fmap_offset;
	int mapindex = ob->actfmap - 1;
	
	if (!CustomData_has_layer(&em->bm->pdata, CD_FACEMAP))
		BM_data_layer_add(em->bm, &em->bm->pdata, CD_FACEMAP);
	
	cd_fmap_offset = CustomData_get_offset(&em->bm->pdata, CD_FACEMAP);
	
	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		map = BM_ELEM_CD_GET_VOID_P(efa, cd_fmap_offset);
		
		if (*map == mapindex) {
			BM_face_select_set(em->bm, efa, select);
		}
	}
}

static int face_map_select_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);
	bFaceMap *fmap = BLI_findlink(&ob->fmaps, ob->actfmap - 1);
	
	if (fmap) {
		fmap_select(ob, true);
		
		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
		WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	}
	return OPERATOR_FINISHED;
}

void OBJECT_OT_face_map_select(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Face Map Faces";
	ot->idname = "OBJECT_OT_face_map_select";
	ot->description = "Select faces belonging to a face map";
	
	/* api callbacks */
	ot->poll = face_map_supported_edit_mode_poll;
	ot->exec = face_map_select_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int face_map_deselect_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);
	bFaceMap *fmap = BLI_findlink(&ob->fmaps, ob->actfmap - 1);
	
	if (fmap) {
		fmap_select(ob, false);

		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
		WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	}
	return OPERATOR_FINISHED;
}

void OBJECT_OT_face_map_deselect(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Deselect Face Map Faces";
	ot->idname = "OBJECT_OT_face_map_deselect";
	ot->description = "Deselect faces belonging to a face map";
	
	/* api callbacks */
	ot->poll = face_map_supported_edit_mode_poll;
	ot->exec = face_map_deselect_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


static int face_map_move_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_context(C);
	bFaceMap *fmap;
	int dir = RNA_enum_get(op->ptr, "direction");
	int pos1, pos2 = -1;

	fmap = BLI_findlink(&ob->fmaps, ob->actfmap - 1);
	if (!fmap) {
		return OPERATOR_CANCELLED;
	}

	pos1 = BLI_findindex(&ob->fmaps, fmap);
	
	if (dir == 1) { /*up*/
		void *prev = fmap->prev;

		if (prev) {
			pos2 = pos1 - 1;
		}
		
		BLI_remlink(&ob->fmaps, fmap);
		BLI_insertlinkbefore(&ob->fmaps, prev, fmap);
	}
	else { /*down*/
		void *next = fmap->next;

		if (next) {
			pos2 = pos1 + 1;
		}
		
		BLI_remlink(&ob->fmaps, fmap);
		BLI_insertlinkafter(&ob->fmaps, next, fmap);
	}

	/* iterate through mesh and substitute the indices as necessary */
	object_facemap_swap(ob, pos2, pos1);
	
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM | ND_VERTEX_GROUP, ob);

	return OPERATOR_FINISHED;
}


void OBJECT_OT_face_map_move(wmOperatorType *ot)
{
	static EnumPropertyItem fmap_slot_move[] = {
		{1, "UP", 0, "Up", ""},
		{-1, "DOWN", 0, "Down", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Move Face Map";
	ot->idname = "OBJECT_OT_face_map_move";
	ot->description = "Move the active face map up/down in the list";

	/* api callbacks */
	ot->poll = face_map_supported_poll;
	ot->exec = face_map_move_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_enum(ot->srna, "direction", fmap_slot_move, 0, "Direction", "Direction to move, UP or DOWN");
}
