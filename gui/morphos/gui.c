#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>

#include <proto/timer.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/asl.h>
#include <clib/debug_protos.h>

#include <workbench/workbench.h>
#include <workbench/icon.h>
#include <proto/icon.h>
#include <mui/Aboutbox_mcc.h>

#include "input/input.h"
#include "mpcommon.h"
#include "libvo/video_out.h"

#include "morphos_stuff.h"

#include "gui/interface.h"
#include "gui.h"

#define D(x)


extern struct Task *maintask;
extern char muiversion[];
extern char muititle[];
extern char revision[];
extern char * _ProgramName;

extern int fullscreen;

#if !defined(__AROS__)
struct Library * MUIMasterBase = NULL;
#endif

guiInterface_t guiInfo;

play_tree_t *playtree = NULL;
float sub_aspect;

APTR app;

extern void initguimembers(void);

/*******************************************************************/

CONST TEXT credits[] =
	"\033bCredits:\033n\n"
	"\t(C) by MPlayer Team\n\n"
	"\033bMorphOS Port:\033n\n"
	"\t(C) 2003-2004 by Nicolas Det\n"
	"\t(C) 2005-2011 by Fabien Coeurjoly\n\n"
	"\033bAROS/AxRuntime Port:\033n\n"
	"\t(C) 2008-2020 by Krzysztof Smiechowicz\n\n"
	"\033bReferences:\033n\n"
	"\thttp://mplayerhq.hu\n"
	"\thttp://fabportnawak.free.fr/mplayer/\n"
	"\thttps://github.com/deadw00d/mplayer-mui";

static struct NewMenu MenuData[] =
{
	/* Project */
	{ NM_TITLE, (UBYTE *) "Project"                  , 0 ,0 ,0             ,(APTR)0            },
	{ NM_ITEM , (UBYTE *) "Open File..."             ,(UBYTE *) "O",0 ,0   ,(APTR)MEN_FILE     },
	{ NM_ITEM , (UBYTE *) "Open Directory..."        ,(UBYTE *) 0,0 ,0     ,(APTR)MEN_DIRECTORY},
	{ NM_ITEM , (UBYTE *) "Open Playlist..."         ,(UBYTE *) 0,0 ,0     ,(APTR)MEN_PLAYLIST },
	{ NM_ITEM , (UBYTE *) "Open Stream..."           ,(UBYTE *) "U",0 ,0   ,(APTR)MEN_STREAM   },
	{ NM_ITEM , NM_BARLABEL                          , 0 ,0 ,0             ,(APTR)0            },
	{ NM_ITEM , (UBYTE *) "Play DVD"                 ,(UBYTE *) "D",0 ,0   ,(APTR)MEN_DVD      },
	{ NM_ITEM , (UBYTE *) "Play DVD from Directory..."  ,(UBYTE *) 0,0 ,0  ,(APTR)MEN_DVD_DIR },
	{ NM_ITEM , (UBYTE *) "Use DVD Menu"             ,(UBYTE *) 0, CHECKIT|MENUTOGGLE ,0,(APTR)MEN_DVDNAV },
	{ NM_ITEM , NM_BARLABEL                          , 0 ,0 ,0             ,(APTR)0            },
	{ NM_ITEM , (UBYTE *) "Properties..."            ,(UBYTE *) "I",0 ,0   ,(APTR)MEN_PROPERTIES },
	{ NM_ITEM , (UBYTE *) "Messages..."              ,(UBYTE *) "M",0 ,0   ,(APTR)MEN_MESSAGES },
	{ NM_ITEM , NM_BARLABEL                          , 0 ,0 ,0             ,(APTR)0            },
	{ NM_ITEM , (UBYTE *) "About..."                 ,(UBYTE *) "?",0 ,0   ,(APTR)MEN_ABOUT    },
	{ NM_ITEM , NM_BARLABEL                          , 0 ,0 ,0             ,(APTR)0            },
	{ NM_ITEM , (UBYTE *) "Iconify"                  ,(UBYTE *) "H",0 ,0   ,(APTR)MEN_HIDE     },
	{ NM_ITEM , (UBYTE *) "Quit"                     ,(UBYTE *) "Q",0 ,0   ,(APTR)MEN_QUIT     },

