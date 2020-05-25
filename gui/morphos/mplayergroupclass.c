#include <clib/macros.h>
#include <proto/dos.h>
#include <proto/asl.h>
#include <proto/keymap.h>
#include <workbench/workbench.h>
#include <devices/rawkeycodes.h>
#include <devices/inputevent.h>
#include <unistd.h>

#include "command.h"
#include "help_mp.h"
#include "m_option.h"
#include "mp_fifo.h"
#include "mp_core.h"
#include "mpcommon.h"
#include "mplayer.h"
#include "poplistclass.h"
#include "input/input.h"
#include "input/mouse.h"
#include "libaf/af.h"
#include "libass/ass.h"
#include "libmpdemux/demuxer.h"
#include "libvo/cgx_common.h"
#include "osdep/keycodes.h"
#ifdef CONFIG_DVDREAD
#include "stream/stream_dvd.h"
#endif
#ifdef CONFIG_DVDNAV
#include "stream/stream_dvdnav.h"
#endif
#include "sub/sub.h"
#include "sub/subreader.h"
#include "sub/vobsub.h"

#include "gui/interface.h"
#include "gui.h"
#include "morphos_stuff.h"
#include "thread.h"

extern guiInterface_t guiInfo;
extern gui_t * mygui;

extern char cfg_dvd_device[];

extern char windowtitle[];

extern BOOL mouse_hidden;

extern TEXT credits[];

static ULONG innerleft, innerright, innertop, innerbottom;

#if !defined(__AROS__)
/* Handle appwindow messages */
static LONG AppMsgFunc(void)
{
	struct AppMessage **x = (struct AppMessage **) REG_A1;
//	  APTR obj = (APTR) REG_A2;
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
	ULONG firstentry = TRUE;
	ULONG previoustrackcount = mygui->playlist->trackcount;

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
							adddirtoplaylist(mygui->playlist, buf, TRUE, FALSE);
						}
						else
						{
							mygui->playlist->add_track(mygui->playlist, buf, NULL, NULL, 0, STREAMTYPE_FILE);
						}
					}
					UnLock(l);
				}
			}

			if(firstentry)
			{
				firstentry = FALSE;

				mygui->playlist->current = previoustrackcount;

				if(mygui->playlist->tracks)
				{
					uiSetFileName(NULL, mygui->playlist->tracks[mygui->playlist->current]->filename, STREAMTYPE_FILE);
					mygui->playercontrol(evLoadPlay, 0);
				}
			}		 
		}
	}

	DoMethod(mygui->playlistgroup, MM_PlaylistGroup_Refresh, TRUE);

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
 * mplayergroupclass
 *****************************************************************/

struct MPlayerGroupData
{
	APTR GR_Toolbar;
	APTR GR_StatusBar;
	APTR GR_Control;

	APTR BT_PlayPause;
	APTR BT_Pause;
	APTR BT_Stop;
	APTR BT_Prev;
	APTR BT_Next;
	APTR BT_SeekB;
	APTR BT_SeekF;
	APTR BT_FullScreen;
	APTR BT_OpenFile;
	APTR BT_PlayDVD;
	APTR BT_OpenURL;
	APTR BT_Mute;
	APTR BT_Properties;
	APTR BT_Playlist;
	APTR BT_Screenshot;

	APTR SL_Seek;
	APTR SL_Volume;

	APTR ST_Status;
	APTR ST_Time;

	APTR AR_Video;

	APTR PL_AudioTracks;
	APTR PL_Subtitles;

	APTR PI_Titles;
	APTR PL_Titles;
	APTR LI_Titles;
	APTR ST_Titles;
	APTR PI_Chapters;
	APTR PL_Chapters;
	APTR LI_Chapters;
	APTR ST_Chapters;
	APTR PI_Angles;
	APTR PL_Angles;
	APTR LI_Angles;
	APTR ST_Angles;
	APTR BT_DVDMenu;
	APTR SP_DVDGroupSpacer;

	struct MUI_InputHandlerNode ihnode;
	struct MUI_EventHandlerNode ehnode;
	
	ULONG update;

	ULONG fullscreen;
	ULONG window_gui_show_gui; // window state of gui_show_gui var
};


