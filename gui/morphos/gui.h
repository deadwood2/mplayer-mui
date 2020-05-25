#ifndef __GUI_H__
#define __GUI_H__

#include <exec/types.h>
#include <proto/utility.h>
#include "classes.h"
#include "m_config.h"
#include "playtree.h"
#include "playlist.h"
#include "gui/app.h"
#include "gui/util/list.h"
#include "gui/util/string.h"
#include "mui.h"

#define THREAD_NAME             "MPlayer GUI Thread"
#define APPLICATION_DESCRIPTION	"Versatile audio and video media player."
#define APPLICATION_BASE		"MPLAYER_GUI"
#define APPLICATION_ICON        "PROGDIR:MPlayer"

#define PATTERN_STREAM   "#?.(mp#?|m4a|aac|dat|st?|ts|#?xa|??v|vob|sfd|avi|divx|asf|qt|ra(~r)|rv|rm#?|og?|cin|roq|fl#?)"
#define PATTERN_PLAYLIST "#?.(pls|m3u|m3u8)"
#define PATTERN_SUBTITLE "#?.(srt|ssa|ass|sub|txt|vob|utf|utf8|utf-8|smi|rt|aqt|jss|js)"

#if defined(__AROS__)
IPTR DoSuperNew(Class *cl, Object *obj, Tag tag1, ...);
APTR AllocVecTaskPooled(ULONG byteSize);
VOID FreeVecTaskPooled(APTR memory);
#undef NewObject
#include <proto/intuition.h>
#define MUIM_Menustrip_Popup                        (MUIB_MUI|0x00420e76) /* MUI: V20 */
#define MUIM_Menustrip_ExitChange                   (MUIB_MUI|0x0042ce4d) /* MUI: V20 */
#define MUIM_Menustrip_InitChange                   (MUIB_MUI|0x0042dcd9) /* MUI: V20 */
#define MUIA_List_DoubleClick                       (MUIB_MUI|0x00424635) /* MUI: V4  i.g BOOL       */
#define MUIA_List_DragType                          (MUIB_MUI|0x00425cd3) /* MUI: V11 isg LONG       */
#define MUIA_List_TitleClick                        (MUIB_MUI|0x00422fd9) /* MUI: V20 ..g LONG       */
#define MUIV_PushMethod_Delay(millis)               MIN(0x0ffffff0, (((ULONG)millis) << 8))
#define NM_WHEEL_UP                                 0x7a
#define NM_WHEEL_DOWN                               0x7b
#define MUIV_Frame_Window                           MUIV_Frame_None
#define MUIV_Frame_Page                             MUIV_Frame_None
#define MCC_TI_TAGBASE                              ((TAG_USER)|((1307<<16)+0x712))
#define MCC_TI_ID(x)                                (MCC_TI_TAGBASE+(x))
#define MUIA_Textinput_ResetMarkOnCursor            MCC_TI_ID(157)        /* V29 isg BOOL */
#endif

/* gtk emulation */
#define GTK_MB_FATAL         0x1
#define GTK_MB_ERROR         0x2
#define GTK_MB_WARNING       0x4
#define GTK_MB_SIMPLE        0x8

#define gfree free

/* Menu stuff */
enum {
/* Project */
MEN_ABOUT = 100,
MEN_FILE,
MEN_DIRECTORY,
MEN_PLAYLIST,
MEN_DVD,
MEN_DVD_DIR,
MEN_DVDNAV,
MEN_STREAM,
MEN_HIDE,		
MEN_QUIT,		

/* Options */
MEN_PLAYLISTWIN = 200,
MEN_PROPERTIES,
MEN_MESSAGES,
MEN_PREFERENCES,
MEN_MUI,		 
MEN_TOGGLEGUI,
MEN_SHOWSTATUS,
MEN_SHOWTOOLBAR,
MEN_SHOWCONTROL,
MEN_SAVESETTINGS,

/* Play */
MEN_PLAY = 300,
MEN_PAUSE,
MEN_STOP,
MEN_FRAMESTEP,
MEN_SEEKB10S,
MEN_SEEKF10S,
MEN_SEEKB1M,
MEN_SEEKF1M,
MEN_SEEKB10M,
MEN_SEEKF10M,
MEN_INCSPEED,
MEN_DECSPEED,
MEN_REPEAT,
MEN_REPEAT_OFF,
MEN_REPEAT_PLAYLIST,
MEN_REPEAT_SINGLE,
MEN_REPEAT_QUIT,
MEN_PREVIOUS,
MEN_NEXT,
MEN_RECORD,
MEN_DVDMENU,

/* Subtitles */
MEN_LOADSUBTITLE = 400,
MEN_CYCLESUBTITLE,
MEN_UNLOADSUBTITLE,
MEN_SELECTSUBTITLE,
MEN_SUBINCDELAY,
MEN_SUBDECDELAY,
MEN_ENABLEASS,
MEN_SUBTITLEBASE = 4000,

/* Audio */
MEN_AUDIOTRACK = 500,
MEN_MUTE,
MEN_INCVOLUME,
MEN_DECVOLUME,
MEN_AUDIOGAIN,
MEN_AUDIOINCDELAY,
MEN_AUDIODECDELAY,
MEN_AUDIOFILTER,
MEN_EXTRASTEREO,
MEN_VOLNORM,
MEN_KARAOKE,
MEN_SCALETEMPO,
MEN_EQUALIZER,
MEN_AUDIOTRACKBASE = 5000,

/* Video */
MEN_FULLSCREEN = 600,
/**/
MEN_DIMENSIONS,
MEN_DIMENSIONS_FREE,
MEN_DIMENSIONS_ORIGINAL,
MEN_DIMENSIONS_HALF,
MEN_DIMENSIONS_DOUBLE,
/**/
MEN_ASPECT,
MEN_ASPECT_AUTO,
MEN_ASPECT_4_3,
MEN_ASPECT_5_4,
MEN_ASPECT_16_9,
MEN_ASPECT_16_10,
MEN_ASPECT_IGNORE,
MEN_SCREENSHOT,
MEN_CROP,
MEN_SCALE,
/**/
MEN_DEINTERLACER, 
MEN_DEINTERLACER_OFF = 6000,
MEN_DEINTERLACER_L5,
MEN_DEINTERLACER_YADIF,
MEN_DEINTERLACER_LB,
MEN_DEINTERLACER_YADIF1,
MEN_DEINTERLACER_KERNDEINT,
/**/
MEN_VIDEOFILTER = 700,
MEN_DENOISE_OFF = 7000,
MEN_DENOISE_NORMAL,
MEN_DENOISE_SOFT,
MEN_DEBLOCK = 800,
MEN_DERING,
/**/
MEN_ROTATE = 900,
MEN_ROTATE_OFF = 8000,
MEN_ROTATE_1,
MEN_ROTATE_2,
MEN_ROTATE_3,
MEN_ROTATE_4,
MEN_MIRROR = 1000,
MEN_FLIP
};