	/* Play */
	{ NM_TITLE, (UBYTE *) "Control"                  , 0 ,0 ,0             ,(APTR)0            },
	{ NM_ITEM , (UBYTE *) "Playlist..."              ,(UBYTE *) "L",0 ,0   ,(APTR)MEN_PLAYLISTWIN },
	{ NM_ITEM , NM_BARLABEL                          , 0 ,0 ,0             ,(APTR)0            },
	{ NM_ITEM , (UBYTE *) "Play"                     ,(UBYTE *) "P",0 ,0   ,(APTR)MEN_PLAY     },
	{ NM_ITEM , (UBYTE *) "Pause"                    ,(UBYTE *) 0,0 ,0     ,(APTR)MEN_PAUSE    },
	{ NM_ITEM , (UBYTE *) "Stop"                     ,(UBYTE *) "S",0 ,0   ,(APTR)MEN_STOP     },
	{ NM_ITEM , (UBYTE *) "Frame Step"               ,(UBYTE *) ".",0 ,0   ,(APTR)MEN_FRAMESTEP},
	{ NM_ITEM , NM_BARLABEL                          , 0 ,0 ,0             ,(APTR)0            },
	{ NM_ITEM, (UBYTE *) "Seek"                      , 0 ,0 ,0             ,(APTR)0 },
	{ NM_SUB, (UBYTE *) "-10 seconds"                , 0 ,0 ,0             ,(APTR)MEN_SEEKB10S },
	{ NM_SUB, (UBYTE *) "+10 seconds"                , 0 ,0 ,0             ,(APTR)MEN_SEEKF10S },
	{ NM_SUB, (UBYTE *) "-1 minute"                  , 0 ,0 ,0             ,(APTR)MEN_SEEKB1M },
	{ NM_SUB, (UBYTE *) "+1 minute"                  , 0 ,0 ,0             ,(APTR)MEN_SEEKF1M },
	{ NM_SUB, (UBYTE *) "-10 minutes"                , 0 ,0 ,0             ,(APTR)MEN_SEEKB10M },
	{ NM_SUB, (UBYTE *) "+10 minutes"                , 0 ,0 ,0             ,(APTR)MEN_SEEKF10M },
	{ NM_ITEM , NM_BARLABEL                          , 0 ,0 ,0             ,(APTR)0            },
	{ NM_ITEM, (UBYTE *) "Increase Speed"            , 0 ,0 ,0             ,(APTR)MEN_INCSPEED },
	{ NM_ITEM, (UBYTE *) "Decrease Speed"            , 0 ,0 ,0             ,(APTR)MEN_DECSPEED },
	{ NM_ITEM , NM_BARLABEL                          , 0 ,0 ,0             ,(APTR)0            },
	{ NM_ITEM , (UBYTE *) "Repeat"                   ,(UBYTE *) 0,0 ,0     ,(APTR)MEN_REPEAT   },
	{ NM_SUB , (UBYTE *) "Off"                       ,(UBYTE *) 0,CHECKIT|MENUTOGGLE ,0     ,(APTR)MEN_REPEAT_OFF },
	{ NM_SUB , (UBYTE *) "Title"                     ,(UBYTE *) 0,CHECKIT|MENUTOGGLE ,0     ,(APTR)MEN_REPEAT_SINGLE },
	{ NM_SUB , (UBYTE *) "Playlist"                  ,(UBYTE *) 0,CHECKIT|MENUTOGGLE ,0     ,(APTR)MEN_REPEAT_PLAYLIST },
	{ NM_SUB , (UBYTE *) "Exit At Playlist End"      ,(UBYTE *) 0,CHECKIT|MENUTOGGLE ,0     ,(APTR)MEN_REPEAT_QUIT },
	{ NM_ITEM , (UBYTE *) "Next"                     ,(UBYTE *) "N",0 ,0   ,(APTR)MEN_NEXT     },
	{ NM_ITEM , (UBYTE *) "Previous"                 ,(UBYTE *) "B",0 ,0   ,(APTR)MEN_PREVIOUS },
	{ NM_ITEM , NM_BARLABEL                          , 0 ,0 ,0             ,(APTR)0            },
	{ NM_ITEM , (UBYTE *) "Record Stream"            ,(UBYTE *) 0, CHECKIT|MENUTOGGLE ,0,(APTR)MEN_RECORD },

	/* Video */

	{ NM_TITLE, (UBYTE *) "Video"                    , 0 ,0 ,0             ,(APTR)0 },
	{ NM_ITEM , (UBYTE *) "Fullscreen"               ,(UBYTE *) 0,0 ,0     ,(APTR)MEN_FULLSCREEN },
	{ NM_ITEM  , NM_BARLABEL                         , 0 ,0 ,0             ,(APTR)0 },
	{ NM_ITEM , (UBYTE *) "Window Dimension"         ,(UBYTE *) 0,0 ,0     ,(APTR)MEN_DIMENSIONS },
	{ NM_SUB  , (UBYTE *) "Ignore"                   ,(UBYTE *) 0,CHECKIT|MENUTOGGLE, 0     ,(APTR)MEN_DIMENSIONS_FREE },
	{ NM_SUB  , NM_BARLABEL                          , 0 ,0 ,0                              ,(APTR)0 },
	{ NM_SUB  , (UBYTE *) "50%"                      ,(UBYTE *) 0,CHECKIT|MENUTOGGLE, 0     ,(APTR)MEN_DIMENSIONS_HALF },
	{ NM_SUB  , (UBYTE *) "100%"                     ,(UBYTE *) 0,CHECKIT|MENUTOGGLE, 0     ,(APTR)MEN_DIMENSIONS_ORIGINAL },
	{ NM_SUB  , (UBYTE *) "200%"                     ,(UBYTE *) 0,CHECKIT|MENUTOGGLE, 0     ,(APTR)MEN_DIMENSIONS_DOUBLE },

	{ NM_ITEM , (UBYTE *) "Aspect Ratio"             ,(UBYTE *) 0,0 ,0     ,(APTR)MEN_ASPECT },
	{ NM_SUB  , (UBYTE *) "Auto"                     ,(UBYTE *) 0,0, 0     ,(APTR)MEN_ASPECT_AUTO },
	{ NM_SUB  , NM_BARLABEL                          , 0 ,0 ,0             ,(APTR)0            },
	{ NM_SUB  , (UBYTE *) "4:3"                      ,(UBYTE *) 0,0, 0 ,(APTR)MEN_ASPECT_4_3 },
	{ NM_SUB  , (UBYTE *) "5:4"                      ,(UBYTE *) 0,0, 0 ,(APTR)MEN_ASPECT_5_4 },
	{ NM_SUB  , (UBYTE *) "16:9"                     ,(UBYTE *) 0,0, 0 ,(APTR)MEN_ASPECT_16_9 },
	{ NM_SUB  , (UBYTE *) "16:10"                    ,(UBYTE *) 0,0, 0 ,(APTR)MEN_ASPECT_16_10 },

