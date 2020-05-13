#include <clib/macros.h>
#include <proto/dos.h>

#include "gui/interface.h"
#include "gui.h"

/******************************************************************
 * playlistlistclass
 *****************************************************************/

struct PlayListEntry
{
	char name[256];
	char artist[256];
	char title[256];
	int duration;
};

struct PlaylistListData
{
	APTR pic_play;
	APTR pic_stop;
	APTR pic_pause;

	APTR img_play;
	APTR img_stop;
	APTR img_pause;

	ULONG sort_col;
	LONG sort_direction;
};

static void doset(APTR obj, struct PlaylistListData *data, struct TagItem *tags)
{
	FORTAG(tags)
	{
		case MUIA_List_TitleClick:
		{
			if(tag->ti_Data == 0)
				break;

			if(data->sort_col == tag->ti_Data)
				data->sort_direction *= -1;
			else
				data->sort_col = tag->ti_Data;

			mygui->playlist->sort_playlist(mygui->playlist, (int) data->sort_col, (int) data->sort_direction);
			DoMethod(obj, MUIM_List_Sort);
			set(obj, MUIA_List_Active, mygui->playlist->current);

			break;
		}
	}
	NEXTTAG
}

DEFNEW(PlaylistList)
{
	obj = (Object *)DoSuperNew(cl, obj,
					MUIA_Frame, MUIV_Frame_InputList,
					MUIA_CycleChain, 1,
					MUIA_List_Format, ",MIW=-1 MAW=-2 BAR, BAR H, BAR H, BAR H",
					MUIA_Listview_MultiSelect, MUIV_Listview_MultiSelect_None,
				    MUIA_Dropable, TRUE,
					MUIA_List_ShowDropMarks, TRUE,
                    MUIA_List_DragSortable, TRUE,
					MUIA_List_DragType, 1,
				    MUIA_List_Title, TRUE,
					MUIA_List_MinLineHeight, 22,
					End;

	if(obj)
	{
		struct PlaylistListData *data = INST_DATA(cl, obj);
		data->sort_col = 1;
        data->sort_direction = 1;	 
	}

	return (IPTR)obj;
}

DEFDISP(PlaylistList)
{
	return (DOSUPER);
}

DEFSET(PlaylistList)
{
	struct PlaylistListData *data = INST_DATA(cl, obj);

	doset(obj, data, INITTAGS);
	return (DOSUPER);
}

static STRPTR list_titles[] =
{
	"",
	"Title",
	"Duration",
	"Artist",
	"Filename",
	NULL
};

DEFMMETHOD(List_Display)
{
	struct PlaylistListData *data = INST_DATA(cl, obj);
	struct PlayListEntry *entry = (struct PlayListEntry *) msg->entry;

	if (entry)
	{
		static TEXT duration[64];
		static TEXT title[256];
		static TEXT state[32];

		LONG pos = (LONG) msg->array[-1];
		int	h=0, m=0, s=0;

		IPTR image = 0;

		if(pos%2)
		{
			msg->array[ -9 ] = (STRPTR) 10;
		}

		if(entry->duration)
		{
			h = entry->duration / 3600;
			m = entry->duration / 60 % 60;
			s = entry->duration % 60;
			snprintf(duration, sizeof(duration), "%02d:%02d:%02d", h, m, s);
		}
		else
		{
			snprintf(duration, sizeof(duration), "N/A");
		}

		switch(guiInfo.Playing)
		{
			default:
			case GUI_STOP:
				image = (IPTR) data->img_stop;
				break;
			case GUI_PLAY:
				image = (IPTR) data->img_play;
				break;
			case GUI_PAUSE:
				image = (IPTR) data->img_pause;
				break;
		}

		snprintf(state, sizeof(state), "\033O[%08lx]",
				 pos == mygui->playlist->current ? image : /*(ULONG)data->img_stop*/0);

		snprintf(title, sizeof(title), "%s%s",
				 pos == mygui->playlist->current ? "\033b" : "",
				 entry->title[0] ? entry->title : (char *) FilePart(entry->name));

		msg->array[0] = state;
		msg->array[1] = title;
		msg->array[2] = duration;
		msg->array[3] = entry->artist[0] ? entry->artist : "N/A";
		msg->array[4] = (char *) entry->name;
	}
	else
	{
		int i;
		static TEXT title[64];

		snprintf(title, sizeof(title), "%s \033I[6:%s]", list_titles[data->sort_col], (data->sort_direction > 0) ? "38" : "39" );

		for(i = 0; i < 5; i++)
		{
			if(i == data->sort_col)
			{
				msg->array[i] = title;
			}
			else
			{
				msg->array[i] = list_titles[i];
			}
		}
	}
	return 0;
}

DEFMMETHOD(List_Construct)
{
	pl_track_t * data = (pl_track_t *) msg->entry;
	struct PlayListEntry * new_entry = (struct PlayListEntry *) AllocVecPooled(msg->pool, sizeof(struct PlayListEntry));

	if (new_entry)
	{
		memset(new_entry, 0, sizeof(struct PlayListEntry));
		if(data->filename) stccpy(new_entry->name, data->filename, sizeof(new_entry->name));
		if(data->artist)   stccpy(new_entry->artist, data->artist, sizeof(new_entry->artist));
		if(data->title)    stccpy(new_entry->title, data->title, sizeof(new_entry->title));
		new_entry->duration = data->duration;
	}
	return (IPTR) new_entry;
}

DEFMMETHOD(List_Destruct)
{
	struct PlayListEntry *entry = (struct PlayListEntry *) msg->entry;

	if (entry)
		FreeVecPooled(msg->pool, (void *) entry);

	return 0;
}

DEFMMETHOD(Setup)
{
	struct PlaylistListData *data = INST_DATA(cl, obj);
	ULONG rc;

	if((rc = DOSUPER))
	{
		data->pic_play  = MUI_NewObject(MUIC_Dtpic, MUIA_Dtpic_Name, "PROGDIR:images/play.png", End;
		data->pic_pause = MUI_NewObject(MUIC_Dtpic, MUIA_Dtpic_Name, "PROGDIR:images/pause.png", End;
		data->pic_stop  = MUI_NewObject(MUIC_Dtpic, MUIA_Dtpic_Name, "PROGDIR:images/stop.png", End;

		if(data->pic_play)  data->img_play  = (APTR) DoMethod(obj, MUIM_List_CreateImage, data->pic_play, 0);
		if(data->pic_pause) data->img_pause = (APTR) DoMethod(obj, MUIM_List_CreateImage, data->pic_pause, 0);
		if(data->pic_stop)  data->img_stop  = (APTR) DoMethod(obj, MUIM_List_CreateImage, data->pic_stop, 0);
	}

	return rc;
}

DEFMMETHOD(Cleanup)
{
	struct PlaylistListData *data = INST_DATA(cl, obj);

	if(data->img_play)  DoMethod(obj, MUIM_List_DeleteImage, data->img_play);
	if(data->img_pause) DoMethod(obj, MUIM_List_DeleteImage, data->img_pause);
	if(data->img_stop)  DoMethod(obj, MUIM_List_DeleteImage, data->img_stop);

	if(data->pic_play)  MUI_DisposeObject(data->pic_play);
	if(data->pic_pause) MUI_DisposeObject(data->pic_pause);
	if(data->pic_stop ) MUI_DisposeObject(data->pic_stop);

	data->pic_play  = NULL;
	data->pic_pause = NULL;
	data->pic_stop  = NULL;

	data->img_play  = NULL;
	data->img_pause = NULL;
	data->img_stop  = NULL;

	return DOSUPER;
}

static LONG compareEntries( struct PlaylistListData * data, struct PlayListEntry *e1, struct PlayListEntry *e2, LONG col )
{
	LONG rc = 0;

	if (col == 1)
	{
		/* column 0 - title */
		char* str1, * str2;

		if(e1->title[0]) str1 = e1->title; else str1 = FilePart(e1->name);
		if(e2->title[0]) str2 = e2->title; else str2 = FilePart(e2->name);

		rc = stricmp(str1, str2);
	}
	else if (col == 2)
	{
		/* column 2 - duration */
		if(e1->duration > e2->duration)
			return 1;
		else if(e1->duration < e2->duration)
			return -1;
		else
			return 0;
	}
	else if (col == 3)
	{
		/* column 3 - artist */
		char* str1, * str2;

		if(e1->title[0]) str1 = e1->title; else str1 = FilePart(e1->name);
		if(e2->title[0]) str2 = e2->title; else str2 = FilePart(e2->name);

		rc = stricmp(e1->artist, e2->artist);

		if (rc == 0)
			rc = stricmp(str1, str2);
	}
	else if(col == 4)
	{
		rc = stricmp(e1->name, e2->name);
	}

	return rc;
}

DEFMMETHOD(List_Compare)
{
	struct PlaylistListData *data = INST_DATA(cl, obj);

	struct PlayListEntry *e1 = (struct PlayListEntry *)msg->entry1;
	struct PlayListEntry *e2 = (struct PlayListEntry *)msg->entry2;

	/* get clicked col and reverse state*/
	LONG col = data->sort_col;
	LONG rev = data->sort_direction;

	LONG result;

	/* compare for each column */

	result = compareEntries(data, e1 , e2, col);
	result *= rev;

	return result;
}

DEFMMETHOD(DragDrop)
{
	if (msg->obj == obj)
	{
		LONG source_index = getv(obj, MUIA_List_Active);
		LONG dropmark = getv(obj, MUIA_List_DropMark);

		if(source_index > dropmark)
		{
			LONG count = source_index - dropmark;

			while(count--)
			{
				mygui->playlist->moveup_track(mygui->playlist, source_index + 1);
				source_index--;
			}
		}
		else if(source_index < dropmark)
		{
			LONG count = dropmark - source_index - 1;

			while(count--)
			{
				mygui->playlist->movedown_track(mygui->playlist, source_index + 1);
				source_index++;
			}		 
		}

		return DOSUPER;
	}

	return 0;
}

BEGINMTABLE2(playlistlistclass)
DECNEW(PlaylistList)
DECDISP(PlaylistList)
DECSET(PlaylistList)
DECMMETHOD(Setup)
DECMMETHOD(Cleanup)
DECMMETHOD(List_Display)
DECMMETHOD(List_Construct)
DECMMETHOD(List_Destruct)
DECMMETHOD(List_Compare)
DECMMETHOD(DragDrop)
ENDMTABLE

DECSUBCLASS_NC(MUIC_List, playlistlistclass, PlaylistListData)
