#include <exec/types.h>
#include <proto/timer.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/asl.h>
#include <proto/alib.h>
#include <proto/icon.h>
#include <proto/wb.h>
#include <proto/intuition.h>

#include <workbench/workbench.h>
#include <workbench/icon.h>
#include <workbench/startup.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#define USE_INLINE_STDARG


#include "config.h"
#include "mp_msg.h"
#include "mixer.h"
#include "mp_core.h"
#include "playtree.h"
#include "input/input.h"
#include "stream/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"
#include "libvo/video_out.h"
#include "sub/subreader.h"

#include "morphos_stuff.h"
#ifdef CONFIG_GUI
#include "gui/interface.h"
#endif

int cache_network = 0; // obsolete option (just there to avoid breaking scripts)

extern MPContext * mpctx;
extern char* filename;
extern void dvd_read_name(char *name, char *serial, const char *device);
extern char* dvd_device;

static char windowtitle[256];
static char last_visited_path[1024];
static char default_pattern[1024] = "#?.(mp#?|m4a|aac|dat|st?|ts|#?xa|??v|vob|sfd|avi|divx|asf|qt|ra(~r)|iso|rv|rm#?|og?|cin|roq|fl#?|pls|m3u|m3u8)";
static char pattern[1024];

/* Special callback to report fontconfig cache creation progress */
extern void (*fontconfig_progress_callback)(int current, int total, char *currentfile);

void myfontconfig_progress_callback(int current, int total, char *currentfile)
{
	mp_msg(MSGT_CPLAYER, MSGL_STATUS, "[FontConfig] Indexing <%s> [%d/%d]\n", currentfile, current+1, total);
}

#if MPLAYER

/**********************************************************************************************************************/

/* REXX PORT */

#include <ctype.h>

#include <proto/rexxsyslib.h>
#include <rexx/storage.h>
#include <rexx/errors.h>

#define MPLAYER_PORTNAME_BASE    "MPLAYER"
#define RESULT_LEN      1024
#define PORT_LEN        80
#define CMD_LEN         1024

/* MPlayer vars */

extern int frame_dropping;
extern int sub_visibility;

extern int vo_gamma_gamma;
extern int vo_gamma_brightness;
extern int vo_gamma_contrast;
extern int vo_gamma_saturation;
extern int vo_gamma_hue;

/***************/

#if defined(__AROS__)
struct RxsLib * RexxSysBase = NULL;
#else
struct Library * RexxSysBase = NULL;
#endif

static struct MsgPort   *ARexxPort;
static char MPLAYER_PORTNAME[PORT_LEN];
static char REGISTERED_PORTNAME[PORT_LEN] = "";
static char RESULT[RESULT_LEN];

char icon_name[] = "PROGDIR:MPlayer.info";


int  rexx_init (void);
void rexx_exit (void);
void rexx_handle_events (void);
static int ADDRESS (char *hostname, char *cmd);

static int mplayer_starting = 0;
static int mplayer_quit = 0;
static int have_rexx = 0;
char * rexx_filename = NULL;

int gui_init (void)
{
	if (!have_rexx)
	{
		have_rexx = rexx_init ();
	}

	return 0;
}

void gui_exit (void)
{
	if(have_rexx)
		rexx_exit();
}

void gui_handle_events (void)
{
	if (have_rexx)
		rexx_handle_events();
}

int rexx_init (void)
{
	int num=1;
	struct MsgPort * foundport = NULL;

	RexxSysBase = (struct Library *) OpenLibrary ("rexxsyslib.library", 0L);
	if (!RexxSysBase)
	{
		mp_msg(MSGT_CPLAYER, MSGL_FATAL, "Unable to open rexxsyslib.library\n");
		return 0;
	}

	do
	{
		snprintf(MPLAYER_PORTNAME, PORT_LEN, "%s.%d", MPLAYER_PORTNAME_BASE, num);

		Forbid();
		foundport = FindPort(MPLAYER_PORTNAME);
		Permit();

		if (foundport)
		{
			mp_msg(MSGT_CPLAYER, MSGL_FATAL,  "Port \"%s\" already exists!\n", MPLAYER_PORTNAME);
		}
		num++;
	} while(foundport);

	ARexxPort = CreatePort (MPLAYER_PORTNAME, 0);
	if (!ARexxPort)
	{
		mp_msg(MSGT_CPLAYER, MSGL_FATAL, "Failed to open AREXX port \"%s\"!\n", MPLAYER_PORTNAME);
		return 0;
	}

	mp_msg(MSGT_CPLAYER, MSGL_INFO, "Rexx port \"%s\" installed.\n", MPLAYER_PORTNAME);

	rexx_handle_events ();

	return 1;
}

void rexx_exit (void)
{
	if(rexx_filename)
		free(rexx_filename);
	rexx_filename = NULL;

	if (ARexxPort)
	{
		struct RexxMsg *msg;

		Forbid ();
		while ((msg = (struct RexxMsg*)GetMsg (ARexxPort)))
		{
		   msg->rm_Result1 = RC_ERROR;
		   msg->rm_Result2 = 0;
		   ReplyMsg ((void*)msg);
		}
		DeletePort (ARexxPort);
		Permit ();
		ARexxPort = NULL;
	}

	if (RexxSysBase)
	{
		CloseLibrary ((void*)RexxSysBase);
		RexxSysBase = NULL;
	}
}

/****************************************************************************/
/* argument parsing routines
 */
static int matchstr (char **line,char *pat)
{
	char *s = *line;
	char match = 0;

	while (isspace (*s))
	++s;
	if (*s == '\"' || *s == '\'')
	match = *s++;
	while (*s && (tolower (*s) == tolower (*pat)) && (!match || *s != match))
	{++s;++pat;}
	if (match && *s == match && s[1])
	++s;
	if (!*pat && (!*s || isspace (*s))) {
	while (isspace (*s))
	    ++s;
	*line = s;
	return 1;
	}
	return 0;
}

/****************************************************************************/

static void extractstr (char **line, char *result, int len)
{
	char *s    = *line;
	char match = 0;

	while (isspace(*s))
	++s;

	if (*s == '\"' || *s == '\'')
	match = *s++;
	while (*s && *s != match) {
	if (*s == '\\' && (s[1] == '\'' || s[1] == '\"'))
	    ++s;
	if (len > 1) {
	    *result++ = *s;
	    --len;
	}
	++s;
	if (!match && isspace (*s))
	    break;
	}
	if (match && *s == match)
	++s;
	while (isspace (*s))
	++s;

	*result  = '\0';
	*line    = s;
}