DEFNEW(MPlayerGroup)
{
	APTR BT_PlayPause, BT_Pause, BT_Stop, BT_SeekB, BT_SeekF,
		 BT_Next, BT_Prev, BT_PlayDVD, BT_OpenFile, BT_OpenURL, BT_Properties, BT_Playlist, BT_Screenshot,
		 BT_Mute, BT_FullScreen, SL_Seek, SL_Volume,
		 ST_Status, ST_Time, GR_Toolbar, GR_StatusBar, GR_Control,
		 PI_Chapters, PL_Chapters, LI_Chapters, ST_Chapters,
		 PI_Titles, PL_Titles, LI_Titles, ST_Titles,
		 PI_Angles, PL_Angles, LI_Angles, ST_Angles, BT_DVDMenu, SP_DVDGroupSpacer;

	APTR AR_Video;

	obj = (Object *)DoSuperNew(cl, obj,
		InnerSpacing(0, 0),
		Child,
			GR_Toolbar = HGroup, MUIA_InnerTop, 4, MUIA_InnerLeft, 4, MUIA_InnerRight, 4, MUIA_InnerBottom, 0, MUIA_Group_VertSpacing, 4,
				Child, BT_OpenFile = PictureButton("Open file", "PROGDIR:images/open.png"),
				Child, BT_PlayDVD  = PictureButton("Play DVD", "PROGDIR:images/dvd.png"),
				Child, BT_OpenURL  = PictureButton("Open URL", "PROGDIR:images/url.png"),

				Child, NewObject(getspacerclass(), NULL, TAG_DONE),

				Child, BT_Screenshot  = PictureButton("Capture current frame", "PROGDIR:images/screenshot.png"),

				Child, NewObject(getspacerclass(), NULL, TAG_DONE),

				Child, BT_Properties = PictureButton("Show media properties", "PROGDIR:images/info.png"),
				Child, BT_Playlist   = PictureButton("Show playlist window", "PROGDIR:images/playlist.png"),

				Child, NewObject(getspacerclass(), NULL, TAG_DONE),

				Child, BT_Prev  = PictureButton("Play previous entry in playlist", "PROGDIR:images/previous.png"),
				Child, BT_Next  = PictureButton("Play next entry in playlist", "PROGDIR:images/xnext.png"),

				Child, HSpace(0),

				Child, BT_DVDMenu = PictureButton("Show DVD Menu", "PROGDIR:images/dvdmenu.png"),

				Child, SP_DVDGroupSpacer = NewObject(getspacerclass(), NULL, TAG_DONE),

				Child, PI_Titles = MUI_NewObject(MUIC_Dtpic, MUIA_Dtpic_Name, "PROGDIR:images/xtitle.png", End,
				Child, PL_Titles = NewObject(getpopstringclass(), NULL,
										MUIA_Popstring_String, ST_Titles = TextObject, TextFrame, MUIA_Background, MUII_TextBack, End,
										MUIA_Popstring_Button, PopButton(MUII_PopUp),
										MUIA_Popobject_Object, LI_Titles = NewObject(getpoplistclass(), NULL, TAG_DONE),
								        End,

				Child, PI_Chapters = MUI_NewObject(MUIC_Dtpic, MUIA_Dtpic_Name, "PROGDIR:images/chapter.png", End,
				Child, PL_Chapters = NewObject(getpopstringclass(), NULL,
										MUIA_Popstring_String, ST_Chapters = TextObject, TextFrame, MUIA_Background, MUII_TextBack, End,
										MUIA_Popstring_Button, PopButton(MUII_PopUp),
										MUIA_Popobject_Object, LI_Chapters = NewObject(getpoplistclass(), NULL, TAG_DONE),
								        End,

				Child, PI_Angles = MUI_NewObject(MUIC_Dtpic, MUIA_Dtpic_Name, "PROGDIR:images/angle.png", End,
				Child, PL_Angles = NewObject(getpopstringclass(), NULL,
										MUIA_Popstring_String, ST_Angles = TextObject, TextFrame, MUIA_Background, MUII_TextBack, End,
										MUIA_Popstring_Button, PopButton(MUII_PopUp),
										MUIA_Popobject_Object, LI_Angles = NewObject(getpoplistclass(), NULL, TAG_DONE),
								        End,

				End,

		Child, AR_Video = NewObject(getvideoareaclass(), NULL, TAG_DONE),

		Child,
			GR_Control = HGroup, MUIA_InnerTop, 0, MUIA_InnerLeft, 4, MUIA_InnerRight, 4, MUIA_InnerBottom, 0, MUIA_Group_VertSpacing, 4,
				Child, BT_PlayPause = PictureButton("Play", "PROGDIR:images/play.png"),
				Child, BT_Pause     = PictureButton("Pause/Framestep", "PROGDIR:images/pause.png"),
				Child, BT_Stop      = PictureButton("Stop", "PROGDIR:images/stop.png"),

				Child, NewObject(getspacerclass(), NULL, TAG_DONE),

				Child, BT_SeekB = PictureButton("Rewind by 10 seconds", "PROGDIR:images/xrewind10s.png"),
				Child, BT_SeekF = PictureButton("Forward by 10 seconds", "PROGDIR:images/forward10s.png"),

				Child, SL_Seek  = NewObject(getseeksliderclass(), NULL, TAG_DONE),
					
				Child, NewObject(getspacerclass(), NULL, TAG_DONE),

				Child, BT_FullScreen = PictureButton("Toggle fullscreen/window mode", "PROGDIR:images/fullscreen.png"),
				Child, BT_Mute       = PictureButton("Mute on/off", "PROGDIR:images/volume-max.png"),

				Child, SL_Volume = NewObject(getvolumesliderclass(), NULL, TAG_DONE),

				End,

		Child,
			GR_StatusBar = HGroup, MUIA_InnerTop, 0, MUIA_InnerLeft, 4, MUIA_InnerRight, 4, MUIA_InnerBottom, 4, MUIA_Group_VertSpacing, 4,
				Child, ST_Status = 	TextObject,	TextFrame,
									MUIA_Weight, 1000,
								    MUIA_Text_Contents, " ",
									MUIA_Text_SetMin, FALSE,
									MUIA_Text_SetMax, FALSE,
									gui_black_status ? MUIA_Text_PreParse : TAG_IGNORE, "\0333",
									MUIA_Background, gui_black_status ? MUII_SHADOW : MUII_TextBack,
								    End,
				Child, ST_Time   = 	TextObject,	TextFrame,
									MUIA_Weight, 0,
									MUIA_Text_Contents, "00:00:00 / 00:00:00",
									gui_black_status ? MUIA_Text_PreParse : TAG_IGNORE, "\0333",
									MUIA_Background, gui_black_status ? MUII_SHADOW : MUII_TextBack,
									End,
				End,

		TAG_DONE
	);

	if (obj)
	{
		struct MPlayerGroupData *data = INST_DATA(cl, obj);

		data->update = TRUE;
		data->window_gui_show_gui = gui_show_gui;

		data->GR_Toolbar = GR_Toolbar;
		data->GR_StatusBar = GR_StatusBar;
		data->GR_Control = GR_Control;

		data->BT_PlayPause = BT_PlayPause;
		data->BT_Pause = BT_Pause;
		data->BT_Stop = BT_Stop;
		data->BT_SeekB = BT_SeekB;
		data->BT_SeekF = BT_SeekF;
		data->BT_Prev = BT_Prev;
		data->BT_Next = BT_Next;
		data->BT_Mute = BT_Mute;

		data->BT_FullScreen = BT_FullScreen;
		data->SL_Seek = SL_Seek;
		data->SL_Volume = SL_Volume;

		data->BT_OpenFile = BT_OpenFile;
		data->BT_PlayDVD = BT_PlayDVD;
		data->BT_OpenURL = BT_OpenURL;

		data->BT_Properties = BT_Properties;
		data->BT_Playlist = BT_Playlist;

		data->BT_Screenshot = BT_Screenshot;

		data->ST_Status = ST_Status;
		data->ST_Time = ST_Time;

		data->AR_Video = AR_Video;
		mygui->videoarea = AR_Video;

		data->PI_Chapters = PI_Chapters;
		data->PL_Chapters = PL_Chapters;
		data->LI_Chapters = LI_Chapters;
		data->ST_Chapters = ST_Chapters;
		data->PI_Titles = PI_Titles;
		data->PL_Titles = PL_Titles;
		data->LI_Titles = LI_Titles;
		data->ST_Titles = ST_Titles;
		data->PI_Angles = PI_Angles;
		data->PL_Angles = PL_Angles;
		data->LI_Angles = LI_Angles;
		data->ST_Angles = ST_Angles;
		data->BT_DVDMenu = BT_DVDMenu;
		data->SP_DVDGroupSpacer = SP_DVDGroupSpacer;

		/* buttons notifications */

		/* Main buttons */
		DoMethod(data->BT_PlayPause, MUIM_Notify, MA_PictureButton_Hit, MUIV_EveryTime,
				 obj, 1, MM_MPlayerGroup_Play);

		DoMethod(data->BT_Pause, MUIM_Notify, MA_PictureButton_Hit, MUIV_EveryTime,
				 obj, 1, MM_MPlayerGroup_Pause);

		DoMethod(data->BT_Stop, MUIM_Notify, MA_PictureButton_Hit, MUIV_EveryTime,
				 obj, 1, MM_MPlayerGroup_Stop);

		DoMethod(data->BT_SeekB, MUIM_Notify, MA_PictureButton_Hit, MUIV_EveryTime,
				 obj, 3, MM_MPlayerGroup_Seek, MV_MPlayerGroup_SeekFromRewind, 10);

		DoMethod(data->BT_SeekF, MUIM_Notify, MA_PictureButton_Hit, MUIV_EveryTime,
				 obj, 3, MM_MPlayerGroup_Seek, MV_MPlayerGroup_SeekFromForward, 10);

		DoMethod(data->BT_FullScreen, MUIM_Notify, MA_PictureButton_Hit, MUIV_EveryTime,
				 obj, 1, MM_MPlayerGroup_ToggleFullScreen);

		DoMethod(data->BT_Mute, MUIM_Notify, MA_PictureButton_Hit, MUIV_EveryTime,
				 obj, 1, MM_MPlayerGroup_Mute);

		/* Open */
		DoMethod(data->BT_PlayDVD, MUIM_Notify, MA_PictureButton_Hit, MUIV_EveryTime,
				 obj, 3, MM_MPlayerGroup_Open, MV_MPlayerGroup_Open_DVD, NULL);

		DoMethod(data->BT_OpenFile, MUIM_Notify, MA_PictureButton_Hit, MUIV_EveryTime,
				 obj, 1, MM_MPlayerGroup_OpenFileRequester);

		DoMethod(data->BT_OpenURL, MUIM_Notify, MA_PictureButton_Hit, MUIV_EveryTime,
				 obj, 1, MM_MPlayerGroup_OpenURLRequester);

		/* Playlist navigation */
		DoMethod(data->BT_Prev, MUIM_Notify, MA_PictureButton_Hit, MUIV_EveryTime,
				 obj, 1, MM_MPlayerGroup_Prev);

		DoMethod(data->BT_Next, MUIM_Notify, MA_PictureButton_Hit, MUIV_EveryTime,
				 obj, 1, MM_MPlayerGroup_Next);

		DoMethod(data->BT_Properties, MUIM_Notify, MA_PictureButton_Hit, MUIV_EveryTime,
				 obj, 1, MM_MPlayerGroup_ShowProperties);

		DoMethod(data->BT_Playlist, MUIM_Notify, MA_PictureButton_Hit, MUIV_EveryTime,
				 obj, 1, MM_MPlayerGroup_ShowPlaylist);

		DoMethod(data->BT_Screenshot, MUIM_Notify, MA_PictureButton_Hit, MUIV_EveryTime,
				 obj, 1, MM_MPlayerGroup_Screenshot);

		/* Seek Slider */
		DoMethod(data->SL_Seek, MUIM_Notify, MUIA_Pressed, FALSE,
				 obj, 3, MUIM_Set, MA_MPlayerGroup_Update, TRUE);

		DoMethod(data->SL_Seek, MUIM_Notify, MUIA_Pressed, TRUE,
				 obj, 3, MUIM_Set, MA_MPlayerGroup_Update, FALSE);

	    DoMethod(data->SL_Seek, MUIM_Notify, MUIA_Slider_Level, MUIV_EveryTime,
				 obj, 1, MM_MPlayerGroup_SmartSeek);

		/* Volume Slider */
		DoMethod(data->SL_Volume, MUIM_Notify, MUIA_Slider_Level, MUIV_EveryTime,
				 obj, 1, MM_MPlayerGroup_Volume);

		/* Handle appwindow events */
		DoMethod(obj, MUIM_Notify, MUIA_AppMessage, MUIV_EveryTime,
				 obj, 3, MUIM_CallHook, &AppMsgHook, MUIV_TriggerValue);

		/* Titles/Chapters/Angles Lists */
		DoMethod(data->LI_Chapters, MUIM_Notify, MUIA_Listview_DoubleClick, TRUE,
			     data->PL_Chapters, 2, MUIM_Popstring_Close, TRUE);

		DoMethod(data->LI_Chapters, MUIM_Notify, MUIA_Listview_DoubleClick, TRUE,
				 obj, 2, MM_MPlayerGroup_SetChapter, TRUE);

		DoMethod(data->LI_Titles, MUIM_Notify, MUIA_Listview_DoubleClick, TRUE,
				 data->PL_Titles, 2, MUIM_Popstring_Close, TRUE);

		DoMethod(data->LI_Titles, MUIM_Notify, MUIA_Listview_DoubleClick, TRUE,
				 obj, 2, MM_MPlayerGroup_SetTitle, TRUE);

		DoMethod(data->LI_Angles, MUIM_Notify, MUIA_Listview_DoubleClick, TRUE,
				 data->PL_Angles, 2, MUIM_Popstring_Close, TRUE);

		DoMethod(data->LI_Angles, MUIM_Notify, MUIA_Listview_DoubleClick, TRUE,
				 obj, 2, MM_MPlayerGroup_SetAngle, TRUE);

		DoMethod(data->BT_DVDMenu, MUIM_Notify, MA_PictureButton_Hit, MUIV_EveryTime,
				 obj, 1, MM_MPlayerGroup_ShowDVDMenu);

		/* Visibility */

		if(!mygui->embedded)
		{
			gui_show_gui = TRUE;
			set(data->AR_Video, MUIA_ShowMe, FALSE);
		}

        DoMethod(obj, MM_MPlayerGroup_ShowPanels, gui_show_gui);

		DoMethod(obj, MM_MPlayerGroup_ShowDVDBrowser, FALSE);
		DoMethod(obj, MM_MPlayerGroup_ShowChapterBrowser, FALSE);

		DoMethod(data->GR_StatusBar, MUIM_Notify, MUIA_ShowMe, TRUE,
				 data->GR_Control, 3, MUIM_Set, MUIA_InnerBottom, 0);

		DoMethod(data->GR_StatusBar, MUIM_Notify, MUIA_ShowMe, FALSE,
				 data->GR_Control, 3, MUIM_Set, MUIA_InnerBottom, 4);
	}

	return (IPTR)obj;
}

DEFDISP(MPlayerGroup)
{
	return (DOSUPER);
}

DEFGET(MPlayerGroup)
{
	struct MPlayerGroupData *data = INST_DATA(cl, obj);

	switch (msg->opg_AttrID)
	{

		case MA_MPlayerGroup_Update:
		{
			*msg->opg_Storage = data->update;
		}
		return (TRUE);
	}

	return DOSUPER;
}

static void doset(APTR obj, struct MPlayerGroupData *data, struct TagItem *tags)
{
	FORTAG(tags)
	{
		case MA_MPlayerGroup_Update:
			data->update = tag->ti_Data;
			break;
	}
	NEXTTAG
}

DEFSET(MPlayerGroup)
{
	struct MPlayerGroupData *data = INST_DATA(cl, obj);

	doset(obj, data, INITTAGS);

	return DOSUPER;
}

DEFMMETHOD(Setup)
{
	struct MPlayerGroupData *data = INST_DATA(cl, obj);

	if (!DOSUPER)
	{
		return (0);
	}

	data->ihnode.ihn_Object = obj;
	data->ihnode.ihn_Flags = MUIIHNF_TIMER;
	data->ihnode.ihn_Millis = 200;
	data->ihnode.ihn_Method = MM_MPlayerGroup_UpdateAll;

	DoMethod(app, MUIM_Application_AddInputHandler, &data->ihnode);

	data->ehnode.ehn_Object = obj;
	data->ehnode.ehn_Class = cl;
	data->ehnode.ehn_Events =  IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY | IDCMP_MOUSEMOVE | IDCMP_ACTIVEWINDOW | IDCMP_INACTIVEWINDOW;
	data->ehnode.ehn_Priority = 2; /* priority over MUI's areaclass */
	data->ehnode.ehn_Flags = MUI_EHF_GUIMODE;
	DoMethod(_win(obj), MUIM_Window_AddEventHandler, (IPTR)&data->ehnode);

	if(mygui->embedded)
	{
		set( _win(obj), MUIA_Window_DefaultObject, data->AR_Video );

		if(!mygui->fullscreen)
		{
			innertop = getv(obj, MUIA_InnerTop);
			innerbottom = getv(obj, MUIA_InnerBottom);
			innerright = getv(obj, MUIA_InnerRight);
			innerleft = getv(obj, MUIA_InnerLeft);
		}
	}
	return TRUE;
}

DEFMMETHOD(Cleanup)
{
	struct MPlayerGroupData *data = INST_DATA(cl, obj);

	DoMethod(_win(obj), MUIM_Window_RemEventHandler, (IPTR)&data->ehnode);
	DoMethod(app, MUIM_Application_RemInputHandler, &data->ihnode);

	return DOSUPER;
}

DEFMMETHOD(Show)
{
	IPTR rc = DOSUPER;

	if(mygui->embedded)
	{
		if(!mygui->fullscreen)
		{
			/* wbscreen is current window's screen */
			mygui->wbscreen = (APTR) getv(mygui->mainwindow, MUIA_Window_Screen);
		}
		else
		{
			/* use frontmost one */
			mygui->wbscreen = NULL;
		}

		mygui->videowindow = (struct Window *) getv(mygui->mainwindow, MUIA_Window);
		mygui->video_top = _top(mygui->videoarea);
		mygui->video_bottom = _bottom(mygui->videoarea);
		mygui->video_left = _left(mygui->videoarea);
		mygui->video_right = _right(mygui->videoarea);
	}
	else
	{
		/* use frontmost one */
		mygui->wbscreen = NULL;
	}


	return rc;
}

DEFSMETHOD(MPlayerGroup_Show)
{
	IPTR val = FALSE;

	switch(msg->show)
	{
		case MV_MPlayerGroup_Show_Off:
			val = FALSE;
			break;

		default:
		case MV_MPlayerGroup_Show_On:
			val = TRUE;
			break;

		case MV_MPlayerGroup_Show_Toggle:
			if(getv(mygui->mainwindow, MUIA_Window_Open))
			{
				val = FALSE;
			}
			else
			{
				val = TRUE;
			}
			break;
	}

	set(mygui->mainwindow, MUIA_Window_Open, val);

	return 0;
}

DEFTMETHOD(MPlayerGroup_ShowPlaylist)
{
	IPTR open = TRUE;

	if(getv(mygui->playlistwindow, MUIA_Window_Open))
	{
		open = FALSE;
	}
	else
	{
		open = TRUE;
	}

	set(mygui->playlistwindow, MUIA_Window_Open, open);

	return 0;
}

DEFTMETHOD(MPlayerGroup_ShowProperties)
{
	IPTR open = TRUE;

	if(getv(mygui->propertieswindow, MUIA_Window_Open))
	{
		open = FALSE;
	}
	else
	{
		open = TRUE;
	}

	set(mygui->propertieswindow, MUIA_Window_Open, open);

	return 0;
}

DEFTMETHOD(MPlayerGroup_ShowPreferences)
{
	DoMethod(mygui->prefsgroup, MM_PrefsGroup_Update);
	set(mygui->prefswindow, MUIA_Window_Open, TRUE);

	return 0;
}

// Show chapter cycle for streams that have a chapter concept (mkv, ...)
DEFSMETHOD(MPlayerGroup_ShowChapterBrowser)
{
	struct MPlayerGroupData *data = INST_DATA(cl, obj);

	set(data->PL_Chapters, MUIA_ShowMe, msg->show);
	set(data->PI_Chapters, MUIA_ShowMe, msg->show);

	if(msg->show)
	{
		DoMethod(obj, MM_MPlayerGroup_Update, MV_MPlayerGroup_Update_Lists, NULL);
	}

	return 0;
}

// Show chapter/title/angle cycles for DVD streams
DEFSMETHOD(MPlayerGroup_ShowDVDBrowser)
{
	struct MPlayerGroupData *data = INST_DATA(cl, obj);

	set(data->PL_Titles, MUIA_ShowMe, msg->show);
	set(data->PI_Titles, MUIA_ShowMe, msg->show);
	set(data->PL_Chapters, MUIA_ShowMe, msg->show);
	set(data->PI_Chapters, MUIA_ShowMe, msg->show);
	set(data->PL_Angles, MUIA_ShowMe, msg->show);
	set(data->PI_Angles, MUIA_ShowMe, msg->show);
	set(data->BT_DVDMenu, MUIA_ShowMe, msg->show ? (guiInfo.StreamType == STREAMTYPE_DVDNAV ? TRUE : FALSE) : FALSE);
	set(data->SP_DVDGroupSpacer, MUIA_ShowMe, msg->show ? (guiInfo.StreamType == STREAMTYPE_DVDNAV ? TRUE : FALSE) : FALSE);

	if(msg->show)
	{
		DoMethod(obj, MM_MPlayerGroup_Update, MV_MPlayerGroup_Update_Lists, NULL);
	}

	return 0;
}

DEFTMETHOD(MPlayerGroup_Play)
{
	mygui->playercontrol(evPlay, 0);
	return 0;
}

DEFTMETHOD(MPlayerGroup_Pause)
{
	mp_cmd_t * cmd = calloc(1, sizeof(*cmd));
	cmd->id=MP_CMD_FRAME_STEP;
	cmd->name=strdup("frame_step");
    mp_input_queue_cmd(cmd);
	return 0;
}

DEFTMETHOD(MPlayerGroup_Stop)
{
	mygui->playercontrol(evStop, 0);
	return 0;
}

DEFTMETHOD(MPlayerGroup_Prev)
{
	mygui->playercontrol(evPrev, 0);
	return 0;
}

DEFTMETHOD(MPlayerGroup_Next)
{
	mygui->playercontrol(evNext, 0);
	return 0;
}

DEFSMETHOD(MPlayerGroup_IncreaseSpeed)
{
	char cmd_str[256];
	float value = 0.f;

	switch(msg->value)
	{
		case MV_MPlayerGroup_IncreaseSpeed:
			value = 0.1f;
			break;
		case MV_MPlayerGroup_DecreaseSpeed:
			value = -0.1f;
			break;
	}

	snprintf(cmd_str, sizeof(cmd_str), "speed_incr %.2f", value);
    mp_input_queue_cmd(mp_input_parse_cmd(cmd_str));
	return 0;
}

DEFSMETHOD(MPlayerGroup_MultiplySpeed)
{
	char cmd_str[256];
	float value = 0.f;

	switch(msg->value)
	{
		case MV_MPlayerGroup_IncreaseSpeed:
			value = 0.1f;
			break;
		case MV_MPlayerGroup_DecreaseSpeed:
			value = -0.1f;
			break;
	}

	snprintf(cmd_str, sizeof(cmd_str), "speed_mult %.2f", value);
    mp_input_queue_cmd(mp_input_parse_cmd(cmd_str));
	return 0;
}

DEFSMETHOD(MPlayerGroup_Loop)
{
	switch(msg->which)
	{
		default:
		case MEN_REPEAT_OFF:
			gui_repeat_mode	= 0;
			break;
		case MEN_REPEAT_SINGLE:
			gui_repeat_mode	= 1;
			break;
		case MEN_REPEAT_PLAYLIST:
			gui_repeat_mode	= 2;
			break;
		case MEN_REPEAT_QUIT:
			gui_repeat_mode	= 3;
			break;
	}

	return 0;
}

DEFTMETHOD(MPlayerGroup_Record)
{
	DoMethod(mygui->menustrip, MUIM_GetUData, MEN_RECORD, MUIA_Menuitem_Checked, &mygui->dumpstream);

	return 0;
}

DEFTMETHOD(MPlayerGroup_OpenFileRequester)
{
	APTR tags[] = { (APTR) ASLFR_TitleText, (APTR) "Select File...",
					(APTR) ASLFR_InitialPattern, (APTR) MorphOS_GetPattern(),
					TAG_DONE };

	char ** files = NULL;
	char * ptr;
	ULONG count;

	if(guiInfo.Filename)
	{
		if(strstr(guiInfo.Filename, "://"))
			ptr = NULL;
		else
			ptr = guiInfo.Filename;
	}
	else
	{
		ptr = NULL;
	}

	if(ptr == NULL)
	{
		ptr = MorphOS_GetLastVisitedPath();
	}

	set(app, MUIA_Application_Sleep, TRUE);

	count = asl_run_multiple(ptr, (APTR)&tags, &files, TRUE);

	set(app, MUIA_Application_Sleep, FALSE);

	if (files)
	{
		ULONG i;

		// Play first file
		DoMethod(obj, MM_MPlayerGroup_Open, MV_MPlayerGroup_Open_File, files[0]);

		// Enqueue others
		for(i = 1; i < count; i++)
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
							adddirtoplaylist(mygui->playlist, files[i], TRUE, FALSE);
						}
						else
						{
							mygui->playlist->add_track(mygui->playlist, files[i], NULL, NULL, 0, STREAMTYPE_FILE);
						}
					}
					UnLock(l);
				}
			}
		}

		update_playlistwindow(TRUE);

		asl_free(count, files);
	}

	return 0;
}

DEFTMETHOD(MPlayerGroup_OpenPlaylistRequester)
{
	APTR tags[] = { (APTR) ASLFR_TitleText, (APTR) "Select Playlist...",
					(APTR) ASLFR_InitialPattern, (APTR) PATTERN_PLAYLIST,
				    TAG_DONE };
	STRPTR p;
	STRPTR ptr;

	if(guiInfo.Filename)
	{
		if(strstr(guiInfo.Filename, "://"))
			ptr = NULL;
		else
			ptr = guiInfo.Filename;
	}
	else
	{
		ptr = NULL;
	}

	if(ptr == NULL)
	{
		ptr = MorphOS_GetLastVisitedPath();
	}

	set(app, MUIA_Application_Sleep, TRUE);

	p = asl_run(ptr, (APTR)&tags, FALSE);

	set(app, MUIA_Application_Sleep, FALSE);

	if (p)
	{
		DoMethod(obj, MM_MPlayerGroup_Open, MV_MPlayerGroup_Open_Playlist, p);
		FreeVecTaskPooled(p);
	}

	return 0;
}
DEFTMETHOD(MPlayerGroup_OpenDirectoryRequester)
{
	APTR tags[] = { (APTR) ASLFR_TitleText, (APTR) "Select Directory...",
					(APTR) ASLFR_DrawersOnly, (APTR) TRUE,
				    TAG_DONE };
	STRPTR p;
	STRPTR ptr;

	ptr = MorphOS_GetLastVisitedPath();

	set(app, MUIA_Application_Sleep, TRUE);

	p = asl_run(ptr, (APTR)&tags, FALSE);

	set(app, MUIA_Application_Sleep, FALSE);

	if (p)
	{
		DoMethod(obj, MM_MPlayerGroup_Open, MV_MPlayerGroup_Open_Directory, p);
		FreeVecTaskPooled(p);
	}

	return 0;
}

DEFTMETHOD(MPlayerGroup_OpenDVDDirectoryRequester)
{
	set(mygui->dvddirwindow, MUIA_Window_Open, TRUE);
	return 0;
}

DEFTMETHOD(MPlayerGroup_OpenURLRequester)
{
	set(mygui->urlwindow, MUIA_Window_Open, TRUE);
	return 0;
}

DEFSMETHOD(MPlayer_OpenStream)
{
	if(msg->url)
	{
		DoMethod(obj, MM_MPlayerGroup_Open, MV_MPlayerGroup_Open_Stream, msg->url);
	}

	set(mygui->urlwindow, MUIA_Window_Open, FALSE);

	return 0;
}

DEFTMETHOD(MPlayerGroup_CycleSubtitles)
{
	mp_input_queue_cmd(mp_input_parse_cmd("sub_select"));
	return 0;
}

DEFTMETHOD(MPlayerGroup_OpenSubtitleRequester)
{
	APTR tags[] = { (APTR) ASLFR_TitleText, (APTR) "Select Subtitles...",
					(APTR) ASLFR_InitialPattern, (APTR) PATTERN_SUBTITLE,
				    TAG_DONE };
	STRPTR p;
	STRPTR ptr;
	STRPTR dir = NULL;

    if (!guiInfo.Playing)
    {
		return 0;
    }

	ptr = guiInfo.SubtitleFilename;

	if(ptr == NULL)
	{
		// Get the containing directory
		if(guiInfo.Filename)
		{
			ptr = guiInfo.Filename;

			dir = (STRPTR) malloc(strlen(ptr) + 1);

			if(dir)
			{
				stccpy(dir, ptr, FilePart(ptr) - ptr + 1);
				ptr = dir;
			}
		}
	}

	set(app, MUIA_Application_Sleep, TRUE);

	p = asl_run(ptr, (APTR)&tags, FALSE);

	set(app, MUIA_Application_Sleep, FALSE);

	free(dir);

	if (p)
	{
		char cmd_str[256];
		guiSetFilename(guiInfo.SubtitleFilename, p);
		FreeVecTaskPooled(p);

		snprintf(cmd_str, sizeof(cmd_str), "sub_load \"%s\"", guiInfo.SubtitleFilename);
		mp_input_queue_cmd(mp_input_parse_cmd(cmd_str));

		snprintf(cmd_str, sizeof(cmd_str), "sub_select %d", guiInfo.mpcontext->global_sub_size);
		mp_input_queue_cmd(mp_input_parse_cmd(cmd_str));
	}
	return 0;
}

DEFSMETHOD(MPlayerGroup_SelectSubtitle)
{
	char cmd_str[64];

	snprintf(cmd_str, sizeof(cmd_str), "sub_select %d", (int) msg->which);
	mp_input_queue_cmd(mp_input_parse_cmd(cmd_str));

	return 0;
}

DEFSMETHOD(MPlayerGroup_UnloadSubtitles)
{
	int which;
	char cmd_str[64];
	
	if(msg->which == MV_MPlayerGroup_UnloadSubtitles_All)
	{
		which = -1;
	}
	else
	{
		which = msg->which;
	}

	snprintf(cmd_str, sizeof(cmd_str), "sub_remove %d", which);
	mp_input_queue_cmd(mp_input_parse_cmd(cmd_str));

	guiClearSubtitle();

	return 0;
}

extern int sub_source(MPContext * mpctx);
extern int dvd_lang_from_sid(stream_t *stream, int id);
extern int mp_dvdnav_lang_from_sid(stream_t *stream, int sid);

static char * GetSubtitleLabel(int pos, char * buffer, int size)
{
	int source = sub_source(guiInfo.mpcontext);

	if(source == SUB_SOURCE_DEMUX)
	{
		if (vo_spudec && guiInfo.mpcontext->stream->type == STREAMTYPE_DVD && pos >= 0)
		{
		    char lang[3];
			int code = dvd_lang_from_sid(guiInfo.mpcontext->stream, pos);
		    lang[0] = code >> 8;
		    lang[1] = code;
		    lang[2] = 0;
		 	snprintf(buffer, size, "(%d) %s", pos, lang);
		}
		else if(vo_spudec && guiInfo.mpcontext->stream->type == STREAMTYPE_DVDNAV && pos >= 0)
		{
		    int code;
			if ((code = mp_dvdnav_lang_from_sid(guiInfo.mpcontext->stream, pos)))
			{
			    char lang[3];
	            lang[0] = code >> 8;
	            lang[1] = code;
	            lang[2] = 0;
				snprintf(buffer, size, "(%d) %s", pos, lang);
			}
			else
			{
				snprintf(buffer, size, "(%d) %s", pos, MSGTR_Unknown);
			}
		}
		else
		{
			snprintf(buffer, size, "(%d) %s", pos, MSGTR_Unknown);
		}

		return buffer;
	}
	else if(source == SUB_SOURCE_VOBSUB)
	{
		const char *language = MSGTR_Unknown;
		language = vobsub_get_id(vo_vobsub, (unsigned int) pos);
		snprintf(buffer, size, "(%d) %s", pos, language ? language : MSGTR_Unknown);
		return buffer;
	}
	else if(source == SUB_SOURCE_SUBS)
	{
		stccpy(buffer, guiInfo.mpcontext->set_of_subtitles[pos]->filename, size);
		return buffer;
	}

	return NULL;
}

DEFTMETHOD(MPlayerGroup_RefreshSubtitles)
{
	APTR menu = (APTR) DoMethod(mygui->menustrip, MUIM_FindUData, MEN_SELECTSUBTITLE);

	if(menu)
	{
		int i;

		DoMethod(menu, MUIM_Menustrip_InitChange);

		for (i = 0; i < 32; i++)
		{
			APTR entry = (APTR) DoMethod(menu, MUIM_FindUData, MEN_SUBTITLEBASE+i);
			if(entry)
			{
				DoMethod(menu, MUIM_Family_Remove, entry);
			}
		}

		set(menu, MUIA_Menuitem_Enabled, guiInfo.mpcontext->global_sub_size > 0);

		for (i = 0; i < guiInfo.mpcontext->global_sub_size && i < 32; i++)
		{
			char label[256];
			char *ptr = GetSubtitleLabel(i, label, sizeof(label));

			if(ptr)
			{
				APTR o;
				ULONG j;
				ULONG bits;

				j = guiInfo.mpcontext->global_sub_size;
				bits = 0;

				while (j--)
				{
					if (j != i)
					{
						bits |= 1 << j;
					}
				}

				DoMethod(menu, MUIM_Family_AddTail, o = MUI_MakeObject(MUIO_Menuitem, strdup(ptr), 0, (CHECKIT | MENUTOGGLE), MEN_SUBTITLEBASE+i) );
				set(o, MUIA_Menuitem_Exclude, bits);
			}
		}

		for (i = 0; i < guiInfo.mpcontext->global_sub_size && i < 32; i++)
		{
			DoMethod(menu, MUIM_SetUData, MEN_SUBTITLEBASE+i, MUIA_Menuitem_Checked, i == guiInfo.mpcontext->global_sub_pos);
		}

		DoMethod(menu, MUIM_Menustrip_ExitChange);
	}
	return 0;
}

DEFTMETHOD(MPlayerGroup_IncreaseSubtitleDelay)
{
	mp_input_queue_cmd(mp_input_parse_cmd("sub_delay -0.1"));
	return 0;
}

DEFTMETHOD(MPlayerGroup_DecreaseSubtitleDelay)
{
	mp_input_queue_cmd(mp_input_parse_cmd("sub_delay 0.1"));
	return 0;
}

DEFSMETHOD(MPlayerGroup_SelectAudio)
{
	char cmd_str[64];
	int id = 0;

	if (guiInfo.mpcontext->stream->type == STREAMTYPE_DVD || guiInfo.mpcontext->stream->type == STREAMTYPE_DVDNAV)
	{
		id = (int) guiInfo.AudioStream[msg->which].id;
	}
	else
	{
		id = msg->which;
	}

	snprintf(cmd_str, sizeof(cmd_str), "switch_audio %d", id);
	mp_input_queue_cmd(mp_input_parse_cmd(cmd_str));

	return 0;
}

extern char *dvd_audio_stream_channels[6], *dvd_audio_stream_types[8];

static char * GetAudioLabel(int pos, char * buffer, int size)
{
	if ((guiInfo.mpcontext->stream->type == STREAMTYPE_DVD || guiInfo.mpcontext->stream->type == STREAMTYPE_DVDNAV) && pos >= 0)
	{
		char lang[3];
		int code = guiInfo.AudioStream[pos].language;
	    lang[0] = code >> 8;
	    lang[1] = code;
	    lang[2] = 0;
		snprintf(buffer, size, "(%d) %s - %s (%s)",
				 guiInfo.AudioStream[pos].id,
				 lang,
				 dvd_audio_stream_types[guiInfo.AudioStream[pos].type],
				 dvd_audio_stream_channels[guiInfo.AudioStream[pos].channels]);

		return buffer;
	}
	else
	{
		snprintf(buffer, size, "(%d) %s", pos, MSGTR_Unknown);
		return buffer;
	}

	return NULL;
}

DEFTMETHOD(MPlayerGroup_RefreshAudio)
{
	APTR menu = (APTR) DoMethod(mygui->menustrip, MUIM_FindUData, MEN_AUDIOTRACK);

	if(menu)
	{
		int i;

		DoMethod(menu, MUIM_Menustrip_InitChange);

		for (i = 0; i < 32; i++)
		{
			APTR entry = (APTR) DoMethod(menu, MUIM_FindUData, MEN_AUDIOTRACKBASE+i);
			if(entry)
			{
				DoMethod(menu, MUIM_Family_Remove, entry);
			}
		}

		set(menu, MUIA_Menuitem_Enabled, FALSE);

		if ((guiInfo.mpcontext->stream->type == STREAMTYPE_DVD || guiInfo.mpcontext->stream->type == STREAMTYPE_DVDNAV))
		{
			if(guiInfo.AudioStreams)
			{
				set(menu, MUIA_Menuitem_Enabled, TRUE);
			}

			for (i = 0; i < guiInfo.AudioStreams && i < 32; i++)
			{
				char label[256];
				char *ptr = GetAudioLabel(i, label, sizeof(label));

				if(ptr)
				{
					APTR o;
					ULONG j;
					ULONG bits;

					j = guiInfo.AudioStreams;
					bits = 0;

					while (j--)
					{
						if (j != i)
						{
							bits |= 1 << j;
						}
					}

					DoMethod(menu, MUIM_Family_AddTail, o = MUI_MakeObject(MUIO_Menuitem, strdup(ptr), 0, (CHECKIT | MENUTOGGLE), MEN_AUDIOTRACKBASE+i) );
					set(o, MUIA_Menuitem_Exclude, bits);
				}
			}

			for(i = 0; i < guiInfo.AudioStreams && i < 32; i++)
			{
				if(guiInfo.AudioStream[i].id == audio_id)
				{
					DoMethod(menu, MUIM_SetUData, MEN_AUDIOTRACKBASE+i, MUIA_Menuitem_Checked, TRUE);
					break;
				}
			}
		}
		else
		{
			int channels = 0;
			
			if(guiInfo.mpcontext && guiInfo.mpcontext->demuxer && guiInfo.mpcontext->demuxer->a_streams)
			{
				int current = 0;

				for(i = 0; i < MAX_A_STREAMS; i++)
				{
					if(guiInfo.mpcontext->demuxer->a_streams[i])
					{
						channels++;
					}
				}

				if(channels)
				{
					set(menu, MUIA_Menuitem_Enabled, TRUE);
				}

				for (i = 0; i < 32; i++)
				{
					sh_audio_t *sh = guiInfo.mpcontext->demuxer->a_streams[i];

					if(sh)
					{
						char label[256];
						int id = (int) sh->aid;
						char *ptr = GetAudioLabel(id, label, sizeof(label));

						if(ptr)
						{
							APTR o;
							ULONG j;
							ULONG bits;

							j = channels;
							bits = 0;

							while (j--)
							{
								if (j != current)
								{
									bits |= 1 << j;
								}
							}

							DoMethod(menu, MUIM_Family_AddTail, o = MUI_MakeObject(MUIO_Menuitem, strdup(ptr), 0, (CHECKIT | MENUTOGGLE), MEN_AUDIOTRACKBASE+i) );
							set(o, MUIA_Menuitem_Exclude, bits);
						}

						current++;
					}
				}

				current = 0;

				for(i = 0; i < 32; i++)
				{
					sh_audio_t *sh = guiInfo.mpcontext->demuxer->a_streams[i];

					if(sh)
					{
						if(i == audio_id || (audio_id < 0 && current == 0))
						{
							DoMethod(menu, MUIM_SetUData, MEN_AUDIOTRACKBASE+i, MUIA_Menuitem_Checked, TRUE);
							break;
						}

						current++;
					}
				}			 
			}
		}

		if(audio_id < 0)
		{
			DoMethod(menu, MUIM_SetUData, MEN_AUDIOTRACKBASE+0, MUIA_Menuitem_Checked, TRUE);
		}

		DoMethod(menu, MUIM_Menustrip_ExitChange);
	}

	return 0;
}

DEFTMETHOD(MPlayerGroup_IncreaseVolume)
{
	mp_input_queue_cmd(mp_input_parse_cmd("volume 1"));
	return 0;
}

DEFTMETHOD(MPlayerGroup_DecreaseVolume)
{
	mp_input_queue_cmd(mp_input_parse_cmd("volume -1"));
	return 0;
}

DEFTMETHOD(MPlayerGroup_IncreaseAVDelay)
{
	mp_input_queue_cmd(mp_input_parse_cmd("audio_delay -0.100"));
	return 0;
}

DEFTMETHOD(MPlayerGroup_DecreaseAVDelay)
{
	mp_input_queue_cmd(mp_input_parse_cmd("audio_delay 0.100"));
	return 0;
}

extern af_cfg_t af_cfg;

DEFSMETHOD(MPlayerGroup_AudioFilter)
{
	int	i;
	ULONG checked;
	char index[10];
	char * action;
	char * filter = NULL;
	DoMethod(mygui->menustrip, MUIM_GetUData, msg->which, MUIA_Menuitem_Checked, &checked);

	action = checked ? "af-add" : "af-del";

	switch(msg->which)
	{
		case MEN_EXTRASTEREO:
			filter = "extrastereo";
			break;
		case MEN_VOLNORM:
			filter = "volnorm=2";
			break;
		case MEN_KARAOKE:
			filter = "karaoke";
			break;
		case MEN_SCALETEMPO:
			filter = "scaletempo";
			break;
	}

	if(checked)
	{
		m_config_set_option(mconfig, action, filter);

		if(msg->which == MEN_KARAOKE)
			m_config_set_option(mconfig, "af-add", "format=s16be");
	}
	else
	{
        for (i = 0; af_cfg.list [i]; ++i)
		{
			if(!strcmp(filter, af_cfg.list[i]))
			{
				snprintf(index, sizeof(index), "%d", i);
				break;
			}
		}	 

		m_config_set_option(mconfig, action, index);
	}

	mygui->playercontrol(evLoadPlay, 0);

	return 0;
}

static VOID MPlayerGroup_AfDel(char *filter)
{
	char index[10];
	int i;

	if(af_cfg.list)
	{
	    for (i = 0; af_cfg.list [i]; ++i)
		{
			if(strstr(af_cfg.list[i], filter))
			{
				snprintf(index, sizeof(index), "%d", i);
				break;
			}
		}

		m_config_set_option(mconfig, "af-del", index);
	}
}

DEFSMETHOD(MPlayerGroup_AudioGain)
{
	char *filter = "volume=";

	MPlayerGroup_AfDel(filter);

	if(msg->enable)
	{
		char full[256];
		snprintf(full, sizeof(full), "%s%ld", filter, msg->gain);
		m_config_set_option(mconfig, "af-add", full);
	}

	mygui->playercontrol(evLoadPlay, 0);

	return 0;
}

DEFSMETHOD(MPlayerGroup_Equalizer)
{
	char *filter = "equalizer=";

	MPlayerGroup_AfDel(filter);

	if(msg->enable)
	{
		char full[256];
		snprintf(full, sizeof(full), "%s%s", filter, msg->param);
		m_config_set_option(mconfig, "af-add", full);
	}

	mygui->playercontrol(evLoadPlay, 0);

	return 0;
}

DEFSMETHOD(MPlayerGroup_Open)
{
	switch(msg->mode)
	{
		case MV_MPlayerGroup_Open_Playlist:
		case MV_MPlayerGroup_Open_File:

			if(!msg->url)
				break;

			uiSetFileName(NULL, msg->url, STREAMTYPE_FILE);

			mygui->playlist->clear_playlist(mygui->playlist);

			if(!parse_filename(msg->url, playtree, mconfig, 0))
				mygui->playlist->add_track(mygui->playlist, msg->url, NULL, NULL, 0, STREAMTYPE_FILE);

			update_playlistwindow(TRUE);

			mygui->playercontrol(evLoadPlay, 0);

			break;

		case MV_MPlayerGroup_Open_DVD:
			free(dvd_device);
			dvd_device = strdup(cfg_dvd_device);
			mygui->playercontrol(evPlayDVD, 0);
			break;

		case MV_MPlayerGroup_Open_DVD_Directory:
			if(!msg->url)
				break;
			free(dvd_device);
			dvd_device = strdup(msg->url);
			mygui->playercontrol(evPlayDVD, 0);
			break;

		case MV_MPlayerGroup_Open_Stream:

			if(!msg->url)
				break;

			uiSetFileName(NULL, msg->url, STREAMTYPE_STREAM);

			mygui->playlist->clear_playlist(mygui->playlist);

			if(!parse_filename(msg->url, playtree, mconfig, 0))
				mygui->playlist->add_track(mygui->playlist, msg->url, NULL, msg->url, 0, STREAMTYPE_STREAM);

			update_playlistwindow(TRUE);

			mygui->playercontrol(evLoadPlay, 0);

			break;

		case MV_MPlayerGroup_Open_Directory:

			if(!msg->url)
				break;

			mygui->playlist->clear_playlist(mygui->playlist);

			adddirtoplaylist(mygui->playlist, msg->url, TRUE, FALSE);

			if(mygui->playlist->trackcount)
			{
				uiSetFileName(NULL, mygui->playlist->tracks[0]->filename, STREAMTYPE_FILE);

				update_playlistwindow(TRUE);

				mygui->playercontrol(evLoadPlay, 0);
			}

			break;
	}

	return 0;
}

DEFTMETHOD(MPlayerGroup_ToggleFullScreen)
{
	set(obj, MA_MPlayerGroup_Update, TRUE);
	mygui->playercontrol(evFullScreen, 0);
	return 0;
}

DEFTMETHOD(MPlayerGroup_Screenshot)
{
   if(!guiInfo.sh_video) return 0;

	mp_input_queue_cmd(mp_input_parse_cmd("screenshot"));
	return 0;
}

DEFSMETHOD(MPlayerGroup_Aspect)
{
	char cmd_str[64];
	float ratio;

	switch(msg->ratio)
	{
		default:
		case MEN_ASPECT_AUTO:
			ratio = -1;
			break;
		case MEN_ASPECT_4_3:
			ratio = 4.0/3.0;
			break;
		case MEN_ASPECT_5_4:
			ratio = 5.0/3.0;
			break;
		case MEN_ASPECT_16_9:
			ratio = 16.0/9.0;
			break;
		case MEN_ASPECT_16_10:
			ratio = 16.0/10.0;
			break;
	}

	snprintf(cmd_str, sizeof(cmd_str), "switch_ratio %f", ratio);
	mp_input_queue_cmd(mp_input_parse_cmd(cmd_str));

	return 0;
}

DEFSMETHOD(MPlayerGroup_Dimensions)
{
	float factor;
	float width, height;

	switch(msg->which)
	{
		default:
		case MEN_DIMENSIONS_FREE:
			gui_window_dimensions = 0;
			return 0; // nothing else needed there.
			break;

		case MEN_DIMENSIONS_ORIGINAL:
			gui_window_dimensions = 1;
			factor = 1.0f;
			break;
		case MEN_DIMENSIONS_HALF:
			gui_window_dimensions = 2;
			factor = 0.5f;
			break;
		case MEN_DIMENSIONS_DOUBLE:
			gui_window_dimensions = 3;
			factor = 2.0f;
			break;
	}

	if(!guiInfo.sh_video || mygui->fullscreen || !mygui->embedded) /* XXX: implement for separated mode */
	{
		return 0;
	}

	width =  getv(mygui->mainwindow, MUIA_Window_Width) - (mygui->video_right - mygui->video_left + 1) + factor*guiInfo.VideoWidth;
	height = getv(mygui->mainwindow, MUIA_Window_Height) - (mygui->video_bottom - mygui->video_top + 1) + factor*guiInfo.VideoHeight;

	if(mygui->screen)
	{
		width = MIN(width, ((struct Screen *)mygui->screen)->Width);
		height = MIN(height, ((struct Screen *) mygui->screen)->Height);
	}

	set(mygui->mainwindow, MUIA_Window_ID, 0);

	SetAttrs(mygui->mainwindow,
			 MUIA_Window_Width, (IPTR) width,
			 MUIA_Window_Height,(IPTR) height,
			 TAG_DONE);

	set(mygui->mainwindow, MUIA_Window_ID, MAKE_ID('M','A','I','N'));

	return 0;
}

extern m_obj_settings_t* vf_settings;

static void clearfilter(char *filter, char *filterarg)
{
	int i;
	char index[10];

	for (i = 0; vf_settings[i].name; ++i)
	{
		if(!strcmp(vf_settings[i].name, filter))
		{
			int j;

			snprintf(index, sizeof(index), "%d", i);

			if(filterarg)
			{
				for(j = 0; vf_settings[i].attribs[j]; j++)
				{
					if(!strcmp(vf_settings[i].attribs[j], filterarg))
					{
						m_config_set_option(mconfig, "vf-del", index);
						return;
					}
				}
			}
			else
			{
				m_config_set_option(mconfig, "vf-del", index);
				return;
			}
		}
	}
}

DEFSMETHOD(MPlayerGroup_Deinterlacer)
{
	ULONG checked;
	char * filter = NULL;
	char * filterarg = NULL;
	char full[256];

	DoMethod(mygui->menustrip, MUIM_GetUData, msg->which, MUIA_Menuitem_Checked, &checked);

	switch(msg->which)
	{
		case MEN_DEINTERLACER_L5:
			filter = "pp";
			filterarg = "l5";
			break;
		case MEN_DEINTERLACER_YADIF:
			filter = "yadif";
			break;
		case MEN_DEINTERLACER_YADIF1:
			filter = "yadif";
			filterarg = "1";
			break;
		case MEN_DEINTERLACER_KERNDEINT:
			filter = "kerndeint";
			filterarg = "5";
			break;
		case MEN_DEINTERLACER_LB:
			filter = "pp";
			filterarg = "lb";
			break;
	}

	if(checked)
	{
		snprintf(full, sizeof(full), "%s=%s", filter, filterarg);
		m_config_set_option(mconfig, "vf-add", full);
	}
	else
	{
		clearfilter(filter, filterarg);
	}

	mygui->playercontrol(evLoadPlay, 0);

	return 0;
}

DEFSMETHOD(MPlayerGroup_VideoFilter)
{
	ULONG checked;
	char full[256];
	char * filter = NULL;
	char * filterarg = NULL;

	DoMethod(mygui->menustrip, MUIM_GetUData, msg->which, MUIA_Menuitem_Checked, &checked);

	switch(msg->which)
	{
		case MEN_DENOISE_NORMAL:
			filter = "hqdn3d";
			break;
		case MEN_DENOISE_SOFT:
			filter = "hqdn3d";
			filterarg = "2:1:2";
			break;
		case MEN_DEBLOCK:
			filter = "pp";
			filterarg = "vb/hb";
			break;
		case MEN_DERING:
			filter = "pp";
			filterarg = "dr";
			break;
	}

	if(checked)
	{
		snprintf(full, sizeof(full), "%s=%s", filter, filterarg);
		m_config_set_option(mconfig, "vf-add", full);
	}
	else
	{
		clearfilter(filter, filterarg);
	}

	mygui->playercontrol(evLoadPlay, 0);

	return 0;
}

DEFSMETHOD(MPlayerGroup_Rotation)
{
	ULONG checked;
	char full[256];
	char * filter = NULL;
	char * filterarg = NULL;

	DoMethod(mygui->menustrip, MUIM_GetUData, msg->which, MUIA_Menuitem_Checked, &checked);
	
	switch(msg->which)
	{
		case MEN_ROTATE_1:
			filter = "rotate";
			filterarg = "0";
			break;
		case MEN_ROTATE_2:
			filter = "rotate";
			filterarg = "1";
			break;
		case MEN_ROTATE_3:
			filter = "rotate";
			filterarg = "2";
			break;
		case MEN_ROTATE_4:
			filter = "rotate";
			filterarg = "3";
			break;
	}

	clearfilter(filter, NULL);

	if(checked)
	{
		snprintf(full, sizeof(full), "%s=%s", filter, filterarg);
		m_config_set_option(mconfig, "vf-add", full);
	}

	mygui->playercontrol(evLoadPlay, 0);

	return 0;
}

DEFSMETHOD(MPlayerGroup_Mirror)
{
	ULONG checked;
	char * filter = "mirror";

	DoMethod(mygui->menustrip, MUIM_GetUData, msg->which, MUIA_Menuitem_Checked, &checked);

	if(checked)
	{
		if(filter) m_config_set_option(mconfig, "vf-add", filter);
	}
	else
	{
		clearfilter(filter, NULL);
	}

	mygui->playercontrol(evLoadPlay, 0);

	return 0;
}

DEFSMETHOD(MPlayerGroup_Flip)
{
	ULONG checked;
	char * filter = "harddup,flip";

	DoMethod(mygui->menustrip, MUIM_GetUData, msg->which, MUIA_Menuitem_Checked, &checked);

	if(checked)
	{
		if(filter) m_config_set_option(mconfig, "vf-add", filter);
	}
	else
	{
		clearfilter("flip", NULL);
		clearfilter("harddup", NULL);
	}

	mygui->playercontrol(evLoadPlay, 0);

	return 0;
}

DEFSMETHOD(MPlayerGroup_Crop)
{
	char * filter = "crop";

	if(msg->enable)
	{
		char full[256];
		snprintf(full, sizeof(full), "%s=%ld:%ld:%ld:%ld", filter, msg->width, msg->height, msg->left, msg->top);
		clearfilter(filter, NULL);
		m_config_set_option(mconfig, "vf-add", full);
	}
	else
	{
		clearfilter(filter, NULL);
	}

	mygui->playercontrol(evLoadPlay, 0);

	return 0;
}

DEFSMETHOD(MPlayerGroup_Scale)
{
	char * filter = "scale";

	if(msg->enable)
	{
		char full[256];
		snprintf(full, sizeof(full), "%s=%ld:%ld", filter, msg->width, msg->height);
		clearfilter(filter, NULL);
		m_config_set_option(mconfig, "vf-add", full);
	}
	else
	{
		clearfilter(filter, NULL);
	}

	mygui->playercontrol(evLoadPlay, 0);

	return 0;
}

DEFTMETHOD(MPlayerGroup_SmartSeek)
{
	DoMethod(app, MUIM_Application_PushMethod, obj, 2 | MUIV_PushMethod_Delay(50) | MUIF_PUSHMETHOD_SINGLE, MM_MPlayerGroup_Seek, MV_MPlayerGroup_SeekFromSlider);
	return 0;
}

DEFSMETHOD(MPlayerGroup_Seek)
{
	struct MPlayerGroupData *data = INST_DATA(cl, obj);
	int event;

	switch(msg->mode)
	{
		case MV_MPlayerGroup_SeekFromRewind:
			switch(msg->value)
			{
				default:
				case 10:
					event = evBackward10sec;
					break;
				case 60:
					event = evBackward1min;
					break;
				case 600:
					event = evBackward10min;
					break;
			}
			mygui->playercontrol(event, 0);
			break;

		case MV_MPlayerGroup_SeekFromForward:
			switch(msg->value)
			{
				default:
				case 10:
					event = evForward10sec;
					break;
				case 60:
					event = evForward1min;
					break;
				case 600:
					event = evForward10min;
					break;
			}
			mygui->playercontrol(event, 0);
			break;

		case MV_MPlayerGroup_SeekFromSlider:
			guiInfo.Position  = 100.f * ((float) getv(data->SL_Seek, MUIA_Slider_Level))/((float)guiInfo.RunningTime);
			mygui->playercontrol(evSetMoviePosition, guiInfo.Position);
			break;
	}

	return 0;
}

DEFTMETHOD(MPlayerGroup_Volume)
{
	struct MPlayerGroupData *data = INST_DATA(cl, obj);

	guiInfo.Volume = (float) getv(data->SL_Volume, MUIA_Slider_Level);

	mygui->playercontrol(evSetVolume, 0);

	return 0;
}

DEFTMETHOD(MPlayerGroup_Mute)
{
	mygui->playercontrol(evMute, 0);
	return 0;
}

DEFTMETHOD(MPlayerGroup_ShowDVDMenu)
{
	mp_input_queue_cmd(mp_input_parse_cmd("dvdnav menu"));
	return 0;
}

DEFTMETHOD(MPlayerGroup_BuildChapterList)
{
	struct MPlayerGroupData *data = INST_DATA(cl, obj);
	int i;

	DoMethod(data->LI_Chapters, MUIM_List_Clear);

	switch(guiInfo.StreamType)
	{
		case STREAMTYPE_DVDNAV:
		case STREAMTYPE_DVD:
			for(i = 0; i < guiInfo.Chapters; i++)
			{
				char buffer[256];
				snprintf(buffer, sizeof(buffer), "Chapter %d", i + 1);
				DoMethod(data->LI_Chapters, MUIM_List_InsertSingle, buffer, MUIV_List_Insert_Bottom);
			}
			break;
		default:
			if(guiInfo.demuxer)
			{
				for(i = 0; i < /*demuxer_chapter_count(guiInfo.mpcontext->demuxer)*/guiInfo.Chapters; i++)
				{
					char buffer[256];
					// Safe from another thread?
					snprintf(buffer, sizeof(buffer), "Chapter %d: %s", i + 1, demuxer_chapter_name(guiInfo.mpcontext->demuxer, i));
					DoMethod(data->LI_Chapters, MUIM_List_InsertSingle, buffer, MUIV_List_Insert_Bottom);
				}
			}
            break;
	}

	return 0;
}

DEFSMETHOD(MPlayerGroup_SetChapter)
{
	struct MPlayerGroupData *data = INST_DATA(cl, obj);
	struct PopListEntry * entry;

	switch(guiInfo.StreamType)
	{
		case STREAMTYPE_DVDNAV:
		case STREAMTYPE_DVD:
		{
			if(!msg->update)
			{
				nnset(data->LI_Chapters, MUIA_List_Active, guiInfo.Chapter - 1);

				DoMethod(data->LI_Chapters, MUIM_List_GetEntry, MUIV_List_GetEntry_Active, &entry);

				if(entry)
				{
					set(data->ST_Chapters, MUIA_Text_Contents, entry->label);
				}
			}
			else
			{
				DoMethod(data->LI_Chapters, MUIM_List_GetEntry, MUIV_List_GetEntry_Active, &entry);

				if(entry)
				{
					set(data->ST_Chapters, MUIA_Text_Contents, entry->label);
					mygui->playercontrol(ivSetDVDChapter, getv(data->LI_Chapters, MUIA_List_Active));
				}
			}
			break;
		}

		default:
		{
			if(!msg->update)
			{
				if(guiInfo.mpcontext && guiInfo.mpcontext->demuxer)
				{
					nnset(data->LI_Chapters, MUIA_List_Active, demuxer_get_current_chapter(guiInfo.mpcontext->demuxer));

					DoMethod(data->LI_Chapters, MUIM_List_GetEntry, MUIV_List_GetEntry_Active, &entry);

					if(entry)
					{
						set(data->ST_Chapters, MUIA_Text_Contents, entry->label);
					}			 
				}
			}
			else
			{
				char command[64];
				snprintf(command, sizeof(command), "seek_chapter %d 1", getv(data->LI_Chapters, MUIA_List_Active));
				mp_input_queue_cmd(mp_input_parse_cmd(command));
			}

			break;
		}
	}

	return 0;
}

DEFTMETHOD(MPlayerGroup_BuildTitleList)
{
	struct MPlayerGroupData *data = INST_DATA(cl, obj);
	int i;

	DoMethod(data->LI_Titles, MUIM_List_Clear);

	switch(guiInfo.StreamType)
	{
		case STREAMTYPE_DVDNAV:
		case STREAMTYPE_DVD:
			for(i = 1; i <= guiInfo.Tracks; i++)
			{
				char buffer[64];
				snprintf(buffer, sizeof(buffer), "Title %d", i);
				DoMethod(data->LI_Titles, MUIM_List_InsertSingle, buffer, MUIV_List_Insert_Bottom);
			}
			break;

		default:
			;
	}

	return 0;
}

DEFSMETHOD(MPlayerGroup_SetTitle)
{
	struct MPlayerGroupData *data = INST_DATA(cl, obj);
	struct PopListEntry * entry;

	if(!msg->update)
	{
		nnset(data->LI_Titles, MUIA_List_Active, guiInfo.Track - 1);

		DoMethod(data->LI_Titles, MUIM_List_GetEntry, MUIV_List_GetEntry_Active, &entry);

		if(entry)
		{
			set(data->ST_Titles, MUIA_Text_Contents, entry->label);
		}
	}
	else
	{
		DoMethod(data->LI_Titles, MUIM_List_GetEntry, MUIV_List_GetEntry_Active, &entry);

		if(entry)
		{
			set(data->ST_Titles, MUIA_Text_Contents, entry->label);;
			mygui->playercontrol(ivSetDVDTitle, getv(data->LI_Titles, MUIA_List_Active) + 1);
		}
	}

	return 0;
}

DEFTMETHOD(MPlayerGroup_BuildAngleList)
{
	struct MPlayerGroupData *data = INST_DATA(cl, obj);
	int i;

	DoMethod(data->LI_Angles, MUIM_List_Clear);

	switch(guiInfo.StreamType)
	{
		case STREAMTYPE_DVDNAV:
		case STREAMTYPE_DVD:
			for(i = 1; i <= guiInfo.Angles; i++)
			{
				char buffer[64];
				snprintf(buffer, sizeof(buffer), "Angle %d", i);
				DoMethod(data->LI_Angles, MUIM_List_InsertSingle, buffer, MUIV_List_Insert_Bottom);
			}
			break;

		default:
			;
	}

	return 0;
}

DEFSMETHOD(MPlayerGroup_SetAngle)
{
	struct MPlayerGroupData *data = INST_DATA(cl, obj);
	struct PopListEntry * entry;

	if(!msg->update)
	{
		nnset(data->LI_Angles, MUIA_List_Active, guiInfo.Angle - 1);

		DoMethod(data->LI_Angles, MUIM_List_GetEntry, MUIV_List_GetEntry_Active, &entry);

		if(entry)
		{
			set(data->ST_Angles, MUIA_Text_Contents, entry->label);
		}
	}
	else
	{
		DoMethod(data->LI_Angles, MUIM_List_GetEntry, MUIV_List_GetEntry_Active, &entry);

		if(entry)
		{
			set(data->ST_Angles, MUIA_Text_Contents, entry->label);
			mygui->playercontrol(evSetDVDAngle, getv(data->LI_Angles, MUIA_List_Active) + 1);
		}
	}

	return 0;
}

DEFSMETHOD(MPlayerGroup_HandleMenu)
{
	switch((IPTR) msg->userdata)
	{
		/* Project */
		case MEN_FILE:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_OpenFileRequester);
			break;

		case MEN_PLAYLIST:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_OpenPlaylistRequester);
			break;

		case MEN_MESSAGES:
			set(mygui->consolewindow, MUIA_Window_Open, TRUE);
			break;

		case MEN_DIRECTORY:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_OpenDirectoryRequester);
			break;

		case MEN_DVD:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_Open, MV_MPlayerGroup_Open_DVD, NULL);
			break;

		case MEN_DVD_DIR:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_OpenDVDDirectoryRequester);
			break;

		case MEN_DVDNAV:
			DoMethod(mygui->menustrip, MUIM_GetUData, MEN_DVDNAV, MUIA_Menuitem_Checked, &gui_use_dvdnav);
			break;

		case MEN_STREAM:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_OpenURLRequester);
			break;

		case MEN_ABOUT:
			if(mygui->aboutwindow)
				set(mygui->aboutwindow, MUIA_Window_Open, TRUE);
			else
				MUI_Request(app, mygui->mainwindow, 0, "About MPlayer", "Ok", credits);
			break;

		case MEN_HIDE:
			if(mygui->embedded)
			{
				set(app, MUIA_Application_Iconified, TRUE);
			}
			else // just hide windows, don't iconify
			{
				DoMethod(mygui->maingroup, MM_MPlayerGroup_Show, MV_MPlayerGroup_Show_Toggle);
				set(mygui->playlistwindow, MUIA_Window_Open, FALSE);
				//...
			}
			break;

		/* Play */
		case MEN_PLAY:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_Play);
			break;

		case MEN_PAUSE:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_Pause);
			break;

		case MEN_STOP:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_Stop);
			break;

		case MEN_FRAMESTEP:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_Pause);
			break;

		case MEN_SEEKB10S:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_Seek, MV_MPlayerGroup_SeekFromRewind, 10);
			break;

		case MEN_SEEKF10S:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_Seek, MV_MPlayerGroup_SeekFromForward, 10);
			break;

		case MEN_SEEKB1M:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_Seek, MV_MPlayerGroup_SeekFromRewind, 60);
			break;

		case MEN_SEEKF1M:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_Seek, MV_MPlayerGroup_SeekFromForward, 60);
			break;

		case MEN_SEEKB10M:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_Seek, MV_MPlayerGroup_SeekFromRewind, 600);
			break;

		case MEN_SEEKF10M:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_Seek, MV_MPlayerGroup_SeekFromForward, 600);
			break;

		case MEN_INCSPEED:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_IncreaseSpeed, MV_MPlayerGroup_IncreaseSpeed);
			break;

		case MEN_DECSPEED:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_IncreaseSpeed, MV_MPlayerGroup_DecreaseSpeed);
			break;

		case MEN_PREVIOUS:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_Prev);
			break;

		case MEN_NEXT:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_Next);
			break;

		case MEN_REPEAT_OFF:
		case MEN_REPEAT_SINGLE:
		case MEN_REPEAT_PLAYLIST:
		case MEN_REPEAT_QUIT:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_Loop, (IPTR) msg->userdata);
			break;

		case MEN_RECORD:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_Record);
			break;

		/* Video */
		case MEN_FULLSCREEN:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_ToggleFullScreen);
			break;

		case MEN_SCREENSHOT:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_Screenshot);
			break;

		case MEN_DIMENSIONS_ORIGINAL:
		case MEN_DIMENSIONS_HALF:
		case MEN_DIMENSIONS_DOUBLE:
		case MEN_DIMENSIONS_FREE:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_Dimensions, (IPTR) msg->userdata);
			break;

		case MEN_ASPECT_AUTO:
		case MEN_ASPECT_4_3:
		case MEN_ASPECT_5_4:
		case MEN_ASPECT_16_9:
		case MEN_ASPECT_16_10:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_Aspect, (IPTR) msg->userdata);
			break;

		case MEN_DEINTERLACER_OFF:
		case MEN_DEINTERLACER_L5:
		case MEN_DEINTERLACER_YADIF:
		case MEN_DEINTERLACER_LB:
		case MEN_DEINTERLACER_YADIF1:
		case MEN_DEINTERLACER_KERNDEINT:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_Deinterlacer, (IPTR) msg->userdata);
			break;

		case MEN_DENOISE_OFF:
		case MEN_DENOISE_NORMAL:
		case MEN_DENOISE_SOFT:
		case MEN_DEBLOCK:
		case MEN_DERING:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_VideoFilter, (IPTR) msg->userdata);
			break;

		case MEN_ROTATE_OFF:
		case MEN_ROTATE_1:
		case MEN_ROTATE_2:
		case MEN_ROTATE_3:
		case MEN_ROTATE_4:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_Rotation, (IPTR) msg->userdata);
			break;

		case MEN_FLIP:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_Flip, (IPTR) msg->userdata);
			break;

		case MEN_MIRROR:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_Mirror, (IPTR) msg->userdata);
			break;

		case MEN_CROP:
			set(mygui->cropwindow, MUIA_Window_Open, TRUE);
			break;

		case MEN_SCALE:
			set(mygui->scalewindow, MUIA_Window_Open, TRUE);
			break;

		/* Audio */
		case MEN_MUTE:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_Mute);
			break;

		case MEN_INCVOLUME:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_IncreaseVolume);
			break;

		case MEN_DECVOLUME:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_DecreaseVolume);
			break;

		case MEN_AUDIOINCDELAY:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_IncreaseAVDelay);
			break;

		case MEN_AUDIODECDELAY:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_DecreaseAVDelay);
			break;

		case MEN_EXTRASTEREO:
		case MEN_VOLNORM:
		case MEN_KARAOKE:
		case MEN_SCALETEMPO:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_AudioFilter, (IPTR) msg->userdata);
			break;

		case MEN_AUDIOGAIN:
			set(mygui->audiogainwindow, MUIA_Window_Open, TRUE);
			break;

		case MEN_EQUALIZER:
			set(mygui->equalizerwindow, MUIA_Window_Open, TRUE);
			break;

		/* Subtitles */
		case MEN_CYCLESUBTITLE:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_CycleSubtitles);
			break;

		case MEN_LOADSUBTITLE:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_OpenSubtitleRequester);
			break;

		case MEN_UNLOADSUBTITLE:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_UnloadSubtitles, MV_MPlayerGroup_UnloadSubtitles_All);
			break;

		case MEN_ENABLEASS:
			DoMethod(mygui->menustrip, MUIM_GetUData, MEN_ENABLEASS, MUIA_Menuitem_Checked, &ass_enabled);
			break;

		case MEN_SUBINCDELAY:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_IncreaseSubtitleDelay);
			break;

		case MEN_SUBDECDELAY:
			DoMethod(mygui->maingroup, MM_MPlayerGroup_DecreaseSubtitleDelay);
			break;

		/* Options */
		case MEN_PROPERTIES:
			DoMethod(obj, MM_MPlayerGroup_ShowProperties);
			break;

		case MEN_PREFERENCES:
			DoMethod(obj, MM_MPlayerGroup_ShowPreferences);
			break;

		case MEN_PLAYLISTWIN:
			DoMethod(obj, MM_MPlayerGroup_ShowPlaylist);
			break;

		case MEN_TOGGLEGUI:
			DoMethod(mygui->menustrip, MUIM_GetUData, MEN_TOGGLEGUI, MUIA_Menuitem_Checked, &gui_show_gui);
			DoMethod(mygui->maingroup, MM_MPlayerGroup_ShowPanels, gui_show_gui);
			break;

		case MEN_SHOWSTATUS:
			DoMethod(mygui->menustrip, MUIM_GetUData, MEN_SHOWSTATUS, MUIA_Menuitem_Checked, &gui_show_status);
			DoMethod(mygui->maingroup, MM_MPlayerGroup_ShowStatus, gui_show_status);
			break;

		case MEN_SHOWTOOLBAR:
			DoMethod(mygui->menustrip, MUIM_GetUData, MEN_SHOWTOOLBAR, MUIA_Menuitem_Checked, &gui_show_toolbar);
			DoMethod(mygui->maingroup, MM_MPlayerGroup_ShowToolbar, gui_show_toolbar);
			break;

		case MEN_SHOWCONTROL:
			DoMethod(mygui->menustrip, MUIM_GetUData, MEN_SHOWCONTROL, MUIA_Menuitem_Checked, &gui_show_control);
			DoMethod(mygui->maingroup, MM_MPlayerGroup_ShowControl, gui_show_control);
			break;

		case MEN_SAVESETTINGS:
			free(dvd_device);
  	 		dvd_device = strdup(cfg_dvd_device); // Hack to prevent from saving dvddir if it was set
			cfg_write();
			break;

		case MEN_MUI:
			DoMethod(app, MUIM_Application_OpenConfigWindow, 0, NULL);
			break;
	}

	/* variable entries subs & audio menus */
	if((IPTR)msg->userdata >= MEN_SUBTITLEBASE && (IPTR)msg->userdata < MEN_SUBTITLEBASE + 32)
	{
		ULONG checked;
		DoMethod(mygui->menustrip, MUIM_GetUData, msg->userdata, MUIA_Menuitem_Checked, &checked);

		if(checked)
		{
			DoMethod(mygui->maingroup, MM_MPlayerGroup_SelectSubtitle, (IPTR)msg->userdata - MEN_SUBTITLEBASE);
		}
	}

	if((IPTR)msg->userdata >= MEN_AUDIOTRACKBASE && (IPTR)msg->userdata < MEN_AUDIOTRACKBASE + 32)
	{
		ULONG checked;
		DoMethod(mygui->menustrip, MUIM_GetUData, msg->userdata, MUIA_Menuitem_Checked, &checked);

		if(checked)
		{
			DoMethod(mygui->maingroup, MM_MPlayerGroup_SelectAudio, (IPTR)msg->userdata - MEN_AUDIOTRACKBASE);
		}
	}

	return 0;
}

