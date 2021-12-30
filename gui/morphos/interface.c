/*
 * MPlayer GUI for Win32
 * Copyright (C) 2003 Sascha Sommer <saschasommer@freenet.de>
 * Copyright (C) 2006 Erik Augustson <erik_27can@yahoo.com>
 * Copyright (C) 2006 Gianluigi Tiesi <sherpya@netfarm.it>
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <ctype.h>
#include <unistd.h>
#include <proto/dos.h>
#include <intuition/intuition.h>
#include <clib/debug_protos.h>
#include <clib/macros.h>
#include <sys/stat.h>
#ifdef CONFIG_LIBCDIO
#include <cdio/cdio.h>
#endif

#include "gui/util/mem.h"
#include "access_mpcontext.h"
#include "codec-cfg.h"
#include "command.h"
#include "help_mp.h"
#include "playtree.h"
#include "m_config.h"
#include "m_option.h"
#include "mixer.h"
#include "mp_msg.h"
#include "mp_core.h"
#include "mpcommon.h"
#include "mplayer.h"
#include "path.h"
#include "sub/sub.h"
#include "sub/font_load.h"
#include "input/input.h"
#include "libao2/audio_out.h"
#include "libmpcodecs/vd.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"
#include "libvo/video_out.h"
#include "libvo/cgx_common.h"
#include "stream/stream.h"
#ifdef CONFIG_DVDREAD
#include "stream/stream_dvd.h"
#include <dvdnav/dvdnav.h>
#include "stream/stream_dvdnav.h"
#endif
#include "libass/ass.h"
#include "osdep/timer.h"

#include "morphos_stuff.h"
#include "gui/interface.h"
#include "gui.h"
#include "thread.h"

#define D(x) ;

extern void dvd_read_name(char *name, char *serial, const char *device);

extern int quiet;

extern int abs_seek_pos;
extern float rel_seek_secs;
//extern int vcd_track;
extern af_cfg_t af_cfg;
extern mp_osd_obj_t* vo_osd_list;
extern char **sub_name;
extern struct Screen * My_Screen;

char cfg_dvd_device[MAX_PATH];

static char dvdname[MAX_PATH];
char windowtitle[256];

int guiWinID = 0;
char *skinName = NULL;
char *codecname = NULL;
int mplGotoTheNext = 1;
gui_t *mygui = NULL;
int gui_initialized = FALSE;
int gui_failed = 0;
const ao_functions_t *audio_out = NULL;
const vo_functions_t *video_out = NULL;
mixer_t *mixer = NULL;

static struct Thread * guithread = NULL;

/* catch events here in gui mode */
void muiEventHandling(void)
{
#if MPLAYER
	/* REXX PORT */
	gui_handle_events();
	/* END REXX PORT */
#endif
}

void capitalize(char *filename)
{
    unsigned int i;
    BOOL cap = TRUE;
    for (i=0; i < strlen(filename); i++)
    {
        if (cap)
        {
            cap = FALSE;
            filename[i] = toupper(filename[i]);
        }
        else if (filename[i] == ' ')
            cap = TRUE;
        else
            filename[i] = tolower(filename[i]);
    }
}

/* test for playlist files, no need to specify -playlist on the commandline.
 * add any conceivable playlist extensions here.
 * - Erik
 */
int parse_filename(char *file, play_tree_t *playtree, m_config_t *mconfig, int clear)
{
    if(clear)
	{
        mygui->playlist->clear_playlist(mygui->playlist);
	}

    if(strstr(file, ".m3u") || strstr(file, ".pls"))
    {
        playtree = parse_playlist_file(file);
		if(playtree)
		{
			guiPlaylistAdd(playtree, mconfig);
			return 1;
		}
	}
    return 0;
}

STRPTR get_title(void)
{
	STRPTR titleptr = NULL;

	if(guiInfo.mpcontext && guiInfo.Filename)
	{
		STRPTR title = property_expand_string(guiInfo.mpcontext, "${metadata/Title}");
		ULONG use_filepart = strstr(guiInfo.Filename, "://") == NULL;

		if(title && *title)
		{
			titleptr = title;
		}
		else if(mygui->icy_info)
		{
			titleptr = mygui->icy_info;
		}
		else if(use_filepart)
		{
			titleptr = FilePart(guiInfo.Filename);
		}
		else
		{
			titleptr = guiInfo.Filename;
		}
	}

	return titleptr;
}

void guiUpdateICY(char * info)
{
	char * ptr_start = strstr(info, "StreamTitle='");
	char * ptr_end = NULL;

	if(ptr_start)
	{
		ptr_start += strlen("StreamTitle='");
		ptr_end = strstr(ptr_start, "'");
	}

	free(mygui->icy_info);

	if(ptr_start && ptr_end)
	{
		mygui->icy_info = malloc(ptr_end - ptr_start + 1);
		if(mygui->icy_info)
		{
			stccpy(mygui->icy_info, ptr_start, ptr_end - ptr_start + 1);
		}
	}
	else
	{
		mygui->icy_info = strdup(info);
	}

	DoMethod(app, MUIM_Application_PushMethod, mygui->maingroup, 2, MM_MPlayerGroup_SetValues, NULL);
}

void mplayerLoadFont(void)
{
#ifdef CONFIG_FREETYPE
    load_font_ft(vo_image_width, vo_image_height, &vo_font, font_name, osd_font_scale_factor);
#else
    if (vo_font) {
        int i;

        free(vo_font->name);
        free(vo_font->fpath);

        for (i = 0; i < 16; i++) {
            if (vo_font->pic_a[i]) {
                free(vo_font->pic_a[i]->bmp);
                free(vo_font->pic_a[i]->pal);
            }
        }

        for (i = 0; i < 16; i++) {
            if (vo_font->pic_b[i]) {
                free(vo_font->pic_b[i]->bmp);
                free(vo_font->pic_b[i]->pal);
            }
        }

        free(vo_font);
        vo_font = NULL;
    }

    if (font_name) {
        vo_font = read_font_desc(font_name, font_factor, 0);

        if (!vo_font)
            gmp_msg(MSGT_GPLAYER, MSGL_ERR, MSGTR_CantLoadFont, font_name);
    } else {
        font_name = gstrdup(get_path("font/font.desc"));
        vo_font   = read_font_desc(font_name, font_factor, 0);

        if (!vo_font) {
            nfree(font_name);
            font_name = gstrdup(MPLAYER_DATADIR "/font/font.desc");
            vo_font   = read_font_desc(font_name, font_factor, 0);
        }
    }
#endif
}

/* really hardcore */
void guiClearSubtitle(void)
{
    if (vo_osd_list)
    {
        int len;
        mp_osd_obj_t *osd = vo_osd_list;
        while (osd)
        {
            if (osd->type == OSDTYPE_SUBTITLE) break;
            osd = osd->next;
        }
        if (osd && osd->flags & OSDFLAG_VISIBLE)
        {
            len = osd->stride * (osd->bbox.y2 - osd->bbox.y1);
            memset(osd->bitmap_buffer, 0, len);
            memset(osd->alpha_buffer, 0, len);
        }
    }
}