/****************************************************************************/

static int matchnum (char **line)
{
	char *s = *line, match = 0;
	int sign = 1, num = 0;

	while (isspace (*s))
	++s;
	if (*s == '\"' || *s == '\'')
	match = *s++;
	if (*s == '-') {
	sign = -1;
	++s;
	}
	if (*s == '+')
	++s;
	while (isspace (*s))
	++s;
	while (*s >= '0' && *s <= '9')
	num = num * 10 + (*s++ - '0');
	if (match && *s == match && s[1])
	++s;
	while (isspace (*s))
	++s;
	*line = s;
	return sign > 0 ? num : -num;
}

static int ADDRESS (char *hostname, char *cmd)
{
	struct MsgPort *RexxPort,
		   *ReplyPort;
	struct RexxMsg *HostMsg,
		   *answer;
	int result = RC_WARN;

	if (!stricmp (hostname, "COMMAND"))
	return SystemTagList(cmd,NULL);

	if ((RexxPort = (void *)FindPort (hostname)))
	{
		if ((ReplyPort = (void *)CreateMsgPort ()))
		{
			if ((HostMsg = CreateRexxMsg (ReplyPort, NULL, hostname)))
			{
				int len = strlen (cmd); /* holger: trick for powerup */
				if ((HostMsg->rm_Args[0] = CreateArgstring (cmd, len)))
				{
				    HostMsg->rm_Action = RXCOMM | RXFF_RESULT;
				    PutMsg (RexxPort, (void*)HostMsg);
				    WaitPort (ReplyPort);
				    while (!(answer = (void *)GetMsg (ReplyPort)));
				    result = answer->rm_Result1;
					if (result == RC_OK)
					{
						if (answer->rm_Result2)
						{
						    strncpy (RESULT,(char *)answer->rm_Result2, RESULT_LEN);
						    DeleteArgstring ((char *)answer->rm_Result2);
						}
						else
							RESULT[0] = '\0';
				    }
				    DeleteArgstring (HostMsg->rm_Args[0]);
				}
				else
					strcpy (RESULT, "Can't create argstring!");
				DeleteRexxMsg (HostMsg);
			}
			else
				strcpy (RESULT, "Can't create rexx message!");
		    DeleteMsgPort (ReplyPort);
		}
		else
			strcpy (RESULT, "Can't alloc reply port!");
	}
	else
		sprintf (RESULT, "Port \"%s\" not found!", hostname);

	return result;
}

static int SEEK(char * line)
{
	int ret = RC_ERROR;
	char * res = "";

	mp_cmd_t * cmd = malloc(sizeof(mp_cmd_t));

	if(cmd)
	{
		int abs;
		int value;
		if(matchstr (&line, "ABS_PERCENT"))  // absolute seek %
		{
			abs = 1;
		}
		else if(matchstr (&line, "ABS_SECS")) // absolute seek in secs
		{
			abs = 2;
		}
		else  // relative seek
		{
			abs = 0;
		}

		value = matchnum(&line);

		cmd->pausing = 0;
		cmd->id    = MP_CMD_SEEK;
		cmd->name  = strdup("seek");
		cmd->nargs = 2;
		cmd->args[0].type = MP_CMD_ARG_FLOAT;
		cmd->args[0].v.f=value;
		cmd->args[1].type = MP_CMD_ARG_INT;
		cmd->args[1].v.i=abs;
		cmd->args[2].type = -1;
		cmd->args[2].v.i=0;

		if(mp_input_queue_cmd(cmd) == 0)
		{
			mp_cmd_free(cmd);
			res = "cmd queue full";
		}

		ret = RC_OK;
		res = "OK";
	}
	else
	{
		res = "not enough memory";
	}

	strncpy (RESULT, res, RESULT_LEN);
	return ret;
}

static int PAUSE(char * line)
{
	int ret = RC_ERROR;
	char * res = "";

	mp_cmd_t * cmd = malloc(sizeof(mp_cmd_t));

	if(cmd)
	{
		cmd->pausing = 0;
		cmd->id    = MP_CMD_PAUSE;
		cmd->name  = strdup("pause");
		cmd->nargs = 0;
		cmd->args[0].type = -1;
		cmd->args[0].v.i=0;

		if(mp_input_queue_cmd(cmd) == 0)
		{
			mp_cmd_free(cmd);
			res = "cmd queue full";
		}

		ret = RC_OK;
		res = "OK";
	}
	else
	{
		res = "not enough memory";
	}

	strncpy (RESULT, res, RESULT_LEN);
	return ret;
}

static int QUIT(char * line)
{
	int ret = RC_ERROR;
	char * res = "";

	mplayer_quit = 1;

	if(mplayer_starting)
	{
		ret = RC_OK;
		res = "OK";
	}
	else
	{
		mp_cmd_t * cmd = malloc(sizeof(mp_cmd_t));

		if(cmd)
		{
			cmd->pausing = 0;
			cmd->id    = MP_CMD_QUIT;
			cmd->name  = strdup("quit");
			cmd->nargs = 0;
			cmd->args[0].type = -1;
			cmd->args[0].v.i=0;

			if(mp_input_queue_cmd(cmd) == 0)
			{
				mp_cmd_free(cmd);
				res = "cmd queue full";
			}

			ret = RC_OK;
			res = "OK";
		}
		else
		{
			res = "not enough memory";
		}
	}

	strncpy (RESULT, res, RESULT_LEN);
	return ret;
}

static int VOLUME(char * line)
{
	int ret = RC_ERROR;
	char * res = "";

	mp_cmd_t * cmd = malloc(sizeof(mp_cmd_t));

	if(cmd)
	{
		int abs;
		int value;
		if(matchstr (&line, "ABS"))  // absolute
		{
			abs = 1;
		}
		else  // relative
		{
			abs = 0;
		}

		value = matchnum(&line);

		cmd->pausing = 0;
		cmd->id    = MP_CMD_VOLUME;
		cmd->name  = strdup("volume");
		cmd->nargs = 2;
		cmd->args[0].type = MP_CMD_ARG_FLOAT;
		cmd->args[0].v.f=(float) value;
		cmd->args[1].type = MP_CMD_ARG_INT;
		cmd->args[1].v.i=abs;
		cmd->args[2].type = -1;
		cmd->args[2].v.i=0;

		if(mp_input_queue_cmd(cmd) == 0)
		{
			mp_cmd_free(cmd);
			res = "cmd queue full";
		}

		ret = RC_OK;
		res = "OK";
	}
	else
	{
		res = "not enough memory";
	}

	strncpy (RESULT, res, RESULT_LEN);
	return ret;
}