DEFMMETHOD(HandleEvent)
{
	IPTR rc = 0;
	struct MPlayerGroupData *data = INST_DATA(cl, obj);

	if (msg->imsg)
	{
		ULONG Class;
		UWORD Code;
		UWORD Qualifier;
		int MouseX, MouseY;

		Class     = msg->imsg->Class;
		Code      = msg->imsg->Code;
		Qualifier = msg->imsg->Qualifier;
		MouseX    = msg->imsg->MouseX;
		MouseY    = msg->imsg->MouseY;
		
		switch( Class )
		{
			case IDCMP_ACTIVEWINDOW:
				if(mygui->embedded) Cgx_HandleBorder(mygui->videowindow, FALSE);
				break;

			case IDCMP_INACTIVEWINDOW:
				if(mygui->embedded) Cgx_HandleBorder(mygui->videowindow, FALSE);
				break;

			case IDCMP_MOUSEBUTTONS:

				if(!mygui->embedded || !getv(data->AR_Video, MUIA_ShowMe)) break;

				/* show mouse again */
				if (mygui->fullscreen && mouse_hidden)
				{
					Cgx_ShowMouse(mygui->screen, mygui->videowindow, TRUE);
				}
				
				/* ignores mouse button events if not in video area */
				if(!_isinobject(data->AR_Video, MouseX, MouseY)) break;

				//Cgx_HandleBorder();

				switch(Code)
				{
					case SELECTDOWN:
						mplayer_put_key(MOUSE_BTN0 | MP_KEY_DOWN);
						break;

					case SELECTUP:
						mplayer_put_key(MOUSE_BTN0 );
						break;

					/* Show on/off GUI panels */
					case MIDDLEDOWN:
						gui_show_gui ^= TRUE;
						DoMethod(obj, MM_MPlayerGroup_ShowPanels, gui_show_gui);

						break;
					default:
						;
				}

				break;

			case IDCMP_MOUSEMOVE:
				{
					char cmd_str[40];

					if(!mygui->embedded || !getv(data->AR_Video, MUIA_ShowMe)) break;

					Cgx_HandleBorder(mygui->videowindow, TRUE);

					/* Show mouse again */
					if (mygui->fullscreen && mouse_hidden)
					{
						Cgx_ShowMouse(mygui->screen, mygui->videowindow, TRUE);
					}

					/* Only handle mouse move events in video area */
					if(!_isinobject(data->AR_Video, MouseX, MouseY)) break;

					sprintf(cmd_str,"set_mouse_pos %i %i",
					(int) (MouseX - mygui->video_left - mygui->x_offset),
					(int) (MouseY - mygui->video_top - mygui->y_offset));

                    mp_input_queue_cmd(mp_input_parse_cmd(cmd_str));
				}

				break;

			case IDCMP_RAWKEY:
				 switch ( Code )
				 {
					 case  RAWKEY_ESCAPE:    mplayer_put_key(KEY_ESC); break;
					 case  RAWKEY_PAGEDOWN:  mplayer_put_key(KEY_PGDWN); break;
					 case  RAWKEY_PAGEUP:    mplayer_put_key(KEY_PGUP); break;

					 case  NM_WHEEL_UP:      if(_isinobject(data->SL_Seek, MouseX, MouseY) ||
												_isinobject(data->SL_Volume, MouseX, MouseY) )
											 {
												 break;
											 }

					 case  RAWKEY_RIGHT:     mplayer_put_key(KEY_RIGHT); break;

					 case  NM_WHEEL_DOWN:    if(_isinobject(data->SL_Seek, MouseX, MouseY) ||
												_isinobject(data->SL_Volume, MouseX, MouseY) )
											 {
												break;
											 }

					 case  RAWKEY_LEFT:      mplayer_put_key(KEY_LEFT); break;
					 case  RAWKEY_UP:        mplayer_put_key(KEY_UP); break;
					 case  RAWKEY_DOWN:      mplayer_put_key(KEY_DOWN); break;
					 case  RAWKEY_F1:        mplayer_put_key(KEY_F+1); break;
					 case  RAWKEY_F2:        mplayer_put_key(KEY_F+2); break;
					 case  RAWKEY_F3:        mplayer_put_key(KEY_F+3); break;
					 case  RAWKEY_F4:        mplayer_put_key(KEY_F+4); break;
					 case  RAWKEY_F5:        mplayer_put_key(KEY_F+5); break;
					 case  RAWKEY_F6:        mplayer_put_key(KEY_F+6); break;
					 case  RAWKEY_F7:        mplayer_put_key(KEY_F+7); break;
					 case  RAWKEY_F8:        mplayer_put_key(KEY_F+8); break;
					 case  RAWKEY_F9:        mplayer_put_key(KEY_F+9); break;
					 case  RAWKEY_F10:       mplayer_put_key(KEY_F+10); break;
					 case  RAWKEY_F11:       mplayer_put_key(KEY_F+11); break;
					 case  RAWKEY_F12:       mplayer_put_key(KEY_F+12); break;
					 case  RAWKEY_RETURN:    mplayer_put_key(KEY_ENTER); break;
					 case  RAWKEY_TAB:       mplayer_put_key(KEY_TAB); break;
					 case  RAWKEY_CONTROL:   mplayer_put_key(KEY_CTRL); break;
					 case  RAWKEY_BACKSPACE: mplayer_put_key(KEY_BACKSPACE); break;
					 case  RAWKEY_DELETE:    mplayer_put_key(KEY_DELETE); break;
					 case  RAWKEY_INSERT:    mplayer_put_key(KEY_INSERT); break;
					 case  RAWKEY_HOME:      mplayer_put_key(KEY_HOME); break;
					 case  RAWKEY_END:       mplayer_put_key(KEY_END); break;
					 case  RAWKEY_KP_ENTER:  mplayer_put_key(KEY_KPENTER); break;
					 case  RAWKEY_KP_1:      mplayer_put_key(KEY_KP1); break;
					 case  RAWKEY_KP_2:      mplayer_put_key(KEY_KP2); break;
					 case  RAWKEY_KP_3:      mplayer_put_key(KEY_KP3); break;
					 case  RAWKEY_KP_4:      mplayer_put_key(KEY_KP4); break;
					 case  RAWKEY_KP_5:      mplayer_put_key(KEY_KP5); break;
					 case  RAWKEY_KP_6:      mplayer_put_key(KEY_KP6); break;
					 case  RAWKEY_KP_7:      mplayer_put_key(KEY_KP7); break;
					 case  RAWKEY_KP_8:      mplayer_put_key(KEY_KP8); break;
					 case  RAWKEY_KP_9:      mplayer_put_key(KEY_KP9); break;
					 case  RAWKEY_KP_0:      mplayer_put_key(KEY_KP0); break;

					 case  RAWKEY_LAMIGA:
					 case  RAWKEY_RAMIGA:    break;

					 default:
					 {
						struct InputEvent ie;
						TEXT c;

						ie.ie_Class        = IECLASS_RAWKEY;
						ie.ie_SubClass     = 0;
						ie.ie_Code         = Code;
						ie.ie_Qualifier    = Qualifier;
						ie.ie_EventAddress = NULL;

						if (MapRawKey(&ie, &c, 1, NULL) == 1)
						{
							mplayer_put_key(c);
						}
					 }
			    }
			    break;

			default:
				;
	    }
	}

	return rc;
}