void mplayerLoadSubtitle(const char *name)
{
    if (guiInfo.Playing == 0)
        return;

    if (subdata)
    {
		int i = 0;
        mp_msg(MSGT_GPLAYER, MSGL_INFO, MSGTR_DeletingSubtitles);
        sub_free(subdata);
        subdata = NULL;
        vo_sub = NULL;

		// NEW still needed?
		/*
		if(sub_name)
		{
			while(sub_name[i])
			{
				free(sub_name[i]);
				i++;
			}
			free(sub_name);
			sub_name = NULL;
		}
		*/

        if (vo_osd_list)
        {
            int len;
            mp_osd_obj_t *osd = vo_osd_list;
            while (osd)
            {
                if (osd->type == OSDTYPE_SUBTITLE) break;
                osd = osd->next;
            }
            if (osd && osd->flags & OSDFLAG_VISIBLE)
            {
                len = osd->stride * (osd->bbox.y2 - osd->bbox.y1);
                memset(osd->bitmap_buffer, 0, len);
                memset(osd->alpha_buffer, 0, len);
            }
        }
    }

    if (name)
    {
        mp_msg(MSGT_GPLAYER, MSGL_INFO, MSGTR_LoadingSubtitles, name);
		subdata = sub_read_file(name, (guiInfo.sh_video ? guiInfo.sh_video->fps : 0));
		if (!subdata) mp_msg(MSGT_GPLAYER, MSGL_ERR, MSGTR_CantLoadSub, name);
        sub_name = (malloc(2 * sizeof(char*))); /* when mplayer will be restarted */
		if(sub_name)
		{
	        sub_name[0] = strdup(name);               /* sub_name[0] will be read */
	        sub_name[1] = NULL;
		}
    }
    update_set_of_subtitles();
}

void startplay(gui_t * gui)
{
	gui->playercontrol(evDropFile, 0);
}

void updatedisplay(gui_t * gui)
{
	DoMethod(app, MUIM_Application_PushMethod, mygui->maingroup, 3, MM_MPlayerGroup_Update, MV_MPlayerGroup_Update_All, NULL);
}

void update_windowdimensions(void)
{
	ULONG menuentry;
	switch(gui_window_dimensions)
	{
		default:
		case 0:
			menuentry = MEN_DIMENSIONS_FREE;
			break;
		case 1:
			menuentry = MEN_DIMENSIONS_ORIGINAL;
			break;
		case 2:
			menuentry = MEN_DIMENSIONS_HALF;
			break;
		case 3:
			menuentry = MEN_DIMENSIONS_DOUBLE;
			break;
	}

	DoMethod(app, MUIM_Application_PushMethod, mygui->maingroup, 2, MM_MPlayerGroup_Dimensions, menuentry);
}

void update_playlistwindow(ULONG rebuild)
{
	DoMethod(app, MUIM_Application_PushMethod, mygui->playlistgroup, 2, MM_PlaylistGroup_Refresh, rebuild);
}

void uiRelSeek(float sec)
{
    rel_seek_secs = sec;
    abs_seek_pos  = 0;
}

void uiAbsSeek(float percent)
{
    if (guiInfo.StreamType == STREAMTYPE_STREAM)
        return;

    rel_seek_secs = percent / 100.0;
    abs_seek_pos  = 3;
}

/* this function gets called by the gui to update mplayer */
void uiEventHandling(int event, float param)
{
    int iparam = (int)param;

	D(kprintf("uiEventHandling(%d, %f)\n", event, param));

	if(guiInfo.mpcontext)
		mixer = mpctx_get_mixer(guiInfo.mpcontext);

    switch(event)
    {
        case evPlay:
        case evPlaySwitchToPause:
        case evPauseSwitchToPlay:
			uiPlay();
            break;
        case evPause:
            uiPause();
            break;
#ifdef CONFIG_DVDREAD
		case evSetDVDAngle:
		{
			 char command[64];
			 snprintf(command, sizeof(command), "switch_angle %d", iparam);
			 guiInfo.Angle = iparam;
			 mp_input_queue_cmd(mp_input_parse_cmd(command));
	         break;
		}
	    case ivSetDVDChapter:
		{
			 char command[64];
			 snprintf(command, sizeof(command), "seek_chapter %d 1", iparam);
			 guiInfo.Chapter = iparam;
			 mp_input_queue_cmd(mp_input_parse_cmd(command));	          
	         break;
		}
		case ivSetDVDTitle:
		{
			 guiInfo.Track = iparam;
			 goto playdvd;

	         break;
		}
        case evPlayDVD:
        {
			char buffer[MAX_PATH];

			D(kprintf("evPlayDVD\n"));
			D(kprintf("dvd_title %d dvd_chapter %d dvd_angle %d\n", dvd_title, dvd_chapter, dvd_angle));

			guiInfo.Track = 1;
playdvd:
			guiInfo.Chapter = 1;
			guiInfo.Angle = 1;

			if(!dvd_device) break; // warn user with a requester

			D(kprintf("-> title %d chapter %d angle %d\n", guiInfo.Track, guiInfo.Chapter, guiInfo.Angle));
			uiSetFileName(NULL, dvd_device, gui_use_dvdnav ? STREAMTYPE_DVDNAV: STREAMTYPE_DVD);

			/* DVD title */
            dvdname[0] = 0;
			buffer[0] = 0;
			dvd_read_name(buffer, NULL, dvd_device);
			capitalize(buffer);
			snprintf(dvdname, sizeof(dvdname), "DVD Movie %s", buffer[0] ? buffer : dvd_device );
            mp_msg(MSGT_GPLAYER, MSGL_V, "Opening DVD %s -> %s\n", dvd_device, dvdname);

			guiInfo.StreamType = (gui_use_dvdnav ? STREAMTYPE_DVDNAV : STREAMTYPE_DVD);
			gui(GUI_PREPARE, 0);

			mygui->playlist->clear_playlist(mygui->playlist);
			mygui->playlist->add_track(mygui->playlist, gui_use_dvdnav ? "dvdnav://" : "dvd://", NULL, dvdname, 0, gui_use_dvdnav ? STREAMTYPE_DVDNAV: STREAMTYPE_DVD);

			// Rebuild playlist
			update_playlistwindow(TRUE);

			mygui->startplay(mygui);

            break;
        }
#endif
        case evDropFile:
        case evLoadPlay:
        {
			D(kprintf("evLoadPlay\n"));

			switch(guiInfo.StreamType)
            {
#ifdef CONFIG_DVDREAD
				case STREAMTYPE_DVDNAV:
				case STREAMTYPE_DVD:
                {
					D(kprintf("title %d chapter %d angle %d\n", guiInfo.Track, guiInfo.Chapter, guiInfo.Angle));

					gui(GUI_SET_STATE, (void *) GUI_PLAY);
                    break;
                }
#endif
                default:
                {
					guiInfo.NewPlay = GUI_FILE_NEW;
					mplGotoTheNext = guiInfo.Playing ? 0 : 1;
					gui(GUI_SET_STATE, (void *) GUI_STOP);
					gui(GUI_SET_STATE, (void *) GUI_PLAY);
                    break;
               }
           }

		   // Update playlist cursor
		   update_playlistwindow(FALSE);
           break;
        }
        case evNext:
            uiNext();
            break;
        case evPrev:
            uiPrev();
            break;
        case evFullScreen:
            mp_input_queue_cmd(mp_input_parse_cmd("vo_fullscreen"));
            break;
        case evExit:
        {
            /* We are asking mplayer to exit, later it will ask us after uninit is made
               this should be the only safe way to quit */
            mp_input_queue_cmd(mp_input_parse_cmd("quit"));
            break;
        }
        case evStop:
			if(guiInfo.Playing)
				gui(GUI_SET_STATE, GUI_STOP);
            break;

	    case evForward10min:     uiRelSeek( 600 ); break;
	    case evBackward10min:    uiRelSeek( -600 );break;
	    case evForward1min:      uiRelSeek( 60 );  break;
	    case evBackward1min:     uiRelSeek( -60 ); break;
	    case evForward10sec:     uiRelSeek( 10 );  break;
	    case evBackward10sec:    uiRelSeek( -10 ); break;
	    case evSetMoviePosition: uiAbsSeek( param ); break;
		
		case evSetBalance:
        case evSetVolume:
        {
            float l,r;

			if (guiInfo.Playing == 0)
                break;

			if (guiInfo.Balance == 50.0f)
				mixer_setvolume(mixer, guiInfo.Volume, guiInfo.Volume);

            l = guiInfo.Volume * ((100.0f - guiInfo.Balance) / 50.0f);
            r = guiInfo.Volume * ((guiInfo.Balance) / 50.0f);

            if (l > guiInfo.Volume) l=guiInfo.Volume;
            if (r > guiInfo.Volume) r=guiInfo.Volume;
            mixer_setvolume(mixer, l, r);
            /* Check for balance support on mixer - there is a better way ?? */
            if (r != l)
            {
                mixer_getvolume(mixer, &l, &r);
                if (r == l)
                {
                    mp_msg(MSGT_GPLAYER, MSGL_V, "[GUI] Mixer doesn't support balanced audio\n");
                    mixer_setvolume(mixer, guiInfo.Volume, guiInfo.Volume);
                    guiInfo.Balance = 50.0f;
                }
            }
            break;
        }
        case evMute:
        {
            mp_cmd_t * cmd = calloc(1, sizeof(*cmd));
            cmd->id=MP_CMD_MUTE;
            cmd->name=strdup("mute");
            mp_input_queue_cmd(cmd);
            break;
        }
    }
}