	{ NM_ITEM , (UBYTE *) "Deinterlacers"            ,(UBYTE *) 0,0 ,0                      ,(APTR)MEN_DEINTERLACER },
	{ NM_SUB  , (UBYTE *) "Lowpass 5"                ,(UBYTE *) 0,CHECKIT|MENUTOGGLE ,0     ,(APTR)MEN_DEINTERLACER_L5 },
	{ NM_SUB  , (UBYTE *) "Linear Blend"             ,(UBYTE *) 0,CHECKIT|MENUTOGGLE ,0     ,(APTR)MEN_DEINTERLACER_LB },
	{ NM_SUB  , (UBYTE *) "Yadif Normal"             ,(UBYTE *) 0,CHECKIT|MENUTOGGLE ,0     ,(APTR)MEN_DEINTERLACER_YADIF },
	{ NM_SUB  , (UBYTE *) "Yadif Double-Rate"        ,(UBYTE *) 0,CHECKIT|MENUTOGGLE ,0     ,(APTR)MEN_DEINTERLACER_YADIF1 },
	{ NM_SUB  , (UBYTE *) "Kerndeint"                ,(UBYTE *) 0,CHECKIT|MENUTOGGLE ,0     ,(APTR)MEN_DEINTERLACER_KERNDEINT },

	{ NM_ITEM , (UBYTE *) "Filters"                  ,(UBYTE *) 0,0 ,0                      ,(APTR)MEN_VIDEOFILTER },
	{ NM_SUB  , (UBYTE *) "Deblock"                  ,(UBYTE *) 0,CHECKIT|MENUTOGGLE ,0     ,(APTR)MEN_DEBLOCK },
	{ NM_SUB  , (UBYTE *) "Dering"                   ,(UBYTE *) 0,CHECKIT|MENUTOGGLE ,0     ,(APTR)MEN_DERING },
	{ NM_SUB  , NM_BARLABEL                          , 0 ,0 ,0                              ,(APTR)0 },
	{ NM_SUB  , (UBYTE *) "Denoise Normal"           ,(UBYTE *) 0,CHECKIT|MENUTOGGLE ,0     ,(APTR)MEN_DENOISE_NORMAL },
	{ NM_SUB  , (UBYTE *) "Denoise Soft"             ,(UBYTE *) 0,CHECKIT|MENUTOGGLE ,0     ,(APTR)MEN_DENOISE_SOFT },

	{ NM_ITEM , (UBYTE *)"Flip"                      ,(UBYTE *) 0,0 ,0                      ,(APTR)0 },
	{ NM_SUB , (UBYTE *) "Horizontally"              ,(UBYTE *) 0,CHECKIT|MENUTOGGLE ,0     ,(APTR)MEN_MIRROR },
	{ NM_SUB , (UBYTE *) "Vertically"                ,(UBYTE *) 0,CHECKIT|MENUTOGGLE ,0     ,(APTR)MEN_FLIP },

	{ NM_ITEM , (UBYTE *) "Rotate"                       ,(UBYTE *) 0,0 ,0     ,(APTR)MEN_ROTATE },
	{ NM_SUB  , (UBYTE *) "90° Clockwise Flipped"        ,(UBYTE *) 0,CHECKIT|MENUTOGGLE ,0 ,(APTR)MEN_ROTATE_1 },
	{ NM_SUB  , (UBYTE *) "90° Clockwise"                ,(UBYTE *) 0,CHECKIT|MENUTOGGLE ,0 ,(APTR)MEN_ROTATE_2 },
	{ NM_SUB  , (UBYTE *) "90° Counterclockwise"         ,(UBYTE *) 0,CHECKIT|MENUTOGGLE ,0 ,(APTR)MEN_ROTATE_3 },
	{ NM_SUB  , (UBYTE *) "90° Counterclockwise Flipped" ,(UBYTE *) 0,CHECKIT|MENUTOGGLE ,0 ,(APTR)MEN_ROTATE_4 },

	{ NM_ITEM  , NM_BARLABEL                         , 0 ,0 ,0             ,(APTR)0            },
	{ NM_ITEM , (UBYTE *) "Crop..."                  ,(UBYTE *) 0,0 ,0     ,(APTR)MEN_CROP },
	{ NM_ITEM , (UBYTE *) "Scale..."                 ,(UBYTE *) 0,0 ,0     ,(APTR)MEN_SCALE },

	{ NM_ITEM  , NM_BARLABEL                         , 0 ,0 ,0             ,(APTR)0            },
	{ NM_ITEM , (UBYTE *) "Screenshot"               ,(UBYTE *) 0,0 ,0     ,(APTR)MEN_SCREENSHOT },

	/* Audio */

	{ NM_TITLE, (UBYTE *) "Audio"                    , 0 ,0 ,0         ,(APTR)0 },
	{ NM_ITEM  , (UBYTE *) "Select Audio Track"      ,(UBYTE *) 0,0 ,0 ,(APTR)MEN_AUDIOTRACK },
	{ NM_ITEM  , NM_BARLABEL                         , 0 ,0 ,0         ,(APTR)0            },
	{ NM_ITEM , (UBYTE *) "Mute"                     ,(UBYTE *) 0,0 ,0 ,(APTR)MEN_MUTE },
	{ NM_ITEM  , NM_BARLABEL                         , 0 ,0 ,0         ,(APTR)0            },
	{ NM_ITEM , (UBYTE *) "Increase Volume"          ,(UBYTE *) 0,0 ,0 ,(APTR)MEN_INCVOLUME },
	{ NM_ITEM , (UBYTE *) "Decrease Volume"          ,(UBYTE *) 0,0 ,0 ,(APTR)MEN_DECVOLUME },
	{ NM_ITEM  , NM_BARLABEL                         , 0 ,0 ,0         ,(APTR)0            },
	{ NM_ITEM , (UBYTE *) "Increase Delay"           ,(UBYTE *) 0,0 ,0 ,(APTR)MEN_AUDIOINCDELAY },
	{ NM_ITEM , (UBYTE *) "Decrease Delay"           ,(UBYTE *) 0,0 ,0 ,(APTR)MEN_AUDIODECDELAY },
	{ NM_ITEM  , NM_BARLABEL                         , 0 ,0 ,0         ,(APTR)0            },
	{ NM_ITEM , (UBYTE *) "Volume Gain..."           ,(UBYTE *) 0,0 ,0 ,(APTR)MEN_AUDIOGAIN },
	{ NM_ITEM , (UBYTE *) "Equalizer..."             ,(UBYTE *) 0,0 ,0 ,(APTR)MEN_EQUALIZER },
	{ NM_ITEM , (UBYTE *) "Filters"                  ,(UBYTE *) 0,0 ,0 ,(APTR)MEN_AUDIOFILTER },
	{ NM_SUB  , (UBYTE *) "Extra Stereo"             ,(UBYTE *) 0,CHECKIT|MENUTOGGLE ,0     ,(APTR)MEN_EXTRASTEREO },
	{ NM_SUB  , (UBYTE *) "Karaoke"                  ,(UBYTE *) 0,CHECKIT|MENUTOGGLE ,0     ,(APTR)MEN_KARAOKE },
	{ NM_SUB  , (UBYTE *) "Volume Normalization"     ,(UBYTE *) 0,CHECKIT|MENUTOGGLE ,0     ,(APTR)MEN_VOLNORM },
	{ NM_SUB  , (UBYTE *) "Scale Tempo"              ,(UBYTE *) 0,CHECKIT|MENUTOGGLE ,0     ,(APTR)MEN_SCALETEMPO },