static int MUTE(char * line)
{
	int ret = RC_ERROR;
	char * res = "";

	mp_cmd_t * cmd = malloc(sizeof(mp_cmd_t));

	if(cmd)
	{
		cmd->pausing = 0;
		cmd->id    = MP_CMD_MUTE;
		cmd->name  = strdup("mute");
		cmd->nargs = 0;
		cmd->args[0].type = -1;
		cmd->args[0].v.i=0;

		if(mp_input_queue_cmd(cmd) == 0)
		{
			mp_cmd_free(cmd);
			res = "cmd queue full";
		}

		ret = RC_OK;
		res = "OK";
	}
	else
	{
		res = "not enough memory";
	}

	strncpy (RESULT, res, RESULT_LEN);
	return ret;
}

static int LOADFILE(char * line)
{
	int ret = RC_ERROR;
	char * res = "";


	if(rexx_filename)
	{
		free(rexx_filename);
		rexx_filename = NULL;
	}

	rexx_filename = strdup(line);

	if(mplayer_starting)
	{
		mplayer_starting = 0;
		ret = RC_OK;
	}
	else
	{
		mp_cmd_t * cmd = malloc(sizeof(mp_cmd_t));

		if(cmd)
		{
			cmd->pausing = 0;
			cmd->id    = MP_CMD_LOADFILE;
			cmd->name  = strdup("loadfile");
			cmd->nargs = 1;
			cmd->args[0].type = MP_CMD_ARG_STRING;
			cmd->args[0].v.s=strdup(line);
			cmd->args[1].type = -1;
			cmd->args[1].v.i=0;


			if(mp_input_queue_cmd(cmd) == 0)
			{
				mp_cmd_free(cmd);
				res = "cmd queue full";
			}

			ret = RC_OK;
			res = "OK";
		}
		else
		{
			res = "not enough memory";
		}
	}

	strncpy (RESULT, res, RESULT_LEN);
	return ret;
}

static int LOADLIST(char * line)
{
	int ret = RC_ERROR;
	char * res = "";

	mp_cmd_t * cmd = malloc(sizeof(mp_cmd_t));

	if(cmd)
	{
		cmd->pausing = 0;
		cmd->id    = MP_CMD_LOADLIST;
		cmd->name  = strdup("loadlist");
		cmd->nargs = 1;
		cmd->args[0].type = MP_CMD_ARG_STRING;
		cmd->args[0].v.s=strdup(line);
		cmd->args[1].type = -1;
		cmd->args[1].v.i=0;

		if(mp_input_queue_cmd(cmd) == 0)
		{
			mp_cmd_free(cmd);
			res = "cmd queue full";
		}

		ret = RC_OK;
		res = "OK";
	}
	else
	{
		res = "not enough memory";
	}

	strncpy (RESULT, res, RESULT_LEN);
	return ret;
}

static int FULLSCREEN(char * line)
{
	int ret = RC_ERROR;
	char * res = "";

	mp_cmd_t * cmd = malloc(sizeof(mp_cmd_t));

	if(cmd)
	{
		cmd->pausing = 0;
		cmd->id    = MP_CMD_VO_FULLSCREEN;
		cmd->name  = strdup("vo_fullscreen");
		cmd->nargs = 0;
		cmd->args[0].type = -1;
		cmd->args[0].v.i=0;

		if(mp_input_queue_cmd(cmd) == 0)
		{
			mp_cmd_free(cmd);
			res = "cmd queue full";
		}

		ret = RC_OK;
		res = "OK";
	}
	else
	{
		res = "not enough memory";
	}

	strncpy (RESULT, res, RESULT_LEN);
	return ret;
}

static int FRAMEDROP(char * line)
{
	int ret = RC_ERROR;
	char * res = "";

	mp_cmd_t * cmd = malloc(sizeof(mp_cmd_t));

	if(cmd)
	{
		int value;
		value = matchnum(&line);

		cmd->pausing = 0;
		cmd->id    = MP_CMD_FRAMEDROPPING;
		cmd->name  = strdup("frame_drop");
		cmd->nargs = 2;
		cmd->args[0].type = MP_CMD_ARG_INT;
		cmd->args[0].v.i=value;
		cmd->args[1].type = -1;
		cmd->args[1].v.i=0;

		if(mp_input_queue_cmd(cmd) == 0)
		{
			mp_cmd_free(cmd);
			res = "cmd queue full";
		}

		ret = RC_OK;
		res = "OK";
	}
	else
	{
		res = "not enough memory";
	}

	strncpy (RESULT, res, RESULT_LEN);
	return ret;
}

static int SUB_VISIBILITY(char * line)
{
	int ret = RC_ERROR;
	char * res = "";

	mp_cmd_t * cmd = malloc(sizeof(mp_cmd_t));

	if(cmd)
	{
		cmd->pausing = 0;
		cmd->id    = MP_CMD_SUB_VISIBILITY;
		cmd->name  = strdup("sub_visibility");
		cmd->nargs = 0;
		cmd->args[0].type = -1;
		cmd->args[0].v.i=0;

		if(mp_input_queue_cmd(cmd) == 0)
		{
			mp_cmd_free(cmd);
			res = "cmd queue full";
		}

		ret = RC_OK;
		res = "OK";
	}
	else
	{
		res = "not enough memory";
	}

	strncpy (RESULT, res, RESULT_LEN);
	return ret;
}