/* mplayer vars */
extern float sub_aspect;
extern play_tree_t* playtree;
extern m_config_t* mconfig;

/* gui struct */
typedef struct gui_t gui_t;

struct gui_t
{
	APTR wbscreen; /* wb screen handle (should be updated on screennotify */

	APTR menustrip;

	APTR maingroup;
	APTR playlistgroup;
	APTR urlgroup;
	APTR dvddirgroup;
	APTR propertiesgroup;
	APTR prefsgroup;
	APTR cropgroup;
	APTR scalegroup;
	APTR audiogaingroup;
	APTR consolegroup;
	APTR equalizergroup;

	APTR aboutwindow;
	APTR mainwindow;
	APTR playlistwindow;
	APTR urlwindow;
	APTR dvddirwindow;
	APTR propertieswindow;
	APTR prefswindow;
	APTR cropwindow;
	APTR scalewindow;
	APTR audiogainwindow;
	APTR consolewindow;
	APTR equalizerwindow;

	ULONG colorkey; /* overlay colorkey pen */
	APTR videoarea; /* video object area */
	struct Window * videowindow; /* used by vo driver */
	ULONG video_top; /* object coordinates updated in videoarea draw method */
	ULONG video_bottom;
	ULONG video_left;
	ULONG video_right;
	ULONG x_offset; /* ratio x offset */
	ULONG y_offset; /* ratio y offset */
	ULONG embedded; /* video displayed in main window or not? needs vo_cgx_*_gui */
	APTR  screen; /* public screen used in embedded mode */
	ULONG fullscreen;

	ULONG running; /* main loop flag */
	ULONG vo_opened; /* gui syncs on that */
	ULONG gui_ready; /* vo_*_gui syncs on that */

	playlist_t *playlist;               		  /* playlist structure */
	void (*startplay)(gui_t *gui);     			  /* start playback */
	void (*updatedisplay)(gui_t *gui);  		  /* refresh gui */
	void (*playercontrol)(int msg, float param);  /* userdefined call back function (guiSetEvent) */

	ULONG status_pushid;
	ULONG last_status_update; /* status last update timestamp */
	ULONG status_dirty; /* if set, status string has to be refreshed */

	ULONG message_pushid;

	/* misc vars */
	STRPTR icy_info;
	ULONG dumpstream;

	ULONG fatalerror; /* Error flag to force quit in waiting loops */
};

/* application ptr */
extern APTR app;

/* gui handle */
extern gui_t *mygui;
extern int gui_initialized;

/* functions */
int creategui(void);

void guiLoadSubtitle(char *name);
void guiClearSubtitle(void);
void guiUpdateICY(char * info);

char * asl_run(STRPTR p, struct TagItem *tags, ULONG remember);
ULONG asl_run_multiple(STRPTR p, struct TagItem *tags, char *** files, ULONG remember);
void asl_free(ULONG count, char ** files);
ULONG name_match(CONST_STRPTR name, CONST_STRPTR pattern);

extern void update_playlistwindow(ULONG rebuild);
extern int parse_filename(char *file, play_tree_t *playtree, m_config_t *mconfig, int clear);
extern void capitalize(char *filename);
STRPTR get_title(void);

/* mplayer ui functions */
void uiCurr(void);
void uiFullScreen(void);
void uiNext(void);
void uiPause(void);
void uiPlay(void);
void uiPrev(void);
void uiEnd(void);
void uiAbsSeek(float sec);
void uiRelSeek(float percent);
void uiSetFileName(char *dir, char *name, int type);
void uiState(void);

/* external vars */
extern int fullscreen;

#endif