	/* Subtitles */
	{ NM_TITLE, (UBYTE *) "Subtitles"                , 0 ,0 ,0             ,(APTR)0 },
	{ NM_ITEM , (UBYTE *) "Cycle Subtitles"          ,(UBYTE *) 0,0 ,0     ,(APTR)MEN_CYCLESUBTITLE },
	{ NM_ITEM , (UBYTE *) "Select Subtitles"         ,(UBYTE *) 0,0 ,0     ,(APTR)MEN_SELECTSUBTITLE },
	{ NM_ITEM , (UBYTE *) "Load Subtitles..."        ,(UBYTE *) 0,0 ,0     ,(APTR)MEN_LOADSUBTITLE },
	{ NM_ITEM , (UBYTE *) "Unload Subtitles"         ,(UBYTE *) 0,0 ,0     ,(APTR)MEN_UNLOADSUBTITLE },
	{ NM_ITEM  , NM_BARLABEL                         , 0 ,0 ,0             ,(APTR)0            },
	{ NM_ITEM , (UBYTE *) "Increase Delay"           ,(UBYTE *) 0,0 ,0     ,(APTR)MEN_SUBINCDELAY },
	{ NM_ITEM , (UBYTE *) "Decrease Delay"           ,(UBYTE *) 0,0 ,0     ,(APTR)MEN_SUBDECDELAY },
	{ NM_ITEM  , NM_BARLABEL                         , 0 ,0 ,0             ,(APTR)0            },
	{ NM_ITEM , (UBYTE *) "Use SSA/ASS Library"      ,(UBYTE *) 0, CHECKIT|MENUTOGGLE ,0 ,(APTR)MEN_ENABLEASS },

	/* Options */
	{ NM_TITLE, (UBYTE *) "Settings"                 , 0 ,0 ,0             ,(APTR)0 },
	{ NM_ITEM , (UBYTE *) "Preferences..."           ,(UBYTE *) 0,0 ,0     ,(APTR)MEN_PREFERENCES },
	{ NM_ITEM , NM_BARLABEL                          , 0 ,0 ,0             ,(APTR)0            },
	{ NM_ITEM , (UBYTE *) "GUI"                      ,(UBYTE *) 0,0 ,0     ,(APTR)0 },
	{ NM_SUB , (UBYTE *) "Enable"                    ,(UBYTE *) 0, CHECKIT|MENUTOGGLE ,0,(APTR)MEN_TOGGLEGUI },
	{ NM_SUB , NM_BARLABEL                           , 0 ,0 ,0             ,(APTR)0            },
	{ NM_SUB , (UBYTE *) "Toolbar"                   ,(UBYTE *) 0, CHECKIT|MENUTOGGLE ,0,(APTR)MEN_SHOWTOOLBAR },
	{ NM_SUB , (UBYTE *) "Control"                   ,(UBYTE *) 0, CHECKIT|MENUTOGGLE ,0,(APTR)MEN_SHOWCONTROL },
	{ NM_SUB , (UBYTE *) "Status"                    ,(UBYTE *) 0, CHECKIT|MENUTOGGLE ,0,(APTR)MEN_SHOWSTATUS },
	{ NM_ITEM , NM_BARLABEL                          , 0 ,0 ,0             ,(APTR)0            },
	{ NM_ITEM , (UBYTE *) "Save"                     ,(UBYTE *) 0,0 ,0     ,(APTR)MEN_SAVESETTINGS },
	{ NM_ITEM , NM_BARLABEL                          , 0 ,0 ,0             ,(APTR)0            },
	{ NM_ITEM , (UBYTE *) "MUI..."                   ,(UBYTE *) 0,0 ,0     ,(APTR)MEN_MUI      },


	{ NM_END, NULL, 0, 0, 0, (APTR)0 }
};
/************************************************************/

ULONG name_match(CONST_STRPTR name, CONST_STRPTR pattern)
{
	ULONG rc = FALSE;

	if (name && pattern)
	{
		TEXT statbuf[ 128 ];	/* To remove memory allocation in most cases */
		STRPTR dynbuf = NULL;
		STRPTR buf;
		ULONG len = strlen( pattern ) * 2 + 2;

		if ( len > sizeof( statbuf ) )
			buf = dynbuf = (STRPTR) malloc( len );
		else
			buf = (STRPTR) statbuf;

		if ( buf )
		{


			if (ParsePatternNoCase(pattern, (UBYTE *)buf, len) != -1)
			{
				if (MatchPatternNoCase(buf, (STRPTR) name))
				{
					rc = TRUE;
				}
			}
		}

		if ( dynbuf )
			free( dynbuf );
	}

	return (rc);
}