static int OSD_TEXT(char * line)
{
	int ret = RC_ERROR;
	char * res = "";

	mp_cmd_t * cmd = malloc(sizeof(mp_cmd_t));

	if(cmd)
	{
		cmd->pausing = 0;
		cmd->id    = MP_CMD_OSD_SHOW_TEXT;
		cmd->name  = strdup("osd_show_text");
		cmd->nargs = 1;
		cmd->args[0].type = MP_CMD_ARG_STRING;
		cmd->args[0].v.s= strdup(line);
		cmd->args[1].type = -1;
		cmd->args[1].v.i=-1;
		cmd->args[2].type = -1;
		cmd->args[2].v.i=0;
		cmd->args[3].type = -1;
		cmd->args[3].v.i=0;

		if(mp_input_queue_cmd(cmd) == 0)
		{
			mp_cmd_free(cmd);
			res = "cmd queue full";
		}

		ret = RC_OK;
		res = "OK";
	}
	else
	{
		res = "not enough memory";
	}

	strncpy (RESULT, res, RESULT_LEN);
	return ret;
}

static int RAW(char * line)
{
	int ret = RC_ERROR;
	char * res = "";

	mp_cmd_t * cmd = mp_input_parse_cmd(line);

	if(cmd)
	{
		if(mp_input_queue_cmd(cmd) == 0)
		{
			mp_cmd_free(cmd);
			res = "cmd queue full";
		}

		ret = RC_OK;
		res = "OK";
	}
	else
	{
		res = "not enough memory";
	}

	strncpy (RESULT, res, RESULT_LEN);
	return ret;
}

static int REGISTERPORT(char * line)
{
	int ret = RC_OK;
	char * res = "OK";

	strncpy(REGISTERED_PORTNAME, line, PORT_LEN);
	strncpy (RESULT, res, RESULT_LEN);
	return ret;
}

static int HELP(char * line)
{
	int ret = RC_OK;
	char * res = "Commands :\nLOADFILE <filename>\nLOADLIST <playlist>\n"
	"PAUSE\nQUIT\nSEEK [ABS_SECS|ABS_PERCENT] <value>\n"
	"VOLUME [ABS] <value>\nMUTE\nFULLSCREEN\nFRAMEDROP <value>\n"
	"SUB_VISIBILITY\nOSD_TEXT <text>\nRAW <raw command>\n\n"
	"QUERY\n\tFILENAME\n\tLENGTH\n\tPOSITION [PERCENT]\n\tVOLUME [PERCENT]\n\tFULLSCREEN\n\tSUB_VISIBILITY\n\tFRAMEDROP\n\tINFO [VIDEO|AUDIO|CLIPINFO|FPS]\n";

	strncpy (RESULT, res, RESULT_LEN);
	return ret;
}

extern uint32_t is_fullscreen;

static int QUERY (char *line)
{
	char *res = NULL;

	if (matchstr (&line, "FILENAME"))
	{
		snprintf(RESULT, RESULT_LEN, "%s", filename);
	}
	else if (matchstr (&line, "LENGTH"))
	{
		unsigned long len = demuxer_get_time_length(mpctx->demuxer);
		snprintf(RESULT, RESULT_LEN, "%lu", len);
	}
	else if (matchstr (&line, "POSITION"))
	{
		if(matchstr (&line, "PERCENT"))
		{
			int percentage = -1;
			percentage = demuxer_get_percent_pos(mpctx->demuxer);
			snprintf(RESULT, RESULT_LEN, "%d", percentage);
		}
		else
		{
			if(mpctx->sh_video)
			{
				int pts=mpctx->sh_video->pts;
				snprintf(RESULT, RESULT_LEN, "%ld", (long int) pts);
			}
		}
	}
	else if(matchstr (&line, "FULLSCREEN"))
	{
		if(is_fullscreen)
			res = "1";
		else
			res = "0";
	}
	else if (matchstr (&line, "INFO"))
	{
		if(matchstr (&line, "VIDEO"))
		{
			if(mpctx->sh_video)
			{
				if (mpctx->sh_video->ds->demuxer->file_format == DEMUXER_TYPE_MPEG_PS
				 || mpctx->sh_video->ds->demuxer->file_format == DEMUXER_TYPE_MPEG_ES
				 || mpctx->sh_video->ds->demuxer->file_format == DEMUXER_TYPE_PVA )
				{
					snprintf(RESULT, RESULT_LEN, "[%s] %dx%d, %4.2f fps, %5.1f kbps",
					"MPEG",
					mpctx->sh_video->disp_w,
					mpctx->sh_video->disp_h,
					mpctx->sh_video->fps,
					mpctx->sh_video->i_bps*0.008f);
				}
				else
				{
					snprintf(RESULT, RESULT_LEN, "[%s] %dx%d, %d bit, %4.2f fps, %5.1f kbps",
					(char *)&mpctx->sh_video->bih->biCompression,
					mpctx->sh_video->bih->biWidth,
					mpctx->sh_video->bih->biHeight,
					mpctx->sh_video->bih->biBitCount,
					mpctx->sh_video->fps,
					mpctx->sh_video->i_bps*0.008f);
				}
			}
		}
		else if(matchstr (&line, "AUDIO"))
		{
			if(mpctx->sh_audio)
			{
				snprintf(RESULT, RESULT_LEN, "[%s] %d Hz, %d ch, %d bit, %3.1f kbps",
				/*mpctx->sh_audio->codec?mpctx->sh_audio->codec->name:*/"N/A",
				mpctx->sh_audio->samplerate,
				mpctx->sh_audio->channels,
				mpctx->sh_audio->samplesize*8,
				mpctx->sh_audio->i_bps*8*0.001);
			}

		}
		else if(matchstr (&line, "FPS"))
		{
			if(mpctx->sh_video)
				snprintf(RESULT, RESULT_LEN, "%f", mpctx->sh_video->fps);
		}
		else if(matchstr (&line, "CLIPINFO"))
		{
		    char **info = mpctx->demuxer->info;
			int n;

			if(info)
			{
				char buf[RESULT_LEN];
				snprintf(RESULT, RESULT_LEN, "CLIPINFO:\n");
				for(n = 0; info[2*n] != NULL ; n++)
				{
					snprintf(buf, RESULT_LEN, "%s: %s\n",info[2*n],info[2*n+1]);
					strncat(RESULT, buf, RESULT_LEN);
				}
			}
		}
	}
	else if (matchstr (&line, "VOLUME"))
	{
		if(matchstr (&line, "PERCENT"))
		{
			float vol;
			mixer_getbothvolume(&mpctx->mixer,&vol);
			snprintf(RESULT, RESULT_LEN, "%f", vol);
		}
		else
		{
			float vol;
			mixer_getbothvolume(&mpctx->mixer,&vol);
			snprintf(RESULT, RESULT_LEN, "%f", (vol*256.0)/100.0);
		}
	}
	else if (matchstr (&line, "GAMMA"))
	{
		snprintf(RESULT, RESULT_LEN, "%d", vo_gamma_gamma);
	}
	else if (matchstr (&line, "BRIGHTNESS"))
	{
		snprintf(RESULT, RESULT_LEN, "%d", vo_gamma_brightness);
	}
	else if (matchstr (&line, "CONTRAST"))
	{
		snprintf(RESULT, RESULT_LEN, "%d", vo_gamma_contrast);
	}
	else if (matchstr (&line, "SATURATION"))
	{
		snprintf(RESULT, RESULT_LEN, "%d", vo_gamma_saturation);
	}
	else if (matchstr (&line, "HUE"))
	{
		snprintf(RESULT, RESULT_LEN, "%d", vo_gamma_hue);
	}
	else if (matchstr (&line, "FRAMEDROP"))
	{
		snprintf(RESULT, RESULT_LEN, "%d", frame_dropping);
	}
	else if (matchstr (&line, "SUB_VISIBILITY"))
	{
		snprintf(RESULT, RESULT_LEN, "%d", sub_visibility);
	}
	else
		return RC_ERROR;

	if (res)
	{
		strncpy (RESULT, res, RESULT_LEN);
	}

	return RC_OK;
}