void uiPlay( void )
{
   D(kprintf("uiPlay\n"));

   if((!guiInfo.Filename ) || (guiInfo.Filename[0] == 0))
     return;

   if(guiInfo.Playing > GUI_STOP)
   {
       uiPause();
       return;
   }
   guiInfo.NewPlay = GUI_FILE_SAME;
   gui(GUI_SET_STATE, (void *)GUI_PLAY);
}

void uiPause( void )
{
   D(kprintf("uiPause\n"));

	switch(guiInfo.Playing)
	{
		case GUI_PLAY:
		{
		    mp_cmd_t * cmd = calloc(1, sizeof(*cmd));
		    cmd->id=MP_CMD_PAUSE;
		    cmd->name=strdup("pause");
		    mp_input_queue_cmd(cmd);
			break;
		}

		case GUI_PAUSE:
			guiInfo.Playing = GUI_PLAY;
			break;

		case GUI_STOP:
			break;
	}
}

void uiNext(void)
{
	D(kprintf("uiNext\n"));
	if(guiInfo.Playing == GUI_PAUSE) return;
    switch(guiInfo.StreamType)
    {

#ifdef CONFIG_DVDREAD
        case STREAMTYPE_DVD:
		case STREAMTYPE_DVDNAV:
			if(guiInfo.Chapter == guiInfo.Chapters)
				return;
			mygui->playercontrol(ivSetDVDChapter, (guiInfo.Chapter - 1) + 1);
            break;
#endif

        default:

            if(mygui->playlist->current == (mygui->playlist->trackcount - 1))
                return;
			if(!mygui->playlist->tracks)
				return;
			mygui->playlist->current++;
			uiSetFileName(NULL, mygui->playlist->tracks[mygui->playlist->current]->filename,
						   mygui->playlist->tracks[mygui->playlist->current]->type);

			mygui->startplay(mygui);

            break;
    }
}

void uiPrev(void)
{
	D(kprintf("uiPrev\n"));

	if(guiInfo.Playing == GUI_PAUSE)
		return;

    switch(guiInfo.StreamType)
    {

#ifdef CONFIG_DVDREAD
        case STREAMTYPE_DVD:
		case STREAMTYPE_DVDNAV:
			if(guiInfo.Chapter == 1)
                return;
			mygui->playercontrol(ivSetDVDChapter, (guiInfo.Chapter - 1) -1);

            break;
#endif

        default:
            if(mygui->playlist->current == 0)
                return;
			if(!mygui->playlist->tracks)
				return;
			mygui->playlist->current--;
			uiSetFileName(NULL, mygui->playlist->tracks[mygui->playlist->current]->filename,
                           mygui->playlist->tracks[mygui->playlist->current]->type);
			
			mygui->startplay(mygui);
			break;
    }
}

void uiEnd( void )
{
	D(kprintf("uiEnd\n"));

	/* Repeat current file */
	if(guiInfo.Playing && gui_repeat_mode == 1)
	{
		mplGotoTheNext = 1;

		if(!mygui->playlist->tracks)
			return;

		update_playlistwindow(FALSE);
		gui(GUI_SET_STATE, (void *) GUI_PLAY);
		return;
	}

	if(!mplGotoTheNext && guiInfo.Playing)
    {
        mplGotoTheNext = 1;
        return;
    }

    if(mplGotoTheNext && guiInfo.Playing &&
	  (mygui->playlist->current < (mygui->playlist->trackcount - 1)))
    {
        /* we've finished this file, reset the aspect */
        if(movie_aspect >= 0)
            movie_aspect = -1;

		mplGotoTheNext = 1;
		guiInfo.NewPlay = GUI_FILE_NEW;

		if(mygui->playlist->tracks)
		{
			mygui->playlist->current++;
			uiSetFileName(NULL, mygui->playlist->tracks[mygui->playlist->current]->filename, mygui->playlist->tracks[mygui->playlist->current]->type);
		}

		// Update playlist cursor
		update_playlistwindow(FALSE);
    }

	if(guiInfo.NewPlay == GUI_FILE_NEW)
        return;

	guiInfo.ElapsedTime   = 0;
	guiInfo.Position      = 0;
	guiInfo.AudioChannels = 0;

#ifdef CONFIG_DVDREAD
	guiInfo.Track   = 1;
	guiInfo.Chapter = 1;
	guiInfo.Angle   = 1;
#endif

    if (mygui->playlist->current == (mygui->playlist->trackcount - 1))
        mygui->playlist->current = 0;

	// Update playlist cursor
	update_playlistwindow(FALSE);

	if(!mygui->embedded)
		fullscreen = 0; // See what it really implies

	/* Loop over playlist, or just stop */
	if(gui_repeat_mode == 2)
	{
		gui(GUI_SET_STATE, (void *) GUI_PLAY);
	}
	else if(gui_repeat_mode == 3)
	{
		exit_player(EXIT_NONE);
	}
	else
	{
		gui(GUI_SET_STATE, (void *) GUI_STOP);
		gui(GUI_SHOW_PANEL, (void *) TRUE);
	}
}