ULONG asl_run_multiple(STRPTR p, struct TagItem *tags, char *** files, ULONG remember)
{
	ULONG count = 0;
	STRPTR dir, file;

	*files = NULL;
	file = NULL;
	dir = NULL;

	if (p)
	{
		dir = AllocVecTaskPooled(strlen(p) + 1);

		if (dir)
		{ /*
			ULONG len;

			file = FilePart(p);

			len = file - p + 1;
			stccpy(dir, p, len);  */
			ULONG len;
			BPTR l;

			memset(dir, 0, strlen(p) + 1);

			l = Lock(p, ACCESS_READ);

			if(l)
			{
				struct FileInfoBlock fi;

				if(Examine(l, &fi))
				{
					if(fi.fib_DirEntryType > 0)
					{
						len = strlen(p) + 1;
                        stccpy(dir, p, len);
						file = NULL;
					}
					else
					{
						file = FilePart(p);
						len = file - p + 1;
						stccpy(dir, p, len);
					}
				}
				UnLock(l);
			}
			else
			{
				file = FilePart(p);
				len = file - p + 1;
				stccpy(dir, p, len);
			}
		}
	}

	if (dir || !p)
	{
		struct FileRequester *req;

		req = (struct FileRequester *)MUI_AllocAslRequestTags(ASL_FileRequest,
			ASLFR_DoPatterns, TRUE,
			ASLFR_DoMultiSelect, TRUE,
			file == dir ? TAG_IGNORE : ASLFR_InitialDrawer, dir,
			file ? ASLFR_InitialFile : TAG_IGNORE, file,
			TAG_MORE, tags
		);

		if (req)
		{
			if (MUI_AslRequestTags(req, TAG_DONE))
			{
				count = req->fr_NumArgs;

				*files = AllocVecTaskPooled(count*sizeof(char *));

				if(*files)
				{
					int i;
					struct WBArg *ap;
					char buf[256];

					for (ap=req->fr_ArgList,i=0;i<req->fr_NumArgs;i++,ap++)
					{
						if(NameFromLock(ap->wa_Lock, buf, sizeof(buf)))
						{
							MorphOS_SetLastVisitedPath(req->fr_Drawer);

							if(remember)
							{
								MorphOS_SetPattern(req->fr_Pattern);
							}

							AddPart(buf, ap->wa_Name, sizeof(buf));

							(*files)[i] = AllocVecTaskPooled(strlen(buf)+1);

							if((*files)[i])
							{
								strcpy((*files)[i], buf);
							}
						}
					}
				}
			}

			MUI_FreeAslRequest(req);
		}

		if (dir)
		{
			FreeVecTaskPooled(dir);
		}
	}

	return count;
}

void asl_free(ULONG count, char ** files)
{
	ULONG i;
	for(i = 0; i < count; i++)
	{
		FreeVecTaskPooled(files[i]);
	}
	FreeVecTaskPooled(files);
}

char * asl_run(STRPTR p, struct TagItem *tags, ULONG remember)
{
	STRPTR result, dir, file;

	file = NULL;
	dir = NULL;

	if (p)
	{
		dir = AllocVecTaskPooled(strlen(p) + 1);

		if (dir)
		{ /*
			ULONG len;

			file = FilePart(p);

			len = file - p + 1;
			stccpy(dir, p, len);*/

			ULONG len;
			BPTR l;

			memset(dir, 0, strlen(p) + 1);

			l = Lock(p, ACCESS_READ);

			if(l)
			{
				struct FileInfoBlock fi;

				if(Examine(l, &fi))
				{

					if(fi.fib_DirEntryType > 0)
					{
						len = strlen(p) + 1;
                        stccpy(dir, p, len);
						file = NULL;
					}
					else
					{
						file = FilePart(p);
						len = file - p + 1;
						stccpy(dir, p, len);
					}
				}
				UnLock(l);
			}
			else
			{
				file = FilePart(p);
				len = file - p + 1;
				stccpy(dir, p, len);
			}
		}
	}

	result = NULL;

	if (dir || !p)
	{
		struct FileRequester *req;

		req = (struct FileRequester *)MUI_AllocAslRequestTags(ASL_FileRequest,
			ASLFR_DoPatterns, TRUE,
			file == dir ? TAG_IGNORE : ASLFR_InitialDrawer, dir,
			file ? ASLFR_InitialFile : TAG_IGNORE, file,
			TAG_MORE, tags
		);

		if (req)
		{
			if (MUI_AslRequestTags(req, TAG_DONE))
			{
				ULONG tlen;

				tlen = strlen(req->fr_Drawer) + strlen(req->fr_File) + 8;

				result = AllocVecTaskPooled(tlen);

				if (result)
				{
					strcpy(result, req->fr_Drawer);
					AddPart(result, req->fr_File, tlen);

					MorphOS_SetLastVisitedPath(req->fr_Drawer);
					if(remember)
					{
						MorphOS_SetPattern(req->fr_Pattern);
					}
				}
			}

			MUI_FreeAslRequest(req);
		}

		if (dir)
		{
			FreeVecTaskPooled(dir);
		}
	}

	return result;
}