static int process_cmd (char *line)
{
	RESULT[0] = '\0';
	if      (matchstr (&line, "QUERY"))
		return QUERY (line);
	else if (matchstr (&line, "LOADFILE"))
		return LOADFILE (line);
	else if (matchstr (&line, "LOADLIST"))
		return LOADLIST (line);
	else if (matchstr (&line, "PAUSE"))
		return PAUSE (line);
	else if (matchstr (&line, "QUIT"))
		return QUIT (line);
	else if (matchstr (&line, "SEEK"))
		return SEEK (line);
	else if (matchstr (&line, "VOLUME"))
		return VOLUME (line);
	else if (matchstr (&line, "MUTE"))
		return MUTE (line);
	else if (matchstr (&line, "FULLSCREEN"))
		return FULLSCREEN (line);
	else if (matchstr (&line, "FRAMEDROP"))
		return FRAMEDROP (line);
	else if (matchstr (&line, "SUB_VISIBILITY"))
		return SUB_VISIBILITY (line);
	else if (matchstr (&line, "OSD_TEXT"))
		return OSD_TEXT (line);
	else if (matchstr (&line, "RAW"))
		return RAW (line);
	else if (matchstr (&line, "REGISTER"))
		return REGISTERPORT (line);
	else if (matchstr (&line, "HELP"))
		return HELP (line);
	else
		return RC_ERROR;

	return RC_OK;
}

struct
{
	int	position;
	float volume;
	int fullscreen;
	int sub_visibility;
	int avdelay;
	int paused;
	int framedrop;
	int mute;
} mplayer_state = { 0, 0, 0, 0, 0, 0, 0, 0} ;

void rexx_handle_events (void)
{
	struct RexxMsg *msg;

	while ((msg = (struct RexxMsg*)GetMsg (ARexxPort)))
	{
		//mp_msg(MSGT_CPLAYER, MSGL_INFO, "Receiving arexx message\n");
		if (!(msg->rm_Action & RXCOMM))
		{
			//mp_msg(MSGT_CPLAYER, MSGL_INFO, "Unknown action '%08X' received!\n", msg->rm_Action);
		    continue;
		}
		msg->rm_Result1 = process_cmd (msg->rm_Args[0]);
		msg->rm_Result2 = 0;

		if (msg->rm_Action & RXFF_RESULT)
		{
		    int len = strlen (RESULT);
		    msg->rm_Result2 = (LONG)CreateArgstring (RESULT,len);
		}
		ReplyMsg ((void*)msg);
	}
}

/* END REXX PORT */

#endif
/**********************************************************************************************************************/

#ifdef __AROS__
#define BUILD_DATE "27.04.2023"
#define VERSION ""
const char version[] = "$VER: MPlayer 1.2 ("BUILD_DATE") © MPlayer Team";
const char muiversion[] = "$VER: MPlayer 1.2 ("BUILD_DATE")";
const char muititle[] = "MPlayer 1.2 ("BUILD_DATE") "VERSION;
const char revision[] = VERSION;
#else
const char version[]    = "$VER: MPlayer 1.0 (25.09.2011) © MPlayer Team, Nicolas Det, Fabien Coeurjoly [SVN: r34123]";
const char muiversion[] = "$VER: MPlayer 1.0 (25.09.2011)";
const char muititle[]   = "MPlayer 1.0 (25.09.2011) [SVN: r34123]";
const char revision[]   = "r34123";
#endif

struct Task * maintask;
unsigned long __stack = 524288*4; /* Just to be on the safe side! */

// some variables for timer.device
// the timer.device can be compared to the RTC
// Yes, global variables ! This way we can init it in mplayer once
// and then use it in user_sleep
// Not very clean but works great !
#ifdef __AROS__
struct Device * TimerBase = NULL;
#else
struct Library *  TimerBase=NULL;
#endif

// And library
#ifndef __AROS__
struct Library       * AsyncIOBase = NULL;
#endif
struct Library       * AslBase     = NULL;
struct IntuitionBase * IntuitionBase = NULL;
struct Library       * UtilityBase = NULL;
struct Library       * WorkbenchBase = NULL;
#ifndef __AROS__
extern struct Library * SocketBase;
#endif
struct Library *ffmpegSocketBase = NULL; // XXX: check that

UBYTE  TimerDevice = -1; // -1 -> not opened
struct timerequest * TimerRequest=NULL;
struct MsgPort *     TimerMsgPort=NULL;

ULONG   AHI_AudioID;

static char **MorphOS_argv = NULL;
static int MorphOS_argc = 0;

static ULONG NameIsInfo(CONST_STRPTR name)
{
	ULONG len;

	/*  filters out filenames like ".info", it is not an icon file!
	 */

	len = strlen(name);

	if (len >= 6)
	{
		UBYTE c;

		name += len - 6;
		c    =  name[0];

		if (((c = name[0]) != ':') && (c != '/') &&
		    (name[1] == '.') &&
		    (((c = name[2]) == 'i') || (c == 'I')) &&
		    (((c = name[3]) == 'n') || (c == 'N')) &&
		    (((c = name[4]) == 'f') || (c == 'F')) &&
		    (((c = name[5]) == 'o') || (c == 'O'))
		)
		{
			return (TRUE);
		}
	}
	return (FALSE);
}