#define STATUS_PERSISTANCE_TIME 2000

static LONG mplayer_update(struct IClass * cl, APTR obj, ULONG mode, APTR arg)
{
	struct MPlayerGroupData *data = INST_DATA(cl, obj);

	// Position
	if((mode == MV_MPlayerGroup_Update_All || mode == MV_MPlayerGroup_Update_Position) && gui_show_gui && gui_show_control)
	{
		static int prevTimeSec = -1;
		static int prevLength = -1;

		if(guiInfo.RunningTime && /*guiInfo.StreamType != STREAMTYPE_STREAM &&*/ prevLength != guiInfo.RunningTime)
		{
			nnset(data->SL_Seek, MUIA_Slider_Max, guiInfo.RunningTime);
			nnset(data->SL_Seek, MUIA_Numeric_Max, guiInfo.RunningTime);

			prevLength = guiInfo.RunningTime;
		}

		if(prevTimeSec != guiInfo.ElapsedTime)
		{
			if(guiInfo.Playing /*&& guiInfo.StreamType != STREAMTYPE_STREAM*/)
			{
				if(data->update && guiInfo.ElapsedTime) /* filter 0, because it seems it's reset to 0 during a short time */
				{
					nnset(data->SL_Seek, MUIA_Slider_Level, guiInfo.RunningTime ? guiInfo.ElapsedTime : 0);
				}
			}
			else
			{
				nnset(data->SL_Seek, MUIA_Slider_Level, 0);
			}
		}
		prevTimeSec = guiInfo.ElapsedTime;
	}

	// Volume
	if((mode == MV_MPlayerGroup_Update_All || mode == MV_MPlayerGroup_Update_Volume) && gui_show_gui && gui_show_control)
	{
		if(guiInfo.Playing)
		{
			static float prevVolume = -1;

			if((int) prevVolume != (int) guiInfo.Volume)
			{
				nnset(data->SL_Volume, MUIA_Slider_Level, (int) guiInfo.Volume);
				if(((int) guiInfo.Volume) == 0)
				{
					set(data->BT_Mute, MUIA_Dtpic_Name, "PROGDIR:images/volume-zero.png");
				}
				else if(guiInfo.Volume > 0)
				{
					set(data->BT_Mute, MUIA_Dtpic_Name, "PROGDIR:images/volume-max.png");
				}
			}
			prevVolume = guiInfo.Volume;
		}
	}

	// Time status
	if((mode == MV_MPlayerGroup_Update_All || mode == MV_MPlayerGroup_Update_Time) && gui_show_gui && gui_show_status)
	{
		static int prevTimeSec = -1;

		if(prevTimeSec != guiInfo.ElapsedTime)
		{
			TEXT formattedtime[64];
			int	h=0, m=0, s=0, ht=0, mt=0, st=0;

			//if(guiInfo.StreamType != STREAMTYPE_STREAM)
			{
				int current = guiInfo.ElapsedTime;
				int length = guiInfo.RunningTime;

				h = current / 3600;
				m = current / 60 % 60;
				s = current % 60;

				ht = length / 3600;
				mt = length / 60 % 60;
				st = length % 60;
			}

			snprintf(formattedtime, sizeof(formattedtime), "%02d:%02d:%02d / %02d:%02d:%02d ", h, m, s, ht, mt, st);

			set(data->ST_Time, MUIA_Text_Contents, formattedtime);
		}
		prevTimeSec = guiInfo.ElapsedTime;
	}

	// String status
	if((mode == MV_MPlayerGroup_Update_All || mode == MV_MPlayerGroup_Update_Status) && gui_show_gui && gui_show_status)
	{
		static int prevPlaying = -1;
		static int prevTrackCount = -1, prevCurrent = -1;
		static char prevFilename[255] = "";
		ULONG refresh = FALSE;
		ULONG secs, micros, currentTime;
		CurrentTime(&secs, &micros);
        currentTime = secs*1000 + micros/1000;

		/* only refresh if needed */
		refresh = prevPlaying != guiInfo.Playing ||
				  prevTrackCount != mygui->playlist->trackcount ||
				  prevCurrent != mygui->playlist->current;

		if(!refresh && guiInfo.Filename)
			refresh = strcmp(guiInfo.Filename, prevFilename) != 0;
				  
		prevPlaying = guiInfo.Playing;
		prevTrackCount = mygui->playlist->trackcount;
		prevCurrent = mygui->playlist->current;

		if(guiInfo.Filename)
			stccpy(prevFilename, guiInfo.Filename, sizeof(prevFilename));

		if(currentTime > (mygui->last_status_update + STATUS_PERSISTANCE_TIME)) // clear status after some time
		{
			arg = NULL; // std play/pause/... status

			if(mygui->status_dirty)
			{
				mygui->status_dirty = FALSE;
				refresh = TRUE;
			}
		}
		else if(arg) // special status string sent from mplayer
		{
			STRPTR status = (STRPTR) arg;
			set(data->ST_Status, MUIA_Text_Contents, status);
			mygui->status_dirty = TRUE;
			return 0;
		}
		else
		{
			return 0;
		}

		if(!arg)
		{
			TEXT status[256];
			TEXT pls_status[32];
			STRPTR plsptr = NULL;

			if(refresh)
			{
				if(mygui->playlist->trackcount)
				{
					snprintf(pls_status, sizeof(pls_status), "[%d/%d] ", mygui->playlist->current+1, mygui->playlist->trackcount);
					plsptr = pls_status;
				}
				else
				{
					plsptr = "";
				}

				switch(guiInfo.Playing)
				{
					default:
					case 0:
						snprintf(status, sizeof(status), "%sIdle...", plsptr);
						break;

					case 1:
					{
						STRPTR title = get_title();
						snprintf(status, sizeof(status), "%sPlaying \033b%s\033n...", plsptr, (title) ? (char *) title : (char *) "");
						break;
					}

					case 2:
						snprintf(status, sizeof(status), "%sPaused...", plsptr);
						break;
				}
				set(data->ST_Status, MUIA_Text_Contents, status);
			}
		}	 
	}

	// Lists/Menus
	if(guiInfo.StreamType == STREAMTYPE_DVD || guiInfo.StreamType == STREAMTYPE_DVDNAV || // DVD/DVDNav
	   (/*guiInfo.mpcontext && guiInfo.mpcontext->demuxer && demuxer_chapter_count(guiInfo.mpcontext->demuxer) > 1*/guiInfo.Chapters > 1)) // But also for other stream types with chapters
	{
		if(mode == MV_MPlayerGroup_Update_Lists)
		{
			DoMethod(obj, MM_MPlayerGroup_BuildChapterList);
			DoMethod(obj, MM_MPlayerGroup_BuildTitleList);
			DoMethod(obj, MM_MPlayerGroup_BuildAngleList);
		}
		else if(mode == MV_MPlayerGroup_Update_All && gui_show_gui && gui_show_toolbar)
		{
			if(data->update)
			{
				DoMethod(obj, MM_MPlayerGroup_SetChapter, FALSE);
				DoMethod(obj, MM_MPlayerGroup_SetTitle, FALSE);
				DoMethod(obj, MM_MPlayerGroup_SetAngle, FALSE);
			}
		}
	}

	return 0;
}