void uiSetFileName(char *dir, char *name, int type)
{
	D(kprintf("uiSetFileName\n"));

	if(!name)
		return;

    if(!dir)
        guiSetFilename(guiInfo.Filename, name)
    else
        guiSetDF(guiInfo.Filename, dir, name);

	D(kprintf("guiInfo.Filename = <%s> type %d\n", guiInfo.Filename, type));

    guiInfo.StreamType = type;
	free(guiInfo.AudioFilename);
	guiInfo.AudioFilename = NULL;
	free(guiInfo.SubtitleFilename);
	guiInfo.SubtitleFilename = NULL;

	/* reset subtitles */
	guiClearSubtitle();
}

void uiFullScreen( void )
{
	D(kprintf("uiFullScreen\n"));

	if(!guiInfo.sh_video)
		return;

	D(kprintf("uiFullScreen2 %p\n", video_out));

	if(video_out && video_out->control(VOCTRL_FULLSCREEN, 0) == VO_FALSE)
	{
		mp_msg(MSGT_GPLAYER, MSGL_INFO, "[GUI] Couldn't switch to window/fullscreen mode");
		exit_player(EXIT_ERROR);
	}
}

int stream_interrupt_cb(int timeout)
{
    usec_sleep(timeout * 1000);
	return mygui->running == FALSE; 
}

void initguimembers(void)
{
	const vo_functions_t * vo;

	stccpy(cfg_dvd_device, dvd_device ? dvd_device : DEFAULT_DVD_DEVICE, sizeof(cfg_dvd_device)); /* workaround for dvddirectory to avoid overwriting original dvd_device */

	memset(mygui, 0, sizeof(*mygui));

	// Shall embedded mode be used ?
	mygui->embedded = FALSE;

	if((vo = init_best_video_out(video_driver_list)))
	{
		if(vo->info && (!strcmp(vo->info->short_name, "cgx_overlay_gui") || !strcmp(vo->info->short_name, "cgx_wpa_gui")))
		{
			mygui->embedded = TRUE;
		}
		Cgx_ControlBlanker(NULL, FALSE); /* hack to equilibrate blanker count */
        if(vo->uninit) vo->uninit();
	}

	mygui->colorkey = 0;
	mygui->startplay = startplay;
	mygui->playercontrol = uiEventHandling;
	mygui->updatedisplay = updatedisplay;
	mygui->playlist = create_playlist();
}

static int guiThread(void)
{
	int rc = 1;
	struct startupmsg * msg;

	stream_set_interrupt_callback(stream_interrupt_cb);

	/* Set cfg items */

	quiet = 1; // avoid being spammed by unwanted status messages

    if(autosync && autosync != gtkAutoSync)
    {
       gtkAutoSyncOn = 1;
       gtkAutoSync = autosync;
    }

	if(subdata)
		guiSetFilename(guiInfo.SubtitleFilename, subdata->filename);

#ifdef CONFIG_ASS
	gtkASS.enabled       = ass_enabled;
	gtkASS.use_margins   = ass_use_margins;
	gtkASS.top_margin    = ass_top_margin;
    gtkASS.bottom_margin = ass_bottom_margin;
#endif

	/* always add screenshot filter */
	m_config_set_option(mconfig, "vf-add", "screenshot");

#if !defined(__AROS__)
	if (NewGetTaskAttrs(NULL, &msg, sizeof(struct startupmsg *),
					  TASKINFOTYPE_STARTUPMSG,
					  TAG_DONE) && msg)
#else
    msg = ((struct Process *)FindTask(NULL))->pr_Task.tc_UserData;
#endif
	{
		mygui = NULL;
		gui_initialized = FALSE;
		rc = creategui();
	}

	mp_msg(MSGT_GPLAYER, MSGL_V, "[GUI] GUI thread terminated\n");

//	  guithread->finished = TRUE;

	gui_failed = rc;

#if defined(__AROS__)
    ReplyMsg((struct Message *)msg);
#endif
    return 0;
}

void guiInit(void)
{
	memset(&guiInfo, 0, sizeof(guiInfo));

	if(guithread) return; /* already exists, ignore */

	/* Create gui thread */
	if(!gui_initialized)
	{
		guithread = RunThread(guiThread, THREAD_NAME, NULL, (APTR) NULL, 0);
		mp_msg(MSGT_GPLAYER, MSGL_V, "[GUI] Creating GUI Thread\n");
	}

	/* Wait until gui is created */
	while(!gui_initialized)
	{
		if(gui_failed)
		{
			WaitForThread(guithread);
			guithread = NULL;
			mp_msg(MSGT_IDENTIFY, MSGL_INFO, "[GUI] Failed to initialize GUI module\n");
			exit_player(EXIT_ERROR);
		}

		usleep(100*1000);
	}

    mp_msg(MSGT_GPLAYER, MSGL_V, "[GUI] GUI thread started.\n");
}

static void guiFree(void)
{
	free( guiInfo.Filename );
	guiInfo.Filename = NULL;
	free( guiInfo.SubtitleFilename );
	guiInfo.SubtitleFilename = NULL;
	free( guiInfo.AudioFilename );
	guiInfo.AudioFilename = NULL;
}

void guiDone(void)
{
	D(kprintf("[MAIN] guiDone()\n"));
	if(gui_initialized)
    {
		mp_msg(MSGT_GPLAYER, MSGL_V, "[GUI] Closed by main mplayer window\n");

		D(kprintf("[MAIN] signaling E to gui thread (to quit main loop)\n"));
		SignalThread(guithread);

		D(kprintf("[MAIN] signaling F to gui thread\n"));
		Signal((struct Task *) guithread->task, SIGBREAKF_CTRL_F);

		D(kprintf("[MAIN] waiting for C|F signal\n"));
		Wait(SIGBREAKF_CTRL_F|SIGBREAKF_CTRL_C);

		D(kprintf("[MAIN] waiting for thread startupmsg reply\n"));
		WaitForThread(guithread);
		guithread = NULL;
		gui_initialized = FALSE;

		D(kprintf("[MAIN] bye bye\n"));
	}

	Cgx_BlankerState(); /* just check blanker count is ok */

	if(mygui)
	{
		if(mygui->playlist)
			mygui->playlist->free_playlist(mygui->playlist);
		mygui->playlist = NULL;

		free(mygui->icy_info);

		free(mygui);
		mygui = NULL;
    }

	guiFree();

	free(dvd_device);
	dvd_device = NULL;
}

#ifdef CONFIG_DVDREAD
typedef struct
{
	dvdnav_t *dvdnav;
} dvdnavpriv_t;
#endif