#define GET_PATH(drawer,file,dest)                                                      \
	dest = (char *) malloc( ( strlen(drawer) + strlen(file) + 2 ) * sizeof(char) );     \
	if (dest)																			\
	{                                                                         			\
		if ( strlen(drawer) == 0) strcpy(dest, file);									\
		else																			\
		{	                                                                            \
			strcpy(dest, drawer);                                                       \
			AddPart(dest, file, ( strlen(drawer) + strlen(file) + 2 ));                 \
		}																			    \
	}																				

/******************************/
static VOID Free_Arg(VOID)
{
	if (MorphOS_argv)
	{
		ULONG i;
		for (i=0; i < MorphOS_argc; i++) if(MorphOS_argv[i]) free(MorphOS_argv[i]);
		free(MorphOS_argv);
	}
}

/******************************/
void MorphOS_ParseArg(int argc, char *argv[], int *new_argc, char ***new_argv)
{
	struct WBStartup *WBStartup = NULL;

	*new_argc = argc;
	*new_argv = argv;

	// Ok is launch from cmd line, just do nothing
	if (argc) return;

	// Ok, ran from WB, we have to handle some funny thing now
	// 1. if there is some WBStartup->Arg then play that file and go
	// 2. else open an ASL requester and add the selected file

	// 1. WBStartup
	WBStartup = (struct WBStartup *) argv;
	if (!WBStartup) return ; // We never know !

#if MPLAYER
#ifdef CONFIG_GUI
	if ( use_gui )
	{
		if(WBStartup->sm_NumArgs <= 1)
        {
            MorphOS_argc = 1;
            MorphOS_argv = malloc(MorphOS_argc* sizeof(char *) );
            if (!MorphOS_argv) goto fail;

            /* application name */
            MorphOS_argv[0] = malloc(strlen(WBStartup->sm_ArgList[0].wa_Name) + 1);
            if (!MorphOS_argv[0]) goto fail;
            strcpy(MorphOS_argv[0], WBStartup->sm_ArgList[0].wa_Name);
            goto ok;
        }
	}
#endif
#endif

	if (WBStartup->sm_NumArgs > 1)
	{
		// The first arg is always the tool name (aka us)
		// Then if more than one arg, with have some file name
		ULONG i, j = 0;
		ULONG foundProjectIcon = FALSE;

		// We will replace the original argc/argv by us
		MorphOS_argc = WBStartup->sm_NumArgs;
		MorphOS_argv = malloc(MorphOS_argc* sizeof(char *) );
		if (!MorphOS_argv) goto fail;

		memset(MorphOS_argv, 0x00, MorphOS_argc * sizeof(char *) );
#ifdef __AROS__
        /* First - application name */
        MorphOS_argv[0] = malloc(strlen(WBStartup->sm_ArgList[0].wa_Name) + 1);
        if (!MorphOS_argv[0]) goto fail;
        strcpy(MorphOS_argv[0], WBStartup->sm_ArgList[0].wa_Name);

        /* Second - clicked file name */
        MorphOS_argv[1] = malloc(1024);
        memset(MorphOS_argv[1], 0x00, 1024);
        NameFromLock(WBStartup->sm_ArgList[1].wa_Lock, MorphOS_argv[1], 1024);
        AddPart(MorphOS_argv[1], WBStartup->sm_ArgList[1].wa_Name, 1024);
#else
		for(i = 0; i < MorphOS_argc; i++)
		{
			if(i == 1)
			{
				if(NameIsInfo(WBStartup->sm_ArgList[i].wa_Name) || !strcmp("GMPlayer", WBStartup->sm_ArgList[i].wa_Name))
				{
					foundProjectIcon = TRUE;
					continue;
				}
			}

			MorphOS_argv[j] = malloc(strlen(WBStartup->sm_ArgList[i].wa_Name) + 1);
			if (!MorphOS_argv[j])
			{
				goto fail;
			}
			strcpy(MorphOS_argv[j], WBStartup->sm_ArgList[i].wa_Name);

			j++;
		}

		if(foundProjectIcon)
		{
			MorphOS_argc--;
		}
#endif
	}
	else
	{
		// Once upon a time, an ASL requester
		ULONG i=0;
		struct FileRequester * MorphOS_FileRequester = NULL;
		char * lastvisitedpath = NULL;

#if MPLAYER
		MorphOS_RestorePath();
#endif
		lastvisitedpath = MorphOS_GetLastVisitedPath();

		MorphOS_FileRequester = AllocAslRequest(ASL_FileRequest, NULL);
		if (!MorphOS_FileRequester) 
		{
		  goto fail;
		}        

		if ( ( AslRequestTags( MorphOS_FileRequester,
						ASLFR_TitleText,        "Select File...",
						ASLFR_DoMultiSelect,    TRUE, 
						ASLFR_DoPatterns,       TRUE,
						ASLFR_RejectIcons,      TRUE,
						ASLFR_InitialDrawer,    lastvisitedpath ?: "",
                        ASLFR_InitialPattern,   "#?.(mp#?|dat|st?|ts|#?xa|??v|vob|sfd|avi|divx|asf|qt|ra#?|rv|rm#?|og?|cin|roq|fl#?)",
						TAG_DONE) ) == FALSE )
		{
			FreeAslRequest(MorphOS_FileRequester);
			MorphOS_FileRequester = NULL;
			goto fail;
		}

		MorphOS_argc = MorphOS_FileRequester->fr_NumArgs + 1;
		MorphOS_argv = malloc(MorphOS_argc * sizeof(char *) );
		if (!MorphOS_argv) goto fail;
	
		memset(MorphOS_argv, 0x00, MorphOS_argc * sizeof(char *) );

		MorphOS_argv[0] = strdup("mplayer");
		if (!MorphOS_argv[0]) goto fail;

		MorphOS_SetLastVisitedPath(MorphOS_FileRequester->fr_Drawer);

		for( i=1; i < MorphOS_argc; i++) 
		{
			GET_PATH(MorphOS_FileRequester->fr_Drawer, 
						MorphOS_FileRequester->fr_ArgList[i-1].wa_Name, 
						MorphOS_argv[i]);
			if (!MorphOS_argv[i]) 
			{
				FreeAslRequest(MorphOS_FileRequester);
				MorphOS_FileRequester = NULL;
				goto fail;
			}
		}

		FreeAslRequest(MorphOS_FileRequester);
	}

ok:
    *new_argc = MorphOS_argc;
    *new_argv = MorphOS_argv;

	return;

fail:

	Free_Arg();

	*new_argc = argc;
	*new_argv = argv;
	return;
}


