#include <proto/asl.h>
#include <clib/macros.h>
#include <proto/dos.h>
#include <workbench/workbench.h>

#include "metadata.h"
#include "morphos_stuff.h"
#include "gui/interface.h"
#include "gui.h"

#if !defined(__AROS__)
static LONG AppMsgFunc(void)
{
	struct AppMessage **x = (struct AppMessage **) REG_A1;
	APTR obj = (APTR) REG_A2;
#else
AROS_UFH3(static LONG, AppMsgFunc,
    AROS_UFPA(struct Hook *         , hook  , A0),
    AROS_UFPA(APTR                  , obj   , A2),
    AROS_UFPA(struct AppMessage **  , x     , A1))
{
    AROS_USERFUNC_INIT
#endif
	struct WBArg *ap;
	struct AppMessage *amsg = *x;
	int i;
	char buf[256];

	for (ap=amsg->am_ArgList,i=0;i<amsg->am_NumArgs;i++,ap++)
	{
		if(NameFromLock(ap->wa_Lock, buf, sizeof(buf)))
		{
			AddPart(buf, ap->wa_Name, sizeof(buf));

			if(!parse_filename(buf, playtree, mconfig, 0))
			{
				BPTR l;

				l = Lock(buf, ACCESS_READ);

				if(l)
				{
					struct FileInfoBlock fi;

					if(Examine(l, &fi))
					{
						if(fi.fib_DirEntryType > 0)
						{
							adddirtoplaylist(mygui->playlist, buf, TRUE, TRUE);
						}
						else
						{
							mygui->playlist->add_track(mygui->playlist, buf, NULL, NULL, 0, STREAMTYPE_FILE);
							DoMethod(obj, MM_PlaylistGroup_Add, mygui->playlist->tracks[mygui->playlist->trackcount - 1]);
						}
					}
					UnLock(l);
				}
			}
		}
	}

	return(0);
#if !defined(__AROS__)
}

static struct EmulLibEntry AppMsgHookGate = { TRAP_LIB, 0, (void (*)(void))AppMsgFunc };
static struct Hook AppMsgHook = { {0, 0}, (HOOKFUNC)&AppMsgHookGate, NULL, NULL };
#else
    AROS_USERFUNC_EXIT
}
static struct Hook AppMsgHook = { {0, 0}, (HOOKFUNC)&AppMsgFunc, NULL, NULL };
#endif

/******************************************************************
 * playlistgroupclass
 *****************************************************************/

struct PlaylistGroupData
{
	APTR LI_Playlist;
	APTR BT_Play;
	APTR BT_Add;
	APTR BT_Remove;
	APTR BT_MoveUp;
	APTR BT_MoveDown;
	APTR BT_Clear;
	APTR BT_Shuffle;

	APTR BT_Save;

	APTR urlwindow;

	char last_added[MAX_PATH];
};