DEFTMETHOD(MPlayerGroup_UpdateAll)
{
	return mplayer_update(cl, obj, MV_MPlayerGroup_Update_All, NULL);
}

DEFSMETHOD(MPlayerGroup_Update)
{
	return mplayer_update(cl, obj, msg->mode, msg->data);
}

DEFSMETHOD(MPlayerGroup_SetValues)
{
	struct MPlayerGroupData *data = INST_DATA(cl, obj);
	// update playlist for meta data ?

	// update status with meta data.
	mygui->status_dirty = TRUE;
	DoMethod(obj, MM_MPlayerGroup_Update, MV_MPlayerGroup_Update_Status, NULL);

	return 0;
}

struct WindowState
{
	APTR window;
	ULONG opened;
};

DEFSMETHOD(MPlayerGroup_SetWindow)
{
	struct WindowState windowstates[] =
	{
		{ mygui->mainwindow, getv(mygui->mainwindow, MUIA_Window_Open) },
		{ mygui->playlistwindow, getv(mygui->playlistwindow, MUIA_Window_Open) },
		{ mygui->urlwindow, getv(mygui->urlwindow, MUIA_Window_Open) },
		{ mygui->dvddirwindow, getv(mygui->dvddirwindow, MUIA_Window_Open) },
		{ mygui->aboutwindow, mygui->aboutwindow ? getv(mygui->aboutwindow, MUIA_Window_Open) : FALSE },
		{ mygui->propertieswindow, getv(mygui->propertieswindow, MUIA_Window_Open) },
		{ mygui->consolewindow, getv(mygui->consolewindow, MUIA_Window_Open) },
		{ mygui->prefswindow, getv(mygui->prefswindow, MUIA_Window_Open) },
		{ NULL, FALSE }
	};

	struct Screen * currentScreen = (struct Screen *) getv(mygui->mainwindow, MUIA_Window_Screen);
	struct Screen * targetScreen = NULL;

	if(msg->window)
	{
		targetScreen = ((struct Window *) msg->window)->WScreen;
	}
	else
	{
		targetScreen = mygui->wbscreen;
	}

	if(/*targetScreen && */currentScreen != targetScreen)
	{
		int i;

		for(i=0; windowstates[i].window; i++)
		{
			if(windowstates[i].opened) set(windowstates[i].window, MUIA_Window_Open, FALSE);
		}

		for(i=0; windowstates[i].window; i++)
		{
			set(windowstates[i].window, MUIA_Window_Screen, targetScreen);
		}

		for(i=0; windowstates[i].window; i++)
		{
			set(windowstates[i].window, MUIA_Window_Open, windowstates[i].opened);
		}
	}

	mygui->gui_ready = TRUE;

	return 0;
}