/******************************/

void MorphOS_Close(void)
{
#if MPLAYER
	if(!use_gui)
	{
		MorphOS_SavePath();
	}
	
	gui_exit();
#endif

	Free_Arg();

#ifndef __AROS__
	if (SocketBase)    CloseLibrary( SocketBase );
	if (AsyncIOBase)   CloseLibrary( AsyncIOBase );
#endif
	if (AslBase)       CloseLibrary(AslBase);
	if (!TimerDevice)  CloseDevice( (struct IORequest *) TimerRequest);
	if (TimerRequest)  DeleteIORequest ( (struct IORequest *) TimerRequest );
	if (TimerMsgPort)  DeleteMsgPort ( TimerMsgPort );
	if (WorkbenchBase) CloseLibrary(WorkbenchBase);
	if (IntuitionBase) CloseLibrary((struct Library *) IntuitionBase);
	if (UtilityBase)   CloseLibrary(UtilityBase);
}

/******************************/

int MorphOS_Open(int argc, char *argv[])
{
	MorphOS_argv = NULL;

    maintask = FindTask(NULL);

#if !defined(__AROS__)
	// register Fontconfig progress callback
	fontconfig_progress_callback = myfontconfig_progress_callback;
#endif

	setlocale(LC_NUMERIC, "C");

	if (!argc) 
	{ 
		// If argc == 0 -> execute from the WB, then no need to display anything
		freopen("NIL:", "w", stderr);
		freopen("NIL:", "w", stdout);
	}

	if ( ! (TimerMsgPort=CreateMsgPort()) )
	{
	   	mp_msg(MSGT_CPLAYER, MSGL_FATAL, "Failed to create MsgPort\n");
	   	MorphOS_Close();
		return -1;
	}

	if ( ! ( TimerRequest = CreateIORequest( TimerMsgPort, sizeof(struct timerequest) ) ) )
	{
	   	mp_msg(MSGT_CPLAYER, MSGL_FATAL, "Failed to create IORequest\n");
	   	MorphOS_Close();
		return -1;
 	}

	if ( (TimerDevice = OpenDevice(TIMERNAME, UNIT_MICROHZ , (struct IORequest *) TimerRequest, NULL) ) )
	{
	   	mp_msg(MSGT_CPLAYER, MSGL_FATAL, "Failed to open" TIMERNAME ".\n");
	   	MorphOS_Close();
		return -1;
 	}

	TimerBase = (struct Library *) TimerRequest->tr_node.io_Device;

#ifndef __AROS__
	if ( ! ( AsyncIOBase = OpenLibrary( "asyncio.library", 0L) ) ) 
	{
		mp_msg(MSGT_CPLAYER, MSGL_FATAL, "Unable to open asyncio.library\n");
	   	MorphOS_Close();
		return -1;
	}
#endif

#ifdef __AROS__
    if (!(UtilityBase = OpenLibrary("utility.library", 0L)))
#else
    if (!(UtilityBase = OpenLibrary("utility.library", 50)))
#endif
	{
		mp_msg(MSGT_CPLAYER, MSGL_FATAL, "Unable to open utility.library\n");
	   	MorphOS_Close();
		return -1;
	}

#ifdef __AROS__
    if (!(IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 0L)))
#else
    if (!(IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 50)))
#endif
	{
		mp_msg(MSGT_CPLAYER, MSGL_FATAL, "Unable to open intuition.library\n");
	   	MorphOS_Close();
		return -1;
	}

	if ( ! ( AslBase = OpenLibrary( "asl.library", 0L) ) ) 
	{
		mp_msg(MSGT_CPLAYER, MSGL_FATAL, "Unable to open asl.library\n");
	   	MorphOS_Close();
		return -1;
	}

	if ( ! (WorkbenchBase = OpenLibrary("workbench.library", 37)) )
	{
		mp_msg(MSGT_CPLAYER, MSGL_FATAL, "Unable to open asl.library\n");
	   	MorphOS_Close();
		return -1;
	}

#if MPLAYER
	gui_init();
	stccpy(pattern, default_pattern, sizeof(pattern));
#endif

	return 0;
}

char * MorphOS_GetScreenTitle(void)
{
#if MPLAYER
	return MPLAYER_PORTNAME;
#else
	return "MPlayer Screen";
#endif
}

char * MorphOS_GetWindowTitle(void)
{
#if MPLAYER
	static int firsttime = TRUE;
	if(mpctx && mpctx->stream && (mpctx->stream->type == STREAMTYPE_DVD || mpctx->stream->type == STREAMTYPE_DVDNAV))
	{
		if(firsttime)
		{
			firsttime = FALSE;

			dvd_read_name(windowtitle, NULL, dvd_device);

			if(windowtitle[0] == '\0')
			{
				stccpy(windowtitle, dvd_device, sizeof(windowtitle));
			}
		}
	}
	else
	{
		if(filename)
		{
			stccpy(windowtitle, filename, sizeof(windowtitle));
		}
		else
		{
#ifdef __AROS__
            stccpy(windowtitle, "MPlayer for AROS i386", sizeof(windowtitle));
#else
            stccpy(windowtitle, "MPlayer for MorphOS", sizeof(windowtitle));
#endif
		}
	}
	return windowtitle;
#else /* MPLAYER */
#ifdef __AROS__
    return "MPlayer for AROS x86";
#else
    return "MPlayer for MorphOS";
#endif
#endif
}

#if MPLAYER
void MorphOS_RestorePath(void)
{
	FILE *f = fopen("PROGDIR:conf/lastvisitedpath", "r");
	if(f)
	{
		if(fgets(last_visited_path, sizeof(last_visited_path), f))
		{
			char *ptr = strchr(last_visited_path, '\n');
			if(ptr) *ptr = 0;
		}
		fclose(f);
	}

	f = fopen("PROGDIR:conf/pattern", "r");
	if(f)
	{
		if(fgets(pattern, sizeof(pattern), f))
		{
			char *ptr = strchr(pattern, '\n');
			if(ptr) *ptr = 0;
		}
		fclose(f);
	}
}