DEFNEW(PlaylistGroup)
{
	APTR LI_Playlist, BT_Play, BT_Add, BT_Remove, BT_MoveUp, BT_MoveDown, BT_Save, BT_Shuffle, BT_Clear;

	obj = (Object *)DoSuperNew(cl, obj,
		MUIA_Group_Horiz, TRUE,
		Child,
			LI_Playlist = NewObject(getplaylistlistclass(), NULL, TAG_DONE),

		Child,
			VGroup, MUIA_Weight, 0,
				Child, BT_Play     = MakeButton("Play"),
				Child, BT_Add      = MakeButton("Add..."),
				Child, BT_Remove   = MakeButton("Remove"),
				Child, BT_Clear    = MakeButton("Remove All"),
				Child, BT_MoveUp   = MakeButton("Move Up"),
				Child, BT_MoveDown = MakeButton("Move Down"),
				Child, BT_Shuffle  = MakeButton("Shuffle"),
				Child, VSpace(0),
				Child, BT_Save     = MakeButton("Save As..."),
				End,

		TAG_DONE
	);

	if (obj)
	{
		struct PlaylistGroupData *data = INST_DATA(cl, obj);

		data->LI_Playlist = LI_Playlist;
		data->BT_Play = BT_Play;
		data->BT_Add = BT_Add;
		data->BT_Remove = BT_Remove;
		data->BT_Clear = BT_Clear;
		data->BT_MoveUp = BT_MoveUp;
		data->BT_MoveDown = BT_MoveDown;
		data->BT_Shuffle = BT_Shuffle;
		data->BT_Save = BT_Save;

		/* buttons notifications */

		DoMethod(data->BT_Play, MUIM_Notify, MUIA_Pressed, FALSE,
				 obj, 1, MM_PlaylistGroup_Play);

		DoMethod(data->BT_Add, MUIM_Notify, MUIA_Pressed, FALSE,
				 obj, 1, MM_PlaylistGroup_AddRequester);

		DoMethod(data->BT_Remove, MUIM_Notify, MUIA_Pressed, FALSE,
				 obj, 1, MM_PlaylistGroup_Remove);

		DoMethod(data->BT_Clear, MUIM_Notify, MUIA_Pressed, FALSE,
				 obj, 1, MM_PlaylistGroup_RemoveAll);

		DoMethod(data->BT_MoveUp, MUIM_Notify, MUIA_Pressed, FALSE,
				 obj, 1, MM_PlaylistGroup_MoveUp);

		DoMethod(data->BT_MoveDown, MUIM_Notify, MUIA_Pressed, FALSE,
				 obj, 1, MM_PlaylistGroup_MoveDown);

		DoMethod(data->BT_Shuffle, MUIM_Notify, MUIA_Pressed, FALSE,
				 obj, 1, MM_PlaylistGroup_Shuffle);

		DoMethod(data->BT_Save, MUIM_Notify, MUIA_Pressed, FALSE,
				 obj, 1, MM_PlaylistGroup_SaveRequester);

		DoMethod(data->LI_Playlist, MUIM_Notify, MUIA_Listview_DoubleClick, MUIV_EveryTime,
				 obj, 1, MM_PlaylistGroup_Play);

		/* Handle appwindow events */
		DoMethod(obj, MUIM_Notify, MUIA_AppMessage, MUIV_EveryTime,
				 obj, 3, MUIM_CallHook, &AppMsgHook, MUIV_TriggerValue);

		/* Restore if needed */

		if(remember_playlist)
		{
			DoMethod(obj, MM_PlaylistGroup_Load, "PROGDIR:conf/lastplaylist.pls");
		}
	}

	return (IPTR)obj;
}

DEFDISP(PlaylistGroup)
{
	if(remember_playlist)
	{
		DoMethod(obj, MM_PlaylistGroup_Save, "PROGDIR:conf/lastplaylist.pls");
	}
	return (DOSUPER);
}

DEFGET(PlaylistGroup)
{
	switch (msg->opg_AttrID)
	{
		case MA_PlaylistGroup_Current:
		{
			*msg->opg_Storage = mygui->playlist->current;
		}
		return (TRUE);
	}

	return DOSUPER;
}

static void doset(APTR obj, struct PlaylistGroupData *data, struct TagItem *tags)
{
	FORTAG(tags)
	{
		case MA_PlaylistGroup_Current:
			mygui->playlist->current = tag->ti_Data;
			break;
	}
	NEXTTAG
}

DEFSET(PlaylistGroup)
{
	struct PlaylistGroupData *data = INST_DATA(cl, obj);

	doset(obj, data, INITTAGS);

	return DOSUPER;
}

DEFSMETHOD(PlaylistGroup_Add)
{
	struct PlaylistGroupData *data = INST_DATA(cl, obj);

	DoMethod(data->LI_Playlist, MUIM_List_InsertSingle, msg->entry, MUIV_List_Insert_Bottom);

	return 0;
}

enum { POPMENU_FILE = 1, POPMENU_DIRECTORY, POPMENU_PLAYLIST, POPMENU_STREAM };