/* this function gets called by mplayer to update the gui */
int gui(int what, void *data)
{
    mixer_t *mixer = NULL;
	stream_t *stream;
#ifdef CONFIG_DVDREAD
	dvd_priv_t *dvd = (dvd_priv_t *) data;
	dvdnavpriv_t * dvdnav = (dvdnavpriv_t *) data;
#endif
    if(!mygui) return 0;

    if (guiInfo.mpcontext)
	{
		audio_out = mpctx_get_audio_out(guiInfo.mpcontext);
        video_out = mpctx_get_video_out(guiInfo.mpcontext);
        mixer = mpctx_get_mixer(guiInfo.mpcontext);
        playtree = mpctx_get_playtree_iter(guiInfo.mpcontext);
	}

	switch (what)
    {
		case GUI_SET_CONTEXT:
		{
			D(kprintf("GUI_SET_CONTEXT %p\n", data));
			guiInfo.mpcontext = data;
            break;
		}

		case GUI_SET_STATE:
        {
			D(kprintf("GUI_SET_STATE\n"));
			switch ((int)data)
			{
		        case GUI_STOP:
		        case GUI_PLAY:
		        case GUI_PAUSE:
		            guiInfo.Playing = (int)data;
		            break;
	        }

			switch (guiInfo.Playing)
            {
				case GUI_PLAY:
                {
					/* workaround: make sure dvdnav really doesn't have cache */
					if(guiInfo.Filename && strstr(guiInfo.Filename, "dvdnav://"))
					{
						guiInfo.StreamType = STREAMTYPE_DVDNAV;
						stream_cache_size = -1;
					}

                    break;
                }
				case GUI_STOP:
                {
                    if(movie_aspect >= 0)
                        movie_aspect = -1;
                    break;
                }
				case GUI_PAUSE:
                    break;
            }

			// Update playlist cursor
			update_playlistwindow(FALSE);

            break;
        }
#if 0 /* Merge of 34866, partially moved to GUI_PREPARE */
	    case GUI_SET_FILE:
		{
			D(kprintf("GUI_SET_FILE\n"));
			// if ( guiInfo.Playing == 1 && guiInfo.NewPlay == GUI_FILE_NEW )
			if (guiInfo.NewPlay == GUI_FILE_NEW)
			{
	            dvd_title = 0;
				//audio_id  = -1;
                if(audio_id != -2) audio_id = -1; // Fixes -nosound
	            video_id  = -1;
	            dvdsub_id = -1;
	            vobsub_id = -1;

	            stream_cache_size = -1;
	            autosync  = 0;
	            force_fps = 0;

				free(mygui->icy_info);
				mygui->icy_info = NULL;
	        }

			guiInfo.sh_video = NULL; // NEW

			if(!mygui->playlist->tracks) return 0;
			guiSetFilename(guiInfo.Filename, mygui->playlist->tracks[mygui->playlist->current]->filename);
			filename = guiInfo.Filename;
			guiInfo.Track = mygui->playlist->current + 1; // NEW (needed?)

            if(gtkAONorm) greplace(&af_cfg.list, "volnorm", "volnorm");
            if(gtkAOExtraStereo)
            {
                char *name = malloc(12 + 20 + 1);
                snprintf(name, 12 + 20, "extrastereo=%f", gtkAOExtraStereoMul);
                name[12 + 20] = 0;
                greplace(&af_cfg.list, "extrastereo", name);
                free(name);
			}

			if(gtkAutoSyncOn) autosync = gtkAutoSync;

	        break;
		}
#endif
	    case GUI_HANDLE_EVENTS:
		{
			//D(kprintf("GUI_HANDLE_EVENTS\n"));
			muiEventHandling();
	        break;
		}

		case GUI_RUN_COMMAND:
        {
			D(kprintf("GUI_RUN_COMMAND %d\n", (int) data));
			mp_msg(MSGT_GPLAYER, MSGL_DBG2, "[interface] GUI_RUN_COMMAND: %d\n", (int)data);
			
			switch((int) data)
            {
				case MP_CMD_VO_FULLSCREEN:
					uiFullScreen();
                    break;

                case MP_CMD_QUIT:
                {
					exit_player(EXIT_NONE);
                    return 0;
                }

				case MP_CMD_GUI_HIDE:
					if(mygui->embedded && guiInfo.Playing)
					{
						if(video_out)
						{
							if(video_out->control(VOCTRL_GUI_NOWINDOW, 0) == VO_FALSE)
							{
								exit_player(EXIT_NONE);
							}
						}
					}
					break;

				case MP_CMD_GUI_UPDATESUBTITLE:
					DoMethod(app, MUIM_Application_PushMethod, mygui->maingroup, 1, MM_MPlayerGroup_RefreshSubtitles);
					break;

				case MP_CMD_GUI_UPDATEAUDIO:
					DoMethod(app, MUIM_Application_PushMethod, mygui->maingroup, 1, MM_MPlayerGroup_RefreshAudio);
					break;

				case MP_CMD_GUI_STOP: // Used?
					gui(GUI_SET_STATE, (void *) GUI_STOP);
                    break;

				case MP_CMD_GUI_PLAY: // Used?
					gui(GUI_SET_STATE, (void *) GUI_PLAY);
                    break;

		        case MP_CMD_PLAY_TREE_STEP:
					uiNext();
		            break;

		        case -MP_CMD_PLAY_TREE_STEP:
					uiPrev();
		            break;

                case MP_CMD_GUI_PLAYLIST:
					DoMethod(app, MUIM_Application_PushMethod, mygui->maingroup, 1, MM_MPlayerGroup_ShowPlaylist);
                    break;

                case MP_CMD_GUI_LOADFILE:
					DoMethod(app, MUIM_Application_PushMethod, mygui->maingroup, 1, MM_MPlayerGroup_OpenFileRequester);
                    break;

                case MP_CMD_GUI_LOADSUBTITLE:
					DoMethod(app, MUIM_Application_PushMethod, mygui->maingroup, 1, MM_MPlayerGroup_OpenSubtitleRequester);
                    break;

                default:
                    break;
            }
            break;
        }

		case GUI_PREPARE:
        {
			D(kprintf("GUI_PREPARE %d\n", guiInfo.StreamType));
            if (guiInfo.NewPlay == GUI_FILE_NEW)
            {
                dvd_title = 0;
                //audio_id  = -1;
                if(audio_id != -2) audio_id = -1; // Fixes -nosound
                video_id  = -1;
                dvdsub_id = -1;
                vobsub_id = -1;

                stream_cache_size = -1;
                autosync  = 0;
                force_fps = 0;

                free(mygui->icy_info);
                mygui->icy_info = NULL;
            }

            guiInfo.sh_video = NULL; // NEW

			switch(guiInfo.StreamType)
            {
                case STREAMTYPE_PLAYLIST:
                    break;
#ifdef CONFIG_DVDREAD
				case STREAMTYPE_DVDNAV:
					stream_cache_size = -1;
					// fall through
				case STREAMTYPE_DVD:
                {
					char tmp[512];

					//dvd_title   = 0; /*guiIntfStruct.DVD.current_title - 1*/;
					//dvd_chapter = 1  /*guiIntfStruct.DVD.current_chapter*/;
					//dvd_angle   = 1  /*guiIntfStruct.DVD.current_angle*/;

					D(kprintf("-> guiInfo.Track = %d dvd_title %d dvd_chapter %d dvd_angle %d\n", guiInfo.Track, dvd_title, dvd_chapter, dvd_angle));

					if(guiInfo.Track == 0)
					{
						guiInfo.Track = 1;
					}

					if(gui_use_dvdnav)
					{
						sprintf(tmp,"dvdnav://%d", guiInfo.Track);
					}
					else
					{
						sprintf(tmp,"dvd://%d", guiInfo.Track);
					}

					guiSetFilename(guiInfo.Filename, tmp);

					dvd_chapter = guiInfo.Chapter;
					dvd_angle = guiInfo.Angle;

                    break;
                }
#endif
            }

			if(!mygui->playlist->tracks) return 0;
			guiSetFilename(guiInfo.Filename, mygui->playlist->tracks[mygui->playlist->current]->filename);

			if(guiInfo.Filename)
				filename = guiInfo.Filename;
			else if(filename && guiInfo.Filename)
				strcpy(guiInfo.Filename, filename);

			// NEW see GTK handling
			guiInfo.VideoWindow = TRUE;

			stream_dump_type = 0;
            mplayerLoadFont();

			guiInfo.NewPlay = 0;

#ifdef CONFIG_ASS
	        ass_enabled       = gtkASS.enabled;
	        ass_use_margins   = gtkASS.use_margins;
	        ass_top_margin    = gtkASS.top_margin;
	        ass_bottom_margin = gtkASS.bottom_margin;
#endif
	        break;
        }

		case GUI_SET_STREAM:
        {
	        stream = data;

			D(kprintf("GUI_SET_STREAM %d\n", stream->type));
			guiInfo.StreamType = stream->type;

			switch(guiInfo.StreamType)
            {
#ifdef CONFIG_DVDREAD
                case STREAMTYPE_DVD:
				case STREAMTYPE_DVDNAV:
					gui(GUI_SET_DVD, (void *) stream->priv);
                    break;
#endif
            }

			/* Cache */
			switch(guiInfo.StreamType)
			{
				case STREAMTYPE_DVD:
					stream_cache_size = gtkCacheSizeDVD;
					break;
				case STREAMTYPE_DVDNAV:
					stream_cache_size = - 1;
					break;
				case STREAMTYPE_STREAM:
					stream_cache_size = gtkCacheSizeNet;
					break;
				default:
				case STREAMTYPE_FILE:
					stream_cache_size = gtkCacheSizeFile;
					break;
			}

			/* Title  */
			switch(guiInfo.StreamType)
			{
				case STREAMTYPE_DVD:
				case STREAMTYPE_DVDNAV:
					snprintf(windowtitle, sizeof(windowtitle), "MPlayer: %s", dvdname);
					break;
				default:
				case STREAMTYPE_STREAM:
				case STREAMTYPE_FILE:
					snprintf(windowtitle, sizeof(windowtitle), "MPlayer: %s", guiInfo.Filename);
					break;
			}

            if(!mygui->embedded || !fullscreen)
				DoMethod(app, MUIM_Application_PushMethod, mygui->mainwindow, 3, MUIM_Set, MUIA_Window_Title, windowtitle);

			D(kprintf("GUI_SET_STREAM OK\n"));

            break;
        }
#ifdef CONFIG_DVDREAD
		case GUI_SET_DVD:
        {
			D(kprintf("GUI_SET_DVD\n"));

#warning "See if we couldn't use more generic demuxer functions here, really. It exposes really too much of dvd internals"

			if(guiInfo.StreamType == STREAMTYPE_DVD)
			{
				D(kprintf("dvd_title %d dvd_chapter %d dvd_angle %d\n", dvd_title, dvd_chapter, dvd_angle));
	            guiInfo.Tracks       = dvd->vmg_file->tt_srpt->nr_of_srpts;
	            guiInfo.Chapters     = dvd->vmg_file->tt_srpt->title[dvd_title].nr_of_ptts;
	            guiInfo.Angles       = dvd->vmg_file->tt_srpt->title[dvd_title].nr_of_angles;
	            guiInfo.AudioStreams = dvd->nr_of_channels;
	            memcpy(guiInfo.AudioStream, dvd->audio_streams, sizeof(dvd->audio_streams));
	            guiInfo.Subtitles = dvd->nr_of_subtitles;
	            memcpy(guiInfo.Subtitle, dvd->subtitles, sizeof(dvd->subtitles));
	            guiInfo.Track   = dvd_title + 1;
				guiInfo.Chapter = dvd_chapter /*+ 1*/;
	            guiInfo.Angle   = dvd_angle + 1;
			}
			else if(guiInfo.StreamType == STREAMTYPE_DVDNAV)
			{
			    uint8_t lg;
				uint16_t i, j, lang, format, id, channels;
			    int base[7] = {128, 0, 0, 0, 160, 136, 0};
			    char tmp[3];

				dvdnav_current_title_info(dvdnav->dvdnav,
										  &guiInfo.Track,
										  &guiInfo.Chapter);
				dvdnav_get_number_of_titles(dvdnav->dvdnav, &guiInfo.Tracks);
				dvdnav_get_number_of_parts(dvdnav->dvdnav, guiInfo.Track, &guiInfo.Chapters);

				dvdnav_get_angle_info(dvdnav->dvdnav,
									  &guiInfo.Angle,
									  &guiInfo.Angles);

				j = 0;
				for(i=0; i<8; i++)
			    {
				  lg = dvdnav_get_audio_logical_stream(dvdnav->dvdnav, i);
			      if(lg == 0xff) continue;

				  channels = dvdnav_audio_stream_channels(dvdnav->dvdnav, lg);
			      if(channels == 0xFFFF)
			        channels = 2; //unknown
			      else
			        channels--;
				  lang = dvdnav_audio_stream_to_lang(dvdnav->dvdnav, lg);
			      if(lang == 0xFFFF)
			        tmp[0] = tmp[1] = '?';
			      else
			      {
			        tmp[0] = lang >> 8;
			        tmp[1] = lang & 0xFF;
			      }
			      tmp[2] = 0;
				  format = dvdnav_audio_stream_format(dvdnav->dvdnav, lg);
			      if(format == 0xFFFF || format > 6)
			        format = 1; //unknown
			      id = i + base[format];

				  guiInfo.AudioStream[j].id = id;
				  guiInfo.AudioStream[j].language = lang;
				  guiInfo.AudioStream[j].channels = channels;
				  guiInfo.AudioStream[j].type = format;

				  j++;
			    }

				guiInfo.AudioStreams = j;

				// NEW Subtitles members?

				//guiIntfStruct.Track = guiIntfStruct.DVD.current_title + 1;
			}

			D(kprintf("-> titles %d chapters %d angles %d\n", guiInfo.Tracks, guiInfo.Chapters, guiInfo.Angles));
			D(kprintf("-> current_title %d current_chapter %d current_angle %d\n", guiInfo.Track, guiInfo.Chapter, guiInfo.Angle));

 			break;
        }
#endif

	    case GUI_SET_AFILTER:
		{
	        guiInfo.afilter = data;
	        break;
		}

	    case GUI_SET_VIDEO:
		{
			D(kprintf("GUI_SET_VIDEO\n"));

	        guiInfo.sh_video = data;

			if (guiInfo.sh_video)
            {
				codecname = guiInfo.sh_video->codec->name;

				guiInfo.FPS = guiInfo.sh_video->fps;
				guiInfo.VideoWidth = guiInfo.sh_video->disp_w;
				guiInfo.VideoHeight = guiInfo.sh_video->disp_h;

				D(kprintf("Codec %s, FPS %d, Width %d, Height %d\n", codecname, (int)guiInfo.FPS, guiInfo.VideoWidth, guiInfo.VideoHeight));

				sub_aspect = (float)guiInfo.VideoWidth / guiInfo.VideoHeight;

				DoMethod(app, MUIM_Application_PushMethod, mygui->cropgroup, 1, MM_CropGroup_UpdateDimensions);
				DoMethod(app, MUIM_Application_PushMethod, mygui->scalegroup, 1, MM_ScaleGroup_UpdateDimensions);
            }

			// make sure controls are shown if this is an audio only file.
			if (!guiInfo.sh_video)
			{
				gui_show_gui = TRUE;
				gui_show_control = gui_show_status = gui_show_toolbar = TRUE;
				DoMethod(app, MUIM_Application_PushMethod, mygui->maingroup, 2, MM_MPlayerGroup_ShowPanels, gui_show_gui);

				// switch back to window mode for audio only
				if(mygui->embedded)
				{
					gui(GUI_SET_WINDOW_PTR, NULL);
				}
			}

			// show/hide video area if there's a video stream or not
			DoMethod(app, MUIM_Application_PushMethod, mygui->videoarea, 3, MUIM_Set, MUIA_ShowMe, mygui->embedded ? guiInfo.sh_video != NULL : FALSE);

			break;
		}

	    case GUI_SET_AUDIO:
		{
	        guiInfo.AudioChannels = data ? ((sh_audio_t *)data)->channels : 0;

	        if (data && !guiInfo.sh_video)
				guiInfo.VideoWindow = FALSE;

	        gui(GUI_SET_MIXER, 0);
	        break;
		}

		/*
		case GUI_SET_AUDIOONLY:
			break;

		case GUI_SET_FILE_FORMAT:
			break;
		*/

	    case GUI_SET_MIXER:
		{
			if (mixer)
			{
	            float l, r;
	            static float last_balance = -1;

	            mixer_getvolume(mixer, &l, &r);

	            guiInfo.Volume = FFMAX(l, r);

				if (guiInfo.Balance != last_balance)
				 {
	                if (guiInfo.Volume)
	                    guiInfo.Balance = ((r - l) / guiInfo.Volume + 1.0) * 50.0;
	                else
	                    guiInfo.Balance = 50.0f;

	                last_balance = guiInfo.Balance;
	            }
	        }
	        break;
		}

		case GUI_REDRAW:
            break;

		case GUI_SETUP_VIDEO_WINDOW:
		{
			D(kprintf("GUI_SETUP_VIDEO_WINDOW\n"));
			guiInfo.VideoWidth  = vo_dwidth;
			guiInfo.VideoHeight = vo_dheight;

			// Do something?
			break;
		}

		case GUI_END_FILE:
		{
			D(kprintf("GUI_END_FILE\n"));
			uiEnd();
			break;
		}

		case GUI_SET_DEMUXER:
		{
			D(kprintf("GUI_SET_DEMUXER %p\n", data));
			guiInfo.demuxer = data;

			/* At this point (demuxer set), we know the stream attributes */
			DoMethod(app, MUIM_Application_PushMethod, mygui->maingroup, 2, MM_MPlayerGroup_SetValues, data != NULL);
			if(data)
			{
				// Maybe use another var?
				guiInfo.Chapters = demuxer_chapter_count(guiInfo.demuxer);

				// Fill stream properties
				DoMethod(app, MUIM_Application_PushMethod, mygui->propertiesgroup, 1, MM_PropertiesGroup_Update);

				// Show/Hide DVD Browser
				DoMethod(app, MUIM_Application_PushMethod, mygui->maingroup, 2, MM_MPlayerGroup_ShowDVDBrowser, guiInfo.StreamType == STREAMTYPE_DVDNAV || guiInfo.StreamType == STREAMTYPE_DVD);

				// Chapters can also exist for other streams than DVD
				DoMethod(app, MUIM_Application_PushMethod, mygui->maingroup, 2, MM_MPlayerGroup_ShowChapterBrowser, guiInfo.Chapters > 1);

				// Refresh chapter/title/angle/audio/subtitle poplists/menus
				DoMethod(app, MUIM_Application_PushMethod, mygui->maingroup, 1, MM_MPlayerGroup_RefreshSubtitles);
				DoMethod(app, MUIM_Application_PushMethod, mygui->maingroup, 1, MM_MPlayerGroup_RefreshAudio);
			}
			break;
		}

		case GUI_SET_WINDOW_PTR:
		{
			D(kprintf("GUI_SET_WINDOW_PTR %p\n", data));
			if(mygui->embedded)
			{
				DoMethod(app, MUIM_Application_PushMethod, mygui->maingroup, 2, MM_MPlayerGroup_OpenFullScreen, (APTR) data);
			}
			else
			{
#if !defined(__AROS__) /* AROS: Don't put MPlayer window on screen which has video */
				mygui->gui_ready = FALSE;
				DoMethod(app, MUIM_Application_PushMethod, mygui->maingroup, 2, MM_MPlayerGroup_SetWindow, (APTR) data);
				while(mygui->running && !mygui->gui_ready) // wait until SetWindow is finished or gui is quitting
				{
					Delay(1);
				}
#endif
			}
			break;
		}

		case GUI_SHOW_PANEL:
		{
			D(kprintf("GUI_SHOW_PANEL %p\n", data));

			gui_show_gui = (ULONG) data;

			if(!mygui->embedded)
			{
				DoMethod(app, MUIM_Application_PushMethod, mygui->maingroup, 2, MM_MPlayerGroup_Show, gui_show_gui);
			}
			else
			{
                DoMethod(app, MUIM_Application_PushMethod, mygui->maingroup, 2, MM_MPlayerGroup_ShowPanels, gui_show_gui);
			}
			break;
		}

		case GUI_LOAD_FILE:
		{
			char * path = (char *) data;
			ULONG previoustrackcount = mygui->playlist->trackcount;

			D(kprintf("GUI_LOAD_FILE %s\n", data));

			if(!parse_filename(path, playtree, mconfig, 0))
			{
				mygui->playlist->add_track(mygui->playlist, path, NULL, NULL, 0, STREAMTYPE_FILE);
			}

			mygui->playlist->current = previoustrackcount;
			
			// Rebuild playlist
			update_playlistwindow(TRUE);

			uiSetFileName(NULL, mygui->playlist->tracks[mygui->playlist->current]->filename, STREAMTYPE_FILE);
			mygui->playercontrol(evLoadPlay, 0);

			break;		  
		}

        default:
			mp_msg(MSGT_GPLAYER, MSGL_ERR, "[GUI] GOT UNHANDLED EVENT %i\n", what);
    }

	return 1;
}