void MorphOS_SavePath(void)
{
	FILE *f;

	if(last_visited_path[0])
	{
		f = fopen("PROGDIR:conf/lastvisitedpath", "w");
		if(f)
		{
			fprintf(f, "%s\n", last_visited_path);
			fclose(f);
		}
	}

	if(pattern[0] == 0)
	{
		stccpy(pattern, default_pattern, sizeof(pattern));
	}

	f = fopen("PROGDIR:conf/pattern", "w");
	if(f)
	{
		fprintf(f, "%s\n", pattern);
		fclose(f);
	}
}

#endif

char * MorphOS_GetLastVisitedPath(void)
{
	if(last_visited_path[0])
	{
		return last_visited_path;
	}
	else
	{
		return NULL;
	}
}

void MorphOS_SetLastVisitedPath(char *path)
{
	stccpy(last_visited_path, path, sizeof(last_visited_path));
}

char * MorphOS_GetPattern(void)
{
	if(pattern[0])
	{
		return pattern;
	}
	else
	{
		return NULL;
	}
}

void MorphOS_SetPattern(char *p)
{
	stccpy(pattern, p, sizeof(pattern));
}

/*****************************************************************************************/

void reverse_fft(float X[], int N)
{
	int I,I0,I1,I2,I3,I4,I5,I6,I7,I8, IS,ID;
	int J,K,M,N2,N4,N8;
	float A,A3,CC1,SS1,CC3,SS3,E,R1,XT;
	float T1,T2,T3,T4,T5,T6;

	M=(int)(log(N)/log(2.0));               /* N=2^M */

/* ----Digit reverse counter--------------------------------------------- */
	J = 1;
	for(I=1;I<N;I++)
	{
		if (I<J)
		{
			XT    = X[J];
			X[J]  = X[I];
			X[I]  = XT;
		}
		K = N/2;
		while(K<J)
		{
			J -= K;
			K /= 2;
		}
		J += K;
	}

/* ----Length two butterflies--------------------------------------------- */
	IS = 1;
	ID = 4;
	do
	{
		for(I0 = IS;I0<=N;I0+=ID)
		{
			I1    = I0 + 1;
			R1    = X[I0];
			X[I0] = R1 + X[I1];
			X[I1] = R1 - X[I1];
		}
		IS = 2 * ID - 1;
		ID = 4 * ID;
	}while(IS<N);

/* ----L shaped butterflies----------------------------------------------- */
	N2 = 2;
	for(K=2;K<=M;K++)
	{
		N2    = N2 * 2;
		N4    = N2/4;
		N8    = N2/8;
		E     = (float) 6.2831853071719586f/N2;
		IS    = 0;
		ID    = N2 * 2;
		do
		{
			for(I=IS;I<N;I+=ID)
			{
				I1 = I + 1;
				I2 = I1 + N4;
				I3 = I2 + N4;
				I4 = I3 + N4;
				T1 = X[I4] +X[I3];
				X[I4] = X[I4] - X[I3];
				X[I3] = X[I1] - T1;
				X[I1] = X[I1] + T1;
				if(N4!=1)
				{
					I1 += N8;
					I2 += N8;
					I3 += N8;
					I4 += N8;
					T1 = (X[I3] + X[I4])*.7071067811865475244f;
					T2 = (X[I3] - X[I4])*.7071067811865475244f;
					X[I4] = X[I2] - T1;
					X[I3] = -X[I2] - T1;
					X[I2] = X[I1] - T2;
					X[I1] = X[I1] + T2;
				}
			}
			IS = 2 * ID - N2;
			ID = 4 * ID;
		}while(IS<N);
		A = E;
		for(J= 2;J<=N8;J++)
		{
			A3 = 3.0 * A;
			CC1   = cos(A);
			SS1   = sin(A);  /*typo A3--really A?*/
			CC3   = cos(A3); /*typo 3--really A3?*/
			SS3   = sin(A3);
			A = (float)J * E;
			IS = 0;
			ID = 2 * N2;
			do
			{
				for(I=IS;I<N;I+=ID)
				{
					I1 = I + J;
					I2 = I1 + N4;
					I3 = I2 + N4;
					I4 = I3 + N4;
					I5 = I + N4 - J + 2;
					I6 = I5 + N4;
					I7 = I6 + N4;
					I8 = I7 + N4;
					T1 = X[I3] * CC1 + X[I7] * SS1;
					T2 = X[I7] * CC1 - X[I3] * SS1;
					T3 = X[I4] * CC3 + X[I8] * SS3;
					T4 = X[I8] * CC3 - X[I4] * SS3;
					T5 = T1 + T3;
					T6 = T2 + T4;
					T3 = T1 - T3;
					T4 = T2 - T4;
					T2 = X[I6] + T6;
					X[I3] = T6 - X[I6];
					X[I8] = T2;
					T2    = X[I2] - T3;
					X[I7] = -X[I2] - T3;
					X[I4] = T2;
					T1    = X[I1] + T5;
					X[I6] = X[I1] - T5;
					X[I1] = T1;
					T1    = X[I5] + T4;
					X[I5] = X[I5] - T4;
					X[I2] = T1;
				}
				IS = 2 * ID - N2;
				ID = 4 * ID;
			}while(IS<N);
		}
	}
}


/*****************************************************************************************/

/* Missing bits */

int vo_init(void)
{
	return 1;
}

void *memccpy (void *s, const void *s0, int c, size_t n)
{
	if (n != 0) {
		unsigned char *s1 = s;
		const unsigned char *s2 = s0;
		do {
			if ((*s1++ = *s2++) == (unsigned char)c)
				return (s1);
		} while (--n != 0);
	}
	return (NULL);
}

STRPTR stristr(CONST_STRPTR str1, CONST_STRPTR str2)
{
	ULONG len = strlen(str2);

	while (*str1)
	{
		if (! strnicmp(str1, str2, len))
			return (char *) str1;

		str1++;
	}

	return NULL;
}

#if defined(__MORPHOS__)
unsigned int sleep(unsigned int seconds)
{
	TimeDelay(0, seconds, 0);
	return 0;
}
#endif

double trunc(double x)
{
	return (int) x;
}

double round(double x)
{
    return (x > 0) ? floor(x + 0.5) : ceil(x - 0.5);
}