DEFSMETHOD(MPlayerGroup_ShowToolbar)
{
	struct MPlayerGroupData *data = INST_DATA(cl, obj);

	set(data->GR_Toolbar, MUIA_ShowMe, gui_show_gui ? msg->show : FALSE);
	return 0;
}

DEFSMETHOD(MPlayerGroup_ShowStatus)
{
	struct MPlayerGroupData *data = INST_DATA(cl, obj);

	set(data->GR_StatusBar, MUIA_ShowMe, gui_show_gui ? msg->show : FALSE);
	return 0;
}

DEFSMETHOD(MPlayerGroup_ShowControl)
{
	struct MPlayerGroupData *data = INST_DATA(cl, obj);
	ULONG show = msg->show;

	// Make sure to show something at all
	if(!getv(data->AR_Video, MUIA_ShowMe) && show == FALSE)
	{
		gui_show_control = TRUE;
		gui_show_gui = TRUE;
		DoMethod(mygui->menustrip, MUIM_SetUData, MEN_SHOWCONTROL, MUIA_Menuitem_Checked, gui_show_control);
		DoMethod(mygui->menustrip, MUIM_SetUData, MEN_TOGGLEGUI, MUIA_Menuitem_Checked, gui_show_gui);
		return 0;
	}

	set(data->GR_Control, MUIA_ShowMe, gui_show_gui ? show : FALSE);

	return 0;
}