/* This function adds/inserts one file into the gui playlist */
static int import_file_into_gui(char *pathname, int insert)
{
    char filename[MAX_PATH];
	struct stat st;

	if (strstr(pathname, "dvd://") || strstr(pathname, "dvdnav://"))
	{
		char buffer[MAX_PATH];

		/* Set DVD title here too */
		ULONG dvdnav = strstr(pathname, "dvdnav://") != NULL;

        dvdname[0] = 0;
		buffer[0] = 0;
		dvd_read_name(buffer, NULL, dvd_device);
		capitalize(buffer);
		snprintf(dvdname, sizeof(dvdname), "DVD Movie %s", buffer[0] ? buffer : dvd_device );
		mp_msg(MSGT_GPLAYER, MSGL_INFO, "[GUI] Adding DVD %s\n", dvdname);
		mygui->playlist->add_track(mygui->playlist, pathname, NULL, dvdname, 0, dvdnav ? STREAMTYPE_DVDNAV : STREAMTYPE_DVD);

        return 1;
	}
	else if(strstr(pathname, "://"))
    {
		mp_msg(MSGT_GPLAYER, MSGL_INFO, "[GUI] Adding special %s\n", pathname);
		mygui->playlist->add_track(mygui->playlist, pathname, NULL, pathname, 0, STREAMTYPE_STREAM);

        return 1;
    }

	if(stat(pathname, &st) != -1)
	{
		if (!S_ISDIR(st.st_mode))
	    {
			mp_msg(MSGT_GPLAYER, MSGL_INFO, "[GUI] Adding filename: %s - fullpath: %s\n", FilePart(pathname), pathname);
			mygui->playlist->add_track(mygui->playlist, pathname, NULL, NULL, 0, STREAMTYPE_FILE);

	        return 1;
	    }
	    else
		{
			mp_msg(MSGT_GPLAYER, MSGL_INFO, "[GUI] Cannot add %s\n", filename);
		}
	}

    return 0;
}