DEFTMETHOD(PlaylistGroup_AddRequester)
{
	extern char muititle[];
	struct PlaylistGroupData *data = INST_DATA(cl, obj);

	int action = 0;
	Object *menu;
	Object *popmenu = (Object *) MenustripObject, Child, menu = MenuObject, MUIA_Menu_Title, "", End, End;

	if(popmenu)
	{
		DoMethod(menu, OM_ADDMEMBER,
				 MenuitemObject,
					MUIA_UserData, POPMENU_FILE,
					MUIA_Menuitem_Title, "Files...",
				 End);

		DoMethod(menu, OM_ADDMEMBER,
				 MenuitemObject,
					MUIA_UserData, POPMENU_DIRECTORY,
					MUIA_Menuitem_Title, "Directory...",
				 End);

		DoMethod(menu, OM_ADDMEMBER,
				 MenuitemObject,
					MUIA_UserData, POPMENU_PLAYLIST,
					MUIA_Menuitem_Title, "Playlist...",
				 End);
	
		DoMethod(menu, OM_ADDMEMBER,
				 MenuitemObject,
					MUIA_UserData, POPMENU_STREAM,
					MUIA_Menuitem_Title, "Stream...",
				 End);	  
	}

	action = DoMethod(popmenu, MUIM_Menustrip_Popup, data->BT_Add, 0, _mleft(data->BT_Add), _mtop(data->BT_Add));

	MUI_DisposeObject(popmenu);

#if defined(__AROS__)
	/* Since MUIM_Menustrip_Popup does nothing for now, just assume selection for files */
	action = POPMENU_FILE;
#endif

	if(action == POPMENU_FILE)
	{
		char ** files = NULL;
		STRPTR ptr;
		ULONG count;

		APTR tags[] = { (APTR) ASLFR_TitleText, (APTR) "Select Files...",
						(APTR) ASLFR_InitialPattern, (APTR) MorphOS_GetPattern(), //"("PATTERN_STREAM "|" PATTERN_PLAYLIST")",
					    TAG_DONE };

		ptr = data->last_added[0] ? data->last_added : MorphOS_GetLastVisitedPath();

		set(app, MUIA_Application_Sleep, TRUE);

		count = asl_run_multiple(ptr, (APTR)&tags, &files, TRUE);

		set(app, MUIA_Application_Sleep, FALSE);

		if (files)
		{
			ULONG i;

			// Enqueue others
			for(i = 0; i < count; i++)
			{
				if(!parse_filename(files[i], playtree, mconfig, 0))
				{
					BPTR l;

					l = Lock(files[i], ACCESS_READ);

					if(l)
					{
						struct FileInfoBlock fi;

						if(Examine(l, &fi))
						{
							if(fi.fib_DirEntryType > 0)
							{
								adddirtoplaylist(mygui->playlist, files[i], TRUE, TRUE);
							}
							else
							{
								mygui->playlist->add_track(mygui->playlist, files[i], NULL, NULL, 0, STREAMTYPE_FILE);
								DoMethod(obj, MM_PlaylistGroup_Add, mygui->playlist->tracks[mygui->playlist->trackcount - 1]);
							}

							stccpy(data->last_added, files[i], sizeof(data->last_added));
						}
						UnLock(l);
					}
				}
			}

			update_playlistwindow(TRUE);

			asl_free(count, files);
		}
	}
	else if(action == POPMENU_DIRECTORY)
	{
		STRPTR ptr, p;
		APTR tags[] = { (APTR) ASLFR_TitleText, (APTR) "Select Directory...",
                        (APTR) ASLFR_DrawersOnly, (APTR) TRUE,
					    TAG_DONE };

		ptr = MorphOS_GetLastVisitedPath();

		set(app, MUIA_Application_Sleep, TRUE);

		p = asl_run(ptr, (APTR)&tags, FALSE);

		set(app, MUIA_Application_Sleep, FALSE);

		if (p)
		{
			adddirtoplaylist(mygui->playlist, p, TRUE, TRUE);
			FreeVecTaskPooled(p);
		}

	}
	else if(action == POPMENU_PLAYLIST)
	{
		STRPTR ptr, p;
		APTR tags[] = { (APTR) ASLFR_TitleText, (APTR) "Select Playlist...",
						(APTR) ASLFR_InitialPattern, (APTR) "("PATTERN_PLAYLIST")",
					    TAG_DONE };

		ptr = MorphOS_GetLastVisitedPath();

		set(app, MUIA_Application_Sleep, TRUE);

		p = asl_run(ptr, (APTR)&tags, FALSE);

		set(app, MUIA_Application_Sleep, FALSE);

		if (p)
		{
			parse_filename(p, playtree, mconfig, 0);
			FreeVecTaskPooled(p);
		}
	}
	else if(action == POPMENU_STREAM)
	{
		data->urlwindow = WindowObject,
					MUIA_Window_ScreenTitle, muititle,
					MUIA_Window_Title, "Enter URL",
					MUIA_Window_ID, MAKE_ID('U','R','L','W'),
					WindowContents,
						NewObject(geturlgroupclass(), NULL, MA_URLGroup_Target, obj, TAG_DONE),
					End;

		if(data->urlwindow)
		{
			DoMethod(app, OM_ADDMEMBER, data->urlwindow);
			set(data->urlwindow, MUIA_Window_Open, TRUE);
		}
	}

	return 0;
}