DEFSMETHOD(MPlayerGroup_ShowPanels)
{
	struct MPlayerGroupData *data = INST_DATA(cl, obj);

	if(!getv(data->AR_Video, MUIA_ShowMe) && msg->show == FALSE)
	{
		return 0;
	}

	DoMethod(obj, MM_MPlayerGroup_ShowStatus,  msg->show ? gui_show_status : FALSE);
	DoMethod(obj, MM_MPlayerGroup_ShowControl, msg->show ? gui_show_control : FALSE);
	DoMethod(obj, MM_MPlayerGroup_ShowToolbar, msg->show ? gui_show_toolbar : FALSE);

	DoMethod(mygui->menustrip, MUIM_SetUData, MEN_SHOWSTATUS, MUIA_Menuitem_Checked, gui_show_status);
	DoMethod(mygui->menustrip, MUIM_SetUData, MEN_SHOWTOOLBAR, MUIA_Menuitem_Checked, gui_show_toolbar);
	DoMethod(mygui->menustrip, MUIM_SetUData, MEN_SHOWCONTROL, MUIA_Menuitem_Checked, gui_show_control);

	DoMethod(mygui->menustrip, MUIM_SetUData, MEN_TOGGLEGUI, MUIA_Menuitem_Checked, gui_show_gui);

	return 0;
}