// This function imports the initial playtree (based on cmd-line files)
// into the gui playlist by either:
// - overwriting gui pl (enqueue=0)
// - appending it to gui pl (enqueue=1)
int guiPlaylistInitialize(play_tree_t *my_playtree, m_config_t *config, int enqueue)
{
    play_tree_iter_t *my_pt_iter = NULL;
    int result = 0;

    if(!mygui) guiInit();

	mygui->playlist->clear_playlist(mygui->playlist);

    if((my_pt_iter = pt_iter_create(&my_playtree, config)))
    {
        while ((filename = pt_iter_get_next_file(my_pt_iter)) != NULL)
        {
            if (parse_filename(filename, my_playtree, config, 0))
                result = 1;
            else if (import_file_into_gui(filename, 0)) /* Add it to end of list */
                result = 1;
        }
    }
    mplGotoTheNext = 1;

    if (result)
    {
		uiSetFileName(NULL, mygui->playlist->tracks[0]->filename, mygui->playlist->tracks[0]->type);
		mygui->playlist->current = 0;

		filename = guiInfo.Filename;
    }

	// Rebuild playlist
	update_playlistwindow(TRUE);

    return result;
}

// This function imports and inserts an playtree, that is created "on the fly",
// for example by parsing some MOV-Reference-File; or by loading an playlist
// with "File Open".
// The file which contained the playlist is thereby replaced with it's contents.
int guiPlaylistAdd(play_tree_t *my_playtree, m_config_t *config)
{
    play_tree_iter_t *my_pt_iter = NULL;
    int result = 0;

    if((my_pt_iter = pt_iter_create(&my_playtree, config)))
    {
        while ((filename = pt_iter_get_next_file(my_pt_iter)) != NULL)
            if (import_file_into_gui(filename, 1)) /* insert it into the list and set plCurrent = new item */
                result = 1;
        pt_iter_destroy(&my_pt_iter);
    }
    filename = NULL;

	// Rebuild playlist
	update_playlistwindow(TRUE);

    return result;
}