DEFSMETHOD(MPlayer_OpenStream)
{
	struct PlaylistGroupData *data = INST_DATA(cl, obj);

	if(msg->url)
	{
		mygui->playlist->add_track(mygui->playlist, msg->url, NULL, msg->url, 0, STREAMTYPE_STREAM);
		DoMethod(obj, MM_PlaylistGroup_Add, mygui->playlist->tracks[mygui->playlist->trackcount - 1]);
	}

	if(data->urlwindow)
	{
		DoMethod(app, MUIM_Application_PushMethod, app, 2, MM_MPlayerApp_DisposeWindow, data->urlwindow);
		data->urlwindow = NULL;
	}

	return 0;
}

DEFTMETHOD(PlaylistGroup_Remove)
{
	struct PlaylistGroupData *data = INST_DATA(cl, obj);

	/* don't remove currently played entry */
	if(mygui->playlist->current == getv(data->LI_Playlist, MUIA_List_Active))
	{
		return 0;
	}

	mygui->playlist->remove_track(mygui->playlist, getv(data->LI_Playlist, MUIA_List_Active) + 1);

	DoMethod(data->LI_Playlist, MUIM_List_Remove, MUIV_List_Remove_Active);

	return 0;
}

DEFTMETHOD(PlaylistGroup_RemoveAll)
{
	struct PlaylistGroupData *data = INST_DATA(cl, obj);

	char * filename = mygui->playlist->tracks[(mygui->playlist->current)]->filename ? strdup(mygui->playlist->tracks[(mygui->playlist->current)]->filename) : NULL;
	char * title = mygui->playlist->tracks[(mygui->playlist->current)]->title ? strdup(mygui->playlist->tracks[(mygui->playlist->current)]->title) : NULL;
	char * artist = mygui->playlist->tracks[(mygui->playlist->current)]->artist ? strdup(mygui->playlist->tracks[(mygui->playlist->current)]->artist) : NULL;
	int duration = mygui->playlist->tracks[(mygui->playlist->current)]->duration;
	int type = mygui->playlist->tracks[(mygui->playlist->current)]->type;

	DoMethod(data->LI_Playlist, MUIM_List_Clear);

	mygui->playlist->clear_playlist(mygui->playlist);
	mygui->playlist->add_track(mygui->playlist, filename, title, artist, duration, type);
	mygui->playlist->current = 0;

	DoMethod(obj, MM_PlaylistGroup_Refresh, TRUE);

	free(filename);
	free(title);
	free(artist);

	return 0;
}

DEFTMETHOD(PlaylistGroup_MoveUp)
{
	struct PlaylistGroupData *data = INST_DATA(cl, obj);

	mygui->playlist->moveup_track(mygui->playlist, getv(data->LI_Playlist, MUIA_List_Active) + 1);

	DoMethod(data->LI_Playlist, MUIM_List_Move, MUIV_List_Move_Active, MUIV_List_Move_Previous);

	set(data->LI_Playlist, MUIA_List_Active, MUIV_List_Active_Up);

	return 0;
}

DEFTMETHOD(PlaylistGroup_MoveDown)
{
	struct PlaylistGroupData *data = INST_DATA(cl, obj);

	mygui->playlist->movedown_track(mygui->playlist, getv(data->LI_Playlist, MUIA_List_Active) + 1);

	DoMethod(data->LI_Playlist, MUIM_List_Move, MUIV_List_Move_Active, MUIV_List_Move_Next);

	set(data->LI_Playlist, MUIA_List_Active, MUIV_List_Active_Down);

	return 0;
}

DEFTMETHOD(PlaylistGroup_Shuffle)
{
	struct PlaylistGroupData *data = INST_DATA(cl, obj);

	DoMethod(data->LI_Playlist, MUIM_List_Clear);

	mygui->playlist->shuffle_playlist(mygui->playlist);

	DoMethod(obj, MM_PlaylistGroup_Refresh, TRUE);

	return 0;
}