int creategui(void)
{
	ULONG signals;
	int res = 0;
	struct DiskObject *diskobject = NULL;
	TEXT iconpath[256];

	/* objects */
	APTR menustrip = NULL;
	APTR mainwindow = NULL, prefswindow = NULL,
		 urlwindow = NULL, dvddirwindow = NULL, playlistwindow = NULL, propertieswindow = NULL,
		 cropwindow = NULL, scalewindow = NULL, audiogainwindow = NULL, consolewindow = NULL,
		 equalizerwindow = NULL;
	APTR GR_MPlayerGroup = NULL, GR_PlaylistGroup = NULL, GR_URLGroup = NULL,
		 GR_DVDDirGroup = NULL, GR_PropertiesGroup = NULL, GR_PrefsGroup = NULL,
		 GR_CropGroup = NULL, GR_ScaleGroup = NULL, GR_AudioGainGroup = NULL, GR_ConsoleGroup = NULL,
		 GR_EqualizerGroup = NULL;
	APTR aboutwindow = NULL;

	snprintf(iconpath, sizeof(iconpath), "PROGDIR:%s.info", _ProgramName);

#if !defined(__AROS__)
	if (!(MUIMasterBase = OpenLibrary((UBYTE *)MUIMASTER_NAME, MUIMASTER_VMIN)))
	{
		fprintf(stderr, "Failed to open "MUIMASTER_NAME".");
		res = 1;
		goto quit;
	}
#endif

	mygui = malloc(sizeof(*mygui));

	if(mygui)
	{
		initguimembers();
	}
	else
	{
		res = 1;
		goto quit;
	}

	if(!classes_init())
	{
		res = 1;
		goto quit;
	}

	mygui->running = TRUE;

	app = NewObject(getmplayerappclass(), NULL,
			MUIA_Application_Title      , "MPlayer",
			MUIA_Application_Version    , muiversion,
			MUIA_Application_Copyright  , "(C)2005-2011 Fabien Coeurjoly",
			MUIA_Application_Author     , "Fabien Coeurjoly",
			MUIA_Application_Description, APPLICATION_DESCRIPTION,
			MUIA_Application_Base       , APPLICATION_BASE,
			MUIA_Application_DiskObject , diskobject = GetDiskObject((UBYTE *)_ProgramName),
			MUIA_Application_Menustrip  , menustrip = MUI_MakeObject(MUIO_MenustripNM, MenuData, 0),


			SubWindow, prefswindow = WindowObject,
					MUIA_Window_ScreenTitle, muititle,
					MUIA_Window_Title, "Preferences",
					MUIA_Window_ID   , MAKE_ID('M','P','P','R'),
					WindowContents,
						GR_PrefsGroup = NewObject(getprefsgroupclass(), NULL, TAG_DONE),

			End,

			SubWindow, playlistwindow = WindowObject,
					MUIA_Window_ScreenTitle, muititle,
					MUIA_Window_Title, "Playlist",
					MUIA_Window_ID, MAKE_ID('P','L','A','Y'),
					MUIA_Window_AppWindow, TRUE,
					MUIA_Window_Width, 600,
					WindowContents,
						GR_PlaylistGroup = NewObject(getplaylistgroupclass(), NULL, TAG_DONE),

			End,

			SubWindow, mainwindow = NewObject(getmplayerwindowclass(), NULL,
					MUIA_Window_ScreenTitle, muititle,
					MUIA_Window_Title, "MPlayer",
					MUIA_Window_ID, MAKE_ID('M','A','I','N'),
					MUIA_Window_AppWindow, TRUE,
					//MUIA_Window_Menustrip, menustrip = MUI_MakeObject(MUIO_MenustripNM, MenuData, 0),
					WindowContents,
						GR_MPlayerGroup = NewObject(getmplayergroupclass(), NULL, TAG_DONE),
			End,

			SubWindow, urlwindow = WindowObject,
					MUIA_Window_ScreenTitle, muititle,
					MUIA_Window_Title, "Enter URL",
					MUIA_Window_ID, MAKE_ID('U','R','L','W'),
					MUIA_Window_Width, 600,
					WindowContents,
						GR_URLGroup = NewObject(geturlgroupclass(), NULL, MA_URLGroup_Target, GR_MPlayerGroup, TAG_DONE),
			End,

			SubWindow, dvddirwindow = WindowObject,
					MUIA_Window_ScreenTitle, muititle,
					MUIA_Window_Title, "Select DVD directory",
					MUIA_Window_ID, MAKE_ID('D','V','D','D'),
					WindowContents,
						GR_DVDDirGroup = NewObject(getdvddirgroupclass(), NULL, TAG_DONE),
			End,

			SubWindow, propertieswindow = WindowObject,
					MUIA_Window_ScreenTitle, muititle,
					MUIA_Window_Title, "Properties",
					MUIA_Window_ID   , MAKE_ID('M','P','I','N'),
					WindowContents,
						GR_PropertiesGroup = NewObject(getpropertiesgroupclass(), NULL, TAG_DONE),
			End,

			SubWindow, cropwindow = WindowObject,
					MUIA_Window_ScreenTitle, muititle,
					MUIA_Window_Title, "Crop",
					MUIA_Window_ID   , MAKE_ID('M','P','C','F'),
					WindowContents,
						GR_CropGroup = NewObject(getcropgroupclass(), NULL, TAG_DONE),
			End,

			SubWindow, scalewindow = WindowObject,
					MUIA_Window_ScreenTitle, muititle,
					MUIA_Window_Title, "Scale",
					MUIA_Window_ID   , MAKE_ID('M','P','S','F'),
					WindowContents,
						GR_ScaleGroup = NewObject(getscalegroupclass(), NULL, TAG_DONE),
			End,

			SubWindow, audiogainwindow = WindowObject,
					MUIA_Window_ScreenTitle, muititle,
					MUIA_Window_Title, "Volume",
					MUIA_Window_ID   , MAKE_ID('M','P','V','F'),
					WindowContents,
						GR_AudioGainGroup = NewObject(getaudiogaingroupclass(), NULL, TAG_DONE),
			End,

			SubWindow, consolewindow = WindowObject,
					MUIA_Window_ScreenTitle, muititle,
					MUIA_Window_Title, "Messages",
					MUIA_Window_ID   , MAKE_ID('M','P','M','E'),
					WindowContents,
						GR_ConsoleGroup = NewObject(getconsolegroupclass(), NULL, TAG_DONE),
			End,

			SubWindow, equalizerwindow = WindowObject,
					MUIA_Window_ScreenTitle, muititle,
					MUIA_Window_Title, "Equalizer",
					MUIA_Window_ID   , MAKE_ID('M','P','E','Q'),
					WindowContents,
						GR_EqualizerGroup = NewObject(getequalizergroupclass(), NULL, TAG_DONE),
			End,


	End;

	if (!app)
	{
		free(mygui);
		mygui = NULL;
		fprintf(stderr, "Failed to create Application.\n");
		res = 1;
		goto quit;
	}

#if defined(__AROS__)
    set(GR_URLGroup, MA_URLGroup_Target, GR_MPlayerGroup);
#endif

    if(remember_path)
		MorphOS_RestorePath();

	/* Dynamically added aboutbox (falls back to mui_request if not available) */
	aboutwindow = AboutboxObject,
				  MUIA_Aboutbox_Build, revision,
				  MUIA_Aboutbox_Credits, credits,
				  MUIA_Aboutbox_LogoFile, iconpath,
				  End;

	if(aboutwindow)
	{
		DoMethod(app, OM_ADDMEMBER, aboutwindow);
	}

	/* Menu handling */
	DoMethod(menustrip, MUIM_SetUData, MEN_DVDNAV, MUIA_Menuitem_Checked, gui_use_dvdnav);

	DoMethod(menustrip, MUIM_SetUData, MEN_TOGGLEGUI, MUIA_Menuitem_Checked, gui_show_gui);
	DoMethod(menustrip, MUIM_SetUData, MEN_SHOWSTATUS, MUIA_Menuitem_Checked, gui_show_status);
	DoMethod(menustrip, MUIM_SetUData, MEN_SHOWTOOLBAR, MUIA_Menuitem_Checked, gui_show_toolbar);
	DoMethod(menustrip, MUIM_SetUData, MEN_SHOWCONTROL, MUIA_Menuitem_Checked, gui_show_control);

	DoMethod(menustrip, MUIM_SetUData, MEN_ENABLEASS, MUIA_Menuitem_Checked, ass_enabled);

	DoMethod(menustrip, MUIM_SetUData, MEN_AUDIOTRACK, MUIA_Menuitem_Enabled, FALSE);
	DoMethod(menustrip, MUIM_SetUData, MEN_SELECTSUBTITLE, MUIA_Menuitem_Enabled, FALSE);

	DoMethod(menustrip, MUIM_SetUData, MEN_REPEAT_OFF, MUIA_Menuitem_Exclude, 2|4|8);
	DoMethod(menustrip, MUIM_SetUData, MEN_REPEAT_SINGLE, MUIA_Menuitem_Exclude, 1|4|8);
	DoMethod(menustrip, MUIM_SetUData, MEN_REPEAT_PLAYLIST, MUIA_Menuitem_Exclude, 1|2|8);
	DoMethod(menustrip, MUIM_SetUData, MEN_REPEAT_QUIT, MUIA_Menuitem_Exclude, 1|2|4);

	DoMethod(menustrip, MUIM_SetUData, MEN_ROTATE_1, MUIA_Menuitem_Exclude, 2|4|8);
	DoMethod(menustrip, MUIM_SetUData, MEN_ROTATE_2, MUIA_Menuitem_Exclude, 1|4|8);
	DoMethod(menustrip, MUIM_SetUData, MEN_ROTATE_3, MUIA_Menuitem_Exclude, 1|2|8);
	DoMethod(menustrip, MUIM_SetUData, MEN_ROTATE_4, MUIA_Menuitem_Exclude, 1|2|4);

	DoMethod(menustrip, MUIM_SetUData, MEN_DIMENSIONS_FREE, MUIA_Menuitem_Exclude, 4|8|16);
	DoMethod(menustrip, MUIM_SetUData, MEN_DIMENSIONS_HALF, MUIA_Menuitem_Exclude, 1|8|16);
	DoMethod(menustrip, MUIM_SetUData, MEN_DIMENSIONS_ORIGINAL, MUIA_Menuitem_Exclude, 1|4|16);
	DoMethod(menustrip, MUIM_SetUData, MEN_DIMENSIONS_DOUBLE, MUIA_Menuitem_Exclude, 1|4|8);

	{
		ULONG menuentry;

		/* dimensions */
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

		DoMethod(menustrip, MUIM_SetUData, menuentry, MUIA_Menuitem_Checked, TRUE);

		/* AudioGainGroup mode */
		switch(gui_repeat_mode)
		{
			default:
			case 0:
				menuentry = MEN_REPEAT_OFF;
				break;
			case 1:
				menuentry = MEN_REPEAT_SINGLE;
				break;
			case 2:
				menuentry = MEN_REPEAT_PLAYLIST;
				break;
			case 3:
				menuentry = MEN_REPEAT_QUIT;
				break;
		}

		DoMethod(menustrip, MUIM_SetUData, menuentry, MUIA_Menuitem_Checked, TRUE);
	}

	/* Window handling */
	DoMethod(mainwindow, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
			 app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

	DoMethod(prefswindow,MUIM_Notify,MUIA_Window_CloseRequest,TRUE,
			 prefswindow, 3, MUIM_Set, MUIA_Window_Open, FALSE);

	DoMethod(playlistwindow, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
			 playlistwindow, 3, MUIM_Set, MUIA_Window_Open, FALSE);

	DoMethod(urlwindow, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
			 urlwindow, 3, MUIM_Set, MUIA_Window_Open, FALSE);

	DoMethod(dvddirwindow, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
			 dvddirwindow, 3, MUIM_Set, MUIA_Window_Open, FALSE);

	DoMethod(propertieswindow, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
			 propertieswindow, 3, MUIM_Set, MUIA_Window_Open, FALSE);

	DoMethod(cropwindow, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
			 cropwindow, 3, MUIM_Set, MUIA_Window_Open, FALSE);

	DoMethod(scalewindow, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
			 scalewindow, 3, MUIM_Set, MUIA_Window_Open, FALSE);

	DoMethod(audiogainwindow, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
			 audiogainwindow, 3, MUIM_Set, MUIA_Window_Open, FALSE);

	DoMethod(consolewindow, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
			 consolewindow, 3, MUIM_Set, MUIA_Window_Open, FALSE);

	DoMethod(equalizerwindow, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
			 equalizerwindow, 3, MUIM_Set, MUIA_Window_Open, FALSE);

	set(mainwindow, MUIA_Window_Open, TRUE);

	mygui->last_status_update = 0;
	mygui->menustrip = menustrip;
	mygui->mainwindow = mainwindow;
	mygui->aboutwindow = aboutwindow;
	mygui->prefswindow = prefswindow;
	mygui->maingroup = GR_MPlayerGroup;
	mygui->playlistwindow = playlistwindow;
	mygui->playlistgroup = GR_PlaylistGroup;
	mygui->urlwindow = urlwindow;
	mygui->urlgroup = GR_URLGroup;
	mygui->dvddirwindow = dvddirwindow;
	mygui->dvddirgroup = GR_DVDDirGroup;
	mygui->propertieswindow = propertieswindow;
	mygui->propertiesgroup = GR_PropertiesGroup;
	mygui->prefsgroup = GR_PrefsGroup,
	mygui->cropwindow = cropwindow;
	mygui->cropgroup = GR_CropGroup;
	mygui->scalewindow = scalewindow;
	mygui->scalegroup = GR_ScaleGroup;
	mygui->audiogainwindow = audiogainwindow;
	mygui->audiogaingroup = GR_AudioGainGroup;
	mygui->consolewindow = consolewindow;
	mygui->consolegroup = GR_ConsoleGroup;
	mygui->equalizerwindow = equalizerwindow;
	mygui->equalizergroup = GR_EqualizerGroup;


	/* video attributes retrieved here */
	mygui->videowindow = (struct Window *) getv(mainwindow, MUIA_Window);
	mygui->video_top = _top(mygui->videoarea);
	mygui->video_bottom = _bottom(mygui->videoarea);
	mygui->video_left = _left(mygui->videoarea);
	mygui->video_right = _right(mygui->videoarea);

	gui_initialized = TRUE;

	if(mygui->embedded)
	{
		gui(GUI_SET_WINDOW_PTR, (void *) (fullscreen ? TRUE : FALSE));
	}

	while (mygui->running)
	{
		ULONG ret = DoMethod(app,MUIM_Application_NewInput,&signals);

		DoMethod(mygui->maingroup, MM_MPlayerGroup_HandleMenu, ret);

		switch(ret)
		{
			case MEN_QUIT:
			case MUIV_Application_ReturnID_Quit:
				mygui->running = FALSE;
				break;
		}

		if (mygui->running && signals)
		{
			signals = Wait(signals | SIGBREAKF_CTRL_E | SIGBREAKF_CTRL_C);
		}

		if(signals & (SIGBREAKF_CTRL_E | SIGBREAKF_CTRL_C))
		{
			D(kprintf("[GUI] received C|E signal, quitting loop\n"));
			mygui->running = FALSE;
		}
	}

quit:
	if(!res)
	{
		if(remember_path)
			MorphOS_SavePath();

		D(kprintf("[GUI] out of main loop, requesting to quit mplayer\n"));
		mp_input_queue_cmd(mp_input_parse_cmd("quit"));
		mp_input_queue_cmd(mp_input_parse_cmd("quit"));
		mp_input_queue_cmd(mp_input_parse_cmd("quit"));
		Signal(maintask, SIGBREAKF_CTRL_C);

		D(kprintf("[GUI] waiting for C|F signal\n"));
		Wait(SIGBREAKF_CTRL_F | SIGBREAKF_CTRL_C);

		D(kprintf("[GUI] signaling C|F to maintask\n"));
		Signal(maintask, SIGBREAKF_CTRL_F | SIGBREAKF_CTRL_C);

		D(kprintf("[GUI] closing window and disposing app\n"));

		if(mainwindow)       set(mainwindow, MUIA_Window_Open, FALSE);
		if(playlistwindow)   set(playlistwindow, MUIA_Window_Open, FALSE);
		if(aboutwindow)      set(aboutwindow, MUIA_Window_Open, FALSE);
		if(urlwindow)        set(urlwindow, MUIA_Window_Open, FALSE);
		if(dvddirwindow)     set(dvddirwindow, MUIA_Window_Open, FALSE);
		if(propertieswindow) set(propertieswindow, MUIA_Window_Open, FALSE);
		if(prefswindow)      set(prefswindow, MUIA_Window_Open, FALSE);
		if(cropwindow)       set(cropwindow, MUIA_Window_Open, FALSE);
		if(scalewindow)      set(scalewindow, MUIA_Window_Open, FALSE);
		if(audiogainwindow)  set(audiogainwindow, MUIA_Window_Open, FALSE);
		if(consolewindow)    set(consolewindow, MUIA_Window_Open, FALSE);
		if(equalizerwindow)  set(equalizerwindow, MUIA_Window_Open, FALSE);

		if(mygui->screen) CloseScreen(mygui->screen);
		mygui->screen = NULL;
	}

	if(diskobject)
	{
		FreeDiskObject(diskobject);
		diskobject = NULL;
	}

	if (app)
	{
		MUI_DisposeObject(app);
		app = NULL;
	}

	classes_cleanup();

#if !defined(__AROS__)
	if (MUIMasterBase)
	{
		CloseLibrary(MUIMasterBase);
		MUIMasterBase = NULL;
	}
#endif

	D(kprintf("[GUI] bye bye\n"));

	return res;
}