void SendMessage(int level, const char *str)
{
	if(mygui)
	{
        static TEXT status[1024];
		static ULONG i = 0;
		char *ptr = str;
		char *dst = status + i;

		while(*ptr && i < sizeof(status))
		{
            if(*ptr == '\n')
			{
				*dst = 0;
				i = 0;
				ptr++;

				// We can send now we have a new line.
				if(mygui->message_pushid)
				{
					DoMethod(app, MUIM_Application_KillPushMethod, mygui->consolegroup, mygui->message_pushid, MM_ConsoleGroup_AddMessage);
				}
				mygui->message_pushid = DoMethod(app, MUIM_Application_PushMethod, mygui->consolegroup, 2, MM_ConsoleGroup_AddMessage, strdup(status));
			}
			else if(*ptr == '\t')
			{
				ptr++;
			}
			else if(*ptr == '\r')
			{
				if(*(ptr + 1) != '\n')
				{
					// We should modify last line content there.
					ptr++;
				}
				else
				{
					ptr++;
				}
			}
			else if(*ptr == 0x1b)
			{
				if(*(ptr + 1))
				{
					ptr++;
					if(*(ptr + 2))
						ptr++;
				}
				ptr++;
			}
			else
			{
				*dst++ = *ptr++;
				i++;
			}
		}
	}
}

void gmp_msg(int mod, int level, const char *format, ...)
{
	static TEXT status[512];
	char str[512];
    va_list va;

    va_start(va, format);
	vsnprintf(str, sizeof(str), format, va);
    va_end(va);

    switch(level)
    {
        case MSGL_FATAL:
        case MSGL_ERR:
		case MSGL_INFO:
			SendMessage(level, str);
			break;
		case MSGL_STATUS:
		{
			if(mygui && gui_show_status && str && *str && *str != '\n')
			{
				ULONG secs, micros, currentTime;
				
				CurrentTime(&secs, &micros);

				currentTime = secs*1000 + micros/1000;
				
				// don't refresh too often
				if(currentTime > mygui->last_status_update + 10)
				{
					int i = 0;
					char *ptr = str;
					char *dst = status;
					
					while(*ptr && i < sizeof(status))
					{
						if(*ptr == '\n')
						{	 
							ptr++;
						}
						else if(*ptr == '\r')
						{
							ptr++;
						}
						else if(*ptr == '\t')
						{
							ptr++;
						}
						else if(*ptr == 0x1b) // Skip escape sequences
						{
							if(*(ptr + 1))
							{
								ptr++;
								if(*(ptr + 2))
									ptr++;
							}
							ptr++;
						}
						else
						{
							*dst++ = *ptr;
							i++;
							ptr++;
						}
					}
					*dst = 0;

					if(mygui->status_pushid)
					{
						DoMethod(app, MUIM_Application_KillPushMethod, mygui->maingroup, mygui->status_pushid, MM_MPlayerGroup_Update);
					}

                    mygui->last_status_update = currentTime;
					mygui->status_pushid = DoMethod(app, MUIM_Application_PushMethod, mygui->maingroup, 3, MM_MPlayerGroup_Update, MV_MPlayerGroup_Update_Status, status);
				}
				else
				{
					mygui->last_status_update = currentTime;
				}
			}
			break;
		}
    }
}