DEFSMETHOD(PlaylistGroup_Refresh)
{
	struct PlaylistGroupData *data = INST_DATA(cl, obj);
	int i = 0;
/*
	if(guiInfo.RunningTime)
	{
		mygui->playlist->tracks[mygui->playlist->current]->duration = guiInfo.RunningTime;
	}
*/

	if(msg->rebuild)
	{
		set(data->LI_Playlist, MUIA_List_Quiet, TRUE);

		DoMethod(data->LI_Playlist, MUIM_List_Clear);

		for (i=0; i < mygui->playlist->trackcount; i++)
		{
			DoMethod(obj, MM_PlaylistGroup_Add, (APTR) mygui->playlist->tracks[i]);
		}
		set(data->LI_Playlist, MUIA_List_Active, mygui->playlist->current);

		set(data->LI_Playlist, MUIA_List_Quiet, FALSE);
	}
	else
	{
		/*
		if(mygui->playlist->tracks)
		{
			mygui->playlist->tracks[mygui->playlist->current]->duration = property_expand_string(guiInfo.mpcontext, "${length}")
		}
		*/
		DoMethod(data->LI_Playlist, MUIM_List_Jump, mygui->playlist->current);
		DoMethod(data->LI_Playlist, MUIM_List_Redraw, MUIV_List_Redraw_All);
	}

	return 0;
}

DEFTMETHOD(PlaylistGroup_Clear)
{
	mygui->playlist->clear_playlist(mygui->playlist);

	DoMethod(obj, MUIM_List_Clear);

	return 0;
}

DEFTMETHOD(PlaylistGroup_Play)
{
	struct PlaylistGroupData *data = INST_DATA(cl, obj);

	if(getv(data->LI_Playlist, MUIA_List_Active) != MUIV_List_Active_Off)
	{
		set(obj, MA_PlaylistGroup_Current, getv(data->LI_Playlist, MUIA_List_Active));

		uiSetFileName(NULL, mygui->playlist->tracks[(mygui->playlist->current)]->filename, mygui->playlist->tracks[(mygui->playlist->current)]->type);
		mygui->playercontrol(evLoadPlay, 0);
	}
	return 0;
}

DEFTMETHOD(PlaylistGroup_SaveRequester)
{
	APTR tags[] = { (APTR) ASLFR_TitleText, (APTR) "Save As...",
					(APTR) ASLFR_DoSaveMode, (APTR) TRUE,
					(APTR) ASLFR_InitialPattern, (APTR) PATTERN_PLAYLIST,
				    TAG_DONE };
	STRPTR p;
	TEXT path[1024];
	snprintf(path, sizeof(path), "%s", MorphOS_GetLastVisitedPath());
	AddPart(path, "playlist.pls", sizeof(path));

	set(app, MUIA_Application_Sleep, TRUE);

	p = asl_run(path, (APTR)&tags, FALSE);

	set(app, MUIA_Application_Sleep, FALSE);

	if (p)
	{
		int i;

		FILE * file = fopen(p, "w");

		if(file)
		{
			for (i=0; i < mygui->playlist->trackcount; i++)
			{
				fprintf(file, "%s\n",mygui->playlist->tracks[i]->filename);
			}
			fclose(file);
		}

		FreeVecTaskPooled(p);
	}

	return 0;
}

DEFSMETHOD(PlaylistGroup_Load)
{
	if(parse_filename(msg->path, playtree, mconfig, 0))
	{
		DoMethod(obj, MM_PlaylistGroup_Refresh, TRUE);
	}

	return 0;
}

DEFSMETHOD(PlaylistGroup_Save)
{
	if (msg->path)
	{
		int i;

		FILE * file = fopen(msg->path, "w");

		if(file)
		{
			for (i=0; i < mygui->playlist->trackcount; i++)
			{
				fprintf(file, "%s\n",mygui->playlist->tracks[i]->filename);
			}
			fclose(file);
		}
	}

	return 0;
}

BEGINMTABLE2(playlistgroupclass)
DECNEW(PlaylistGroup)
DECDISP(PlaylistGroup)
DECGET(PlaylistGroup)
DECSET(PlaylistGroup)
DECTMETHOD(PlaylistGroup_Play)
DECSMETHOD(PlaylistGroup_Add)
DECTMETHOD(PlaylistGroup_AddRequester)
DECTMETHOD(PlaylistGroup_Remove)
DECTMETHOD(PlaylistGroup_RemoveAll)
DECTMETHOD(PlaylistGroup_MoveUp)
DECTMETHOD(PlaylistGroup_MoveDown)
DECTMETHOD(PlaylistGroup_Shuffle)
DECTMETHOD(PlaylistGroup_Clear)
DECSMETHOD(PlaylistGroup_Refresh)
DECTMETHOD(PlaylistGroup_SaveRequester)
DECSMETHOD(PlaylistGroup_Load)
DECSMETHOD(PlaylistGroup_Save)
DECSMETHOD(MPlayer_OpenStream)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Group, playlistgroupclass, PlaylistGroupData)