DEFSMETHOD(MPlayerGroup_OpenFullScreen)
{
	int i;
	struct MPlayerGroupData *data = INST_DATA(cl, obj);

	struct WindowState windowstates[] =
	{
		{ mygui->playlistwindow, getv(mygui->playlistwindow, MUIA_Window_Open) },
		{ mygui->urlwindow, getv(mygui->urlwindow, MUIA_Window_Open) },
		{ mygui->dvddirwindow, getv(mygui->dvddirwindow, MUIA_Window_Open) },
		{ mygui->aboutwindow, mygui->aboutwindow ? getv(mygui->aboutwindow, MUIA_Window_Open) : FALSE },
		{ mygui->propertieswindow, getv(mygui->propertieswindow, MUIA_Window_Open) },
		{ mygui->consolewindow, getv(mygui->consolewindow, MUIA_Window_Open) },
		{ mygui->prefswindow, getv(mygui->prefswindow, MUIA_Window_Open) },
		{ NULL, FALSE }
	};

	if((msg->enable && mygui->fullscreen) || (!msg->enable && !mygui->fullscreen))
	{
		mygui->gui_ready = TRUE;
		return 0;
	}

	set(mygui->mainwindow, MUIA_Window_Open, FALSE);

	for(i=0; windowstates[i].window; i++)
	{
		if(windowstates[i].opened) set(windowstates[i].window, MUIA_Window_Open, FALSE);
	}

	if(!msg->enable)
	{
		for(i=0; windowstates[i].window; i++)
		{
			set(windowstates[i].window, MUIA_Window_Screen, mygui->wbscreen);
		}

		SetAttrs(mygui->mainwindow,
				 MUIA_Window_Title,       windowtitle,
				 MUIA_Window_Borderless,  FALSE,
				 MUIA_Window_DepthGadget, TRUE,
				 MUIA_Window_DragBar,     TRUE,
				 MUIA_Window_CloseGadget, TRUE,
				 MUIA_Window_SizeGadget,  TRUE,
				 MUIA_Window_Backdrop,    FALSE,
				 MUIA_Window_ID,          MAKE_ID('M','A','I','N'),
				 MUIA_Window_Screen,      mygui->wbscreen,
				 TAG_DONE);

		SetAttrs(mygui->maingroup,
				 MUIA_Frame, MUIV_Frame_Window,
				 MUIA_InnerLeft, innerleft,
				 MUIA_InnerRight, innerright,
				 MUIA_InnerTop, innertop,
				 MUIA_InnerBottom, innerbottom,
				 TAG_DONE);

		if(mygui->screen)
		{
			while(!CloseScreen(mygui->screen)) usleep(10000);
			mygui->screen = NULL;
		}

		mygui->fullscreen = FALSE;

		gui_show_gui = data->window_gui_show_gui;
		DoMethod(obj, MM_MPlayerGroup_ShowPanels, gui_show_gui);
	}
	else
	{
		mygui->screen = OpenScreenTags ( NULL,
				SA_LikeWorkbench, TRUE,
				SA_Title,         "MPlayer Screen",
				SA_Depth,         32,
				SA_Type,          PUBLICSCREEN,
				SA_PubName,       MorphOS_GetScreenTitle(),
				SA_ShowTitle,     FALSE,
				SA_Quiet,         TRUE,
		        TAG_DONE);

		if(mygui->screen)
		{
			for(i=0; windowstates[i].window; i++)
			{
				set(windowstates[i].window, MUIA_Window_Screen, mygui->screen);
			}

			PubScreenStatus(mygui->screen, 0);
			set(mygui->mainwindow, MUIA_Window_Screen, mygui->screen);

			SetAttrs(mygui->mainwindow,
					 MUIA_Window_Title,       NULL,
					 MUIA_Window_Borderless,  TRUE,
					 MUIA_Window_DepthGadget, FALSE,
					 MUIA_Window_DragBar,     FALSE,
					 MUIA_Window_CloseGadget, FALSE,
					 MUIA_Window_SizeGadget,  FALSE,
					 MUIA_Window_Backdrop,    TRUE,
					 MUIA_Window_ID,          0,
					 TAG_DONE);

			SetAttrs(mygui->mainwindow,
					 MUIA_Frame, MUIV_Frame_None,
					 MUIA_Window_TopEdge, 0,
					 MUIA_Window_LeftEdge, 0,
					 MUIA_Window_Width, ((struct Screen *)mygui->screen)->Width,
					 MUIA_Window_Height, ((struct Screen *)mygui->screen)->Height,
					 TAG_DONE);

			SetAttrs(mygui->maingroup,
					 InnerSpacing(0, 0),
                     MUIA_Frame, MUIV_Frame_None,
					 TAG_DONE);

			mygui->fullscreen = TRUE;

			data->window_gui_show_gui = gui_show_gui;
			gui_show_gui = FALSE;
			DoMethod(obj, MM_MPlayerGroup_ShowPanels, gui_show_gui);
		}
	}

	for(i=0; windowstates[i].window; i++)
	{
		set(windowstates[i].window, MUIA_Window_Open, windowstates[i].opened);
	}

	set(mygui->mainwindow, MUIA_Window_Open, TRUE);

	if(!getv(mygui->mainwindow, MUIA_Window_Open))
	{
		mygui->fatalerror = TRUE;
		return 0;
	}

	return 0;
}

BEGINMTABLE2(mplayergroupclass)
DECNEW(MPlayerGroup)
DECDISP(MPlayerGroup)
DECMMETHOD(Setup)
DECMMETHOD(Cleanup)
DECMMETHOD(Show)
DECMMETHOD(HandleEvent)
DECGET(MPlayerGroup)
DECSET(MPlayerGroup)
DECSMETHOD(MPlayerGroup_Show)
DECSMETHOD(MPlayerGroup_ShowToolbar)
DECSMETHOD(MPlayerGroup_ShowStatus)
DECSMETHOD(MPlayerGroup_ShowControl)
DECSMETHOD(MPlayerGroup_ShowPanels)
DECTMETHOD(MPlayerGroup_ShowPlaylist)
DECTMETHOD(MPlayerGroup_ShowProperties)
DECTMETHOD(MPlayerGroup_ShowPreferences)
DECSMETHOD(MPlayerGroup_ShowChapterBrowser)
DECSMETHOD(MPlayerGroup_ShowDVDBrowser)
DECSMETHOD(MPlayerGroup_OpenFullScreen)
DECTMETHOD(MPlayerGroup_Play)
DECTMETHOD(MPlayerGroup_Pause)
DECTMETHOD(MPlayerGroup_Stop)
DECTMETHOD(MPlayerGroup_Prev)
DECTMETHOD(MPlayerGroup_Next)
DECSMETHOD(MPlayerGroup_IncreaseSpeed)
DECSMETHOD(MPlayerGroup_MultiplySpeed)
DECSMETHOD(MPlayerGroup_Loop)
DECTMETHOD(MPlayerGroup_Record)
DECSMETHOD(MPlayerGroup_Open)
DECTMETHOD(MPlayerGroup_OpenFileRequester)
DECTMETHOD(MPlayerGroup_OpenPlaylistRequester)
DECTMETHOD(MPlayerGroup_OpenDirectoryRequester)
DECTMETHOD(MPlayerGroup_OpenDVDDirectoryRequester)
DECTMETHOD(MPlayerGroup_OpenURLRequester)
DECSMETHOD(MPlayer_OpenStream)
DECTMETHOD(MPlayerGroup_Mute)
DECTMETHOD(MPlayerGroup_ToggleFullScreen)
DECTMETHOD(MPlayerGroup_Screenshot)
DECSMETHOD(MPlayerGroup_Dimensions)
DECSMETHOD(MPlayerGroup_Aspect)
DECSMETHOD(MPlayerGroup_Deinterlacer)
DECSMETHOD(MPlayerGroup_VideoFilter)
DECSMETHOD(MPlayerGroup_Rotation)
DECSMETHOD(MPlayerGroup_Flip)
DECSMETHOD(MPlayerGroup_Mirror)
DECSMETHOD(MPlayerGroup_Crop)
DECSMETHOD(MPlayerGroup_Scale)
DECSMETHOD(MPlayerGroup_Seek)
DECTMETHOD(MPlayerGroup_SmartSeek)
DECTMETHOD(MPlayerGroup_Volume)
DECTMETHOD(MPlayerGroup_ShowDVDMenu)
DECTMETHOD(MPlayerGroup_BuildChapterList)
DECSMETHOD(MPlayerGroup_SetChapter)
DECTMETHOD(MPlayerGroup_BuildTitleList)
DECSMETHOD(MPlayerGroup_SetTitle)
DECTMETHOD(MPlayerGroup_BuildAngleList)
DECSMETHOD(MPlayerGroup_SetAngle)
DECTMETHOD(MPlayerGroup_CycleSubtitles)
DECTMETHOD(MPlayerGroup_OpenSubtitleRequester)
DECSMETHOD(MPlayerGroup_UnloadSubtitles)
DECSMETHOD(MPlayerGroup_SelectSubtitle)
DECTMETHOD(MPlayerGroup_RefreshSubtitles)
DECTMETHOD(MPlayerGroup_IncreaseSubtitleDelay)
DECTMETHOD(MPlayerGroup_DecreaseSubtitleDelay)
DECSMETHOD(MPlayerGroup_SelectAudio)
DECTMETHOD(MPlayerGroup_RefreshAudio)
DECSMETHOD(MPlayerGroup_AudioFilter)
DECSMETHOD(MPlayerGroup_AudioGain)
DECTMETHOD(MPlayerGroup_IncreaseVolume)
DECTMETHOD(MPlayerGroup_DecreaseVolume)
DECTMETHOD(MPlayerGroup_IncreaseAVDelay)
DECTMETHOD(MPlayerGroup_DecreaseAVDelay)
DECSMETHOD(MPlayerGroup_Update)
DECTMETHOD(MPlayerGroup_UpdateAll)
DECSMETHOD(MPlayerGroup_SetWindow)
DECSMETHOD(MPlayerGroup_SetValues)
DECSMETHOD(MPlayerGroup_HandleMenu)
DECSMETHOD(MPlayerGroup_Equalizer)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Group, mplayergroupclass, MPlayerGroupData)
