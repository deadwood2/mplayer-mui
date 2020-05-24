#include "help_mp.h"
#include "mplayer.h"
#include "libao2/audio_out.h"
#include "libvo/video_out.h"
#include "sub/sub.h"

#include "gui/interface.h"
#include "gui.h"


extern char cfg_dvd_device[];
extern char *stream_dump_name;

/******************************************************************
 * prefsgeneralgroupclass
 *****************************************************************/

static STRPTR regtitles[] = {
	"General",
	"Subtitles",
	NULL
};

static CONST CONST_STRPTR cy_loopfiltermodes[] =
{
	"Don't Skip Anything",
	"Skip All Non Reference Frames",
	"Skip All Non Key Frames",
	"Skip All Frames",
	NULL
};

static struct
{
 char * name;
 char * comment;
} encodings[] =
 {
  { "unicode",     MSGTR_PREFERENCES_FontEncoding1 },
  { "iso-8859-1",  MSGTR_PREFERENCES_FontEncoding2 },
  { "iso-8859-15", MSGTR_PREFERENCES_FontEncoding3 },
  { "iso-8859-2",  MSGTR_PREFERENCES_FontEncoding4 },
  { "cp1250",      MSGTR_PREFERENCES_FontEncoding22},
  { "iso-8859-3",  MSGTR_PREFERENCES_FontEncoding5 },
  { "iso-8859-4",  MSGTR_PREFERENCES_FontEncoding6 },
  { "iso-8859-5",  MSGTR_PREFERENCES_FontEncoding7 },
  { "cp1251",      MSGTR_PREFERENCES_FontEncoding21},
  { "iso-8859-6",  MSGTR_PREFERENCES_FontEncoding8 },
  { "cp1256",      MSGTR_PREFERENCES_FontEncoding23 },
  { "iso-8859-7",  MSGTR_PREFERENCES_FontEncoding9 },
  { "iso-8859-9",  MSGTR_PREFERENCES_FontEncoding10 },
  { "iso-8859-13", MSGTR_PREFERENCES_FontEncoding11 },
  { "iso-8859-14", MSGTR_PREFERENCES_FontEncoding12 },
  { "iso-8859-8",  MSGTR_PREFERENCES_FontEncoding13 },
  { "koi8-r",      MSGTR_PREFERENCES_FontEncoding14 },
  { "koi8-u/ru",   MSGTR_PREFERENCES_FontEncoding15 },
  { "cp936",       MSGTR_PREFERENCES_FontEncoding16 },
  { "big5",        MSGTR_PREFERENCES_FontEncoding17 },
  { "shift-jis",   MSGTR_PREFERENCES_FontEncoding18 },
  { "cp949",       MSGTR_PREFERENCES_FontEncoding19 },
  { "cp874",       MSGTR_PREFERENCES_FontEncoding20 },
  { NULL, NULL }
 };

static CONST CONST_STRPTR cy_encodings[] = {
	MSGTR_PREFERENCES_FontEncoding1,
	MSGTR_PREFERENCES_FontEncoding2,
	MSGTR_PREFERENCES_FontEncoding3,
	MSGTR_PREFERENCES_FontEncoding4,
	MSGTR_PREFERENCES_FontEncoding22,
	MSGTR_PREFERENCES_FontEncoding5,
	MSGTR_PREFERENCES_FontEncoding6,
	MSGTR_PREFERENCES_FontEncoding7,
	MSGTR_PREFERENCES_FontEncoding21,
	MSGTR_PREFERENCES_FontEncoding8,
	MSGTR_PREFERENCES_FontEncoding23,
	MSGTR_PREFERENCES_FontEncoding9,
	MSGTR_PREFERENCES_FontEncoding10,
	MSGTR_PREFERENCES_FontEncoding11,
	MSGTR_PREFERENCES_FontEncoding12,
	MSGTR_PREFERENCES_FontEncoding13,
	MSGTR_PREFERENCES_FontEncoding14,
	MSGTR_PREFERENCES_FontEncoding15,
	MSGTR_PREFERENCES_FontEncoding16,
	MSGTR_PREFERENCES_FontEncoding17,
	MSGTR_PREFERENCES_FontEncoding18,
	MSGTR_PREFERENCES_FontEncoding19,
	MSGTR_PREFERENCES_FontEncoding20,
	NULL
};

static CONST CONST_STRPTR cy_autoscale[] = {
	"No autoscaling",
	"Proportional to movie height",
	"Proportional to movie width",
	"Proportional to movie diagonal",
	NULL
};

struct PrefsGeneralGroupData
{
	APTR PL_VideoDrivers;
	APTR LI_VideoDrivers;
	APTR ST_VideoDrivers;
	APTR PL_AudioDrivers;
	APTR LI_AudioDrivers;
	APTR ST_AudioDrivers;

	APTR ST_DVDDevice;
	APTR CH_FrameDrop;
	APTR ST_AutoSync;
	APTR CH_AutoSync;

	APTR CH_CacheFile;
	APTR ST_CacheFile;
	APTR CH_CacheStream;
	APTR ST_CacheStream;
	APTR CH_CacheDVD;
	APTR ST_CacheDVD;
	APTR CY_LoopFilter;

	APTR CH_RememberPath;
	APTR CH_RememberPlaylist;
	APTR ST_DumpPath;

	APTR CY_Encoding;
	APTR ST_FontName;
	APTR CY_AutoScale;
};

DEFNEW(PrefsGeneralGroup)
{
	APTR PL_VideoDrivers, LI_VideoDrivers, ST_VideoDrivers,
     	 PL_AudioDrivers, LI_AudioDrivers, ST_AudioDrivers;
	APTR ST_DVDDevice, CH_FrameDrop, ST_AutoSync, CH_AutoSync;
	APTR CH_CacheFile, ST_CacheFile, CH_CacheStream, ST_CacheStream, CH_CacheDVD, ST_CacheDVD/*, CY_LoopFilter*/;
	APTR CH_RememberPath, CH_RememberPlaylist, ST_DumpPath;
	APTR CY_Encoding, ST_FontName, CY_AutoScale;

	obj = (Object *)DoSuperNew(cl, obj,
//			MUIA_Register_Titles, regtitles,
		  Child, VGroup,
					Child,
						ColGroup(2), GroupFrameT("Output Drivers"),
							Child,
								Label("Video Driver:"),
							Child,
								PL_VideoDrivers = NewObject(getprefspopstringclass(), NULL,
														MUIA_CycleChain, 1,
														MUIA_Popstring_String, ST_VideoDrivers = TextObject, TextFrame, MUIA_Background, MUII_TextBack, End,
														MUIA_Popstring_Button, PopButton(MUII_PopUp),
														MUIA_Popobject_Object, LI_VideoDrivers = NewObject(getpoplistclass(), NULL, TAG_DONE),
												        End,
							Child,
								Label("Audio Driver:"),
							Child,
								PL_AudioDrivers = NewObject(getprefspopstringclass(), NULL,
														MUIA_CycleChain, 1,
														MUIA_Popstring_String, ST_AudioDrivers = TextObject, TextFrame, MUIA_Background, MUII_TextBack, End,
														MUIA_Popstring_Button, PopButton(MUII_PopUp),
														MUIA_Popobject_Object, LI_AudioDrivers = NewObject(getpoplistclass(), NULL, TAG_DONE),
												        End,
							End,

					Child,
						ColGroup(2), GroupFrameT("Devices"),
							Child,
								Label("DVD Device:"),
							Child,
								ST_DVDDevice = MakeString(""),
							End,

					Child,
						ColGroup(4), GroupFrameT("Performance"),
							Child, CH_FrameDrop = MakeCheck("Allow Framedrop", FALSE),
							Child, TextObject, MUIA_Text_Contents, "Allow Framedrop", End,
							Child, HSpace(0),
							Child, HSpace(0),

							/*
							Child, TextObject, MUIA_Text_Contents, "Loop Filter Mode (H264)", End,
							Child, ST_CacheDVD = MakeCycle("Loop Filter Mode (H264)", cy_loopfiltermodes);
							Child, HSpace(0),
							Child, HSpace(0),
							*/

							Child, CH_AutoSync = MakeCheck("Autosync", FALSE),
							Child, TextObject, MUIA_Text_Contents, "Auto Synchronisation Factor", End,
							Child, ST_AutoSync = MakeNumericString("0"),
							Child, HSpace(0),

							Child, CH_CacheFile = MakeCheck("Cache Files", FALSE),
							Child, TextObject, MUIA_Text_Contents, "Cache Files", End,
							Child, ST_CacheFile = MakeNumericString("0"),
							Child, LLabel("kB"),

							Child, CH_CacheStream = MakeCheck("Cache Network Streams", FALSE),
							Child, TextObject, MUIA_Text_Contents, "Cache Network Streams", End,
							Child, ST_CacheStream = MakeNumericString("0"),
							Child, LLabel("kB"),

							Child, CH_CacheDVD = MakeCheck("Cache DVD", FALSE),
							Child, TextObject, MUIA_Text_Contents, "Cache DVD", End,
							Child, ST_CacheDVD = MakeNumericString("0"),
							Child, LLabel("kB"),

							End,

					Child,
						VGroup, GroupFrameT("Miscellaneous"),
							Child, ColGroup(2),
								Child, CH_RememberPath = MakeCheck("Remember last visited path", TRUE),
								Child, TextObject, MUIA_Text_Contents, "Remember last visited path", End,
								Child, CH_RememberPlaylist = MakeCheck("Restore last playlist", TRUE),
								Child, TextObject, MUIA_Text_Contents, "Restore last playlist", End,
								End,
							Child, ColGroup(2),
								Child, Label("Save Recorded Stream To:"),
								Child, ST_DumpPath = MakeFileString(""),
								End,
							End,

					End,
#if 0
		  Child, ColGroup(2),
					Child, TextObject, MUIA_Text_Contents, "Subtitles Encoding:", End,
					Child, CY_Encoding = Cycle(cy_encodings),

					Child, TextObject, MUIA_Text_Contents, "Subtitles TrueType Font:", End,
					Child, PopaslObject, MUIA_Popstring_Button, ImageObject, ImageButtonFrame,
										 MUIA_InputMode, MUIV_InputMode_RelVerify,
										 MUIA_Image_Spec, MUII_PopFile,
										 MUIA_Background, MUII_ButtonBack,
										 End,
										 MUIA_Popstring_String, ST_FontName = String("",255),
										 End,

					Child, TextObject, MUIA_Text_Contents, "Autoscale:", End,
					Child, CY_AutoScale = Cycle(cy_autoscale),

					End,
#endif
		TAG_DONE
	);

	if (obj)
	{
		struct PrefsGeneralGroupData *data = INST_DATA(cl, obj);

		data->PL_VideoDrivers = PL_VideoDrivers;
		data->LI_VideoDrivers = LI_VideoDrivers;
		data->ST_VideoDrivers = ST_VideoDrivers;
		data->PL_AudioDrivers = PL_AudioDrivers;
		data->LI_AudioDrivers = LI_AudioDrivers;
		data->ST_AudioDrivers = ST_AudioDrivers;

		data->ST_DVDDevice = ST_DVDDevice;
		
		data->CH_FrameDrop = CH_FrameDrop;
		
		data->ST_AutoSync = ST_AutoSync;
		data->CH_AutoSync = CH_AutoSync;

		data->CH_CacheFile = CH_CacheFile;
		data->ST_CacheFile = ST_CacheFile;
		data->CH_CacheStream = CH_CacheStream;
		data->ST_CacheStream = ST_CacheStream;
		data->CH_CacheDVD = CH_CacheDVD;
		data->ST_CacheDVD = ST_CacheDVD;
		/*data->CY_LoopFilter = CY_LoopFilter;*/

		data->CH_RememberPath = CH_RememberPath;
		data->CH_RememberPlaylist = CH_RememberPlaylist;
		data->ST_DumpPath = ST_DumpPath;

#if 0
		data->CY_Encoding = CY_Encoding;
		data->ST_FontName = ST_FontName;
		data->CY_AutoScale = CY_AutoScale;
#endif
		DoMethod(obj, MM_PrefsGroup_Update);

		DoMethod(data->LI_VideoDrivers, MUIM_Notify, MUIA_List_DoubleClick, TRUE,
				 data->PL_VideoDrivers, 2, MUIM_Popstring_Close, TRUE);

		DoMethod(data->LI_AudioDrivers, MUIM_Notify, MUIA_List_DoubleClick, TRUE,
				 data->PL_AudioDrivers, 2, MUIM_Popstring_Close, TRUE);

		/* autosync */
		DoMethod(data->CH_AutoSync, MUIM_Notify, MUIA_Selected, FALSE,
				 data->ST_AutoSync, 3, MUIM_Set, MUIA_Disabled, TRUE);

		DoMethod(data->CH_AutoSync, MUIM_Notify, MUIA_Selected, TRUE,
				 data->ST_AutoSync, 3, MUIM_Set, MUIA_Disabled, FALSE);

		/* cache */
		DoMethod(data->CH_CacheFile, MUIM_Notify, MUIA_Selected, FALSE,
				 data->ST_CacheFile, 3, MUIM_Set, MUIA_Disabled, TRUE);

		DoMethod(data->CH_CacheFile, MUIM_Notify, MUIA_Selected, TRUE,
				 data->ST_CacheFile, 3, MUIM_Set, MUIA_Disabled, FALSE);

		DoMethod(data->CH_CacheStream, MUIM_Notify, MUIA_Selected, FALSE,
				 data->ST_CacheStream, 3, MUIM_Set, MUIA_Disabled, TRUE);

		DoMethod(data->CH_CacheStream, MUIM_Notify, MUIA_Selected, TRUE,
				 data->ST_CacheStream, 3, MUIM_Set, MUIA_Disabled, FALSE);

		DoMethod(data->CH_CacheDVD, MUIM_Notify, MUIA_Selected, FALSE,
				 data->ST_CacheDVD, 3, MUIM_Set, MUIA_Disabled, TRUE);

		DoMethod(data->CH_CacheDVD, MUIM_Notify, MUIA_Selected, TRUE,
				 data->ST_CacheDVD, 3, MUIM_Set, MUIA_Disabled, FALSE);
	}

	return (IPTR)obj;
}

DEFTMETHOD(PrefsGroup_Update)
{
	struct PrefsGeneralGroupData *data = INST_DATA(cl, obj);
	int i;

/* general */

	/* output drivers */
	DoMethod(data->LI_VideoDrivers, MUIM_List_Clear);

	i = 0;
	while(video_out_drivers[i++])
	{
		if (video_out_drivers[i-1]->control(VOCTRL_GUISUPPORT, NULL) == VO_TRUE )
		{
			DoMethod(data->LI_VideoDrivers, MUIM_List_InsertSingle, video_out_drivers[i-1]->info->short_name, MUIV_List_Insert_Bottom);
		}
	}

	if(video_driver_list)
	{
		char * name = strdup(video_driver_list[0]);
		char * sep = strchr(name, ':');
		if(sep) *sep=0;

		i = 0;

		while(video_out_drivers[i++])
		{
			if (!strcmp(video_out_drivers[i-1]->info->short_name, name))
			{
				set(data->LI_VideoDrivers, MUIA_List_Active, i-1);
				break;
			}
		}
	}
	else
	{
		set(data->LI_VideoDrivers, MUIA_List_Active, 0);
	}

	DoMethod(data->LI_AudioDrivers, MUIM_List_Clear);

	i = 0;
	while(audio_out_drivers[i++])
	{
		DoMethod(data->LI_AudioDrivers, MUIM_List_InsertSingle, audio_out_drivers[i-1]->info->short_name, MUIV_List_Insert_Bottom);
	}

	if(audio_driver_list)
	{
		char * name = strdup(audio_driver_list[0]);
		char * sep = strchr(name, ':');
		if(sep) *sep=0;

		i = 0;
		while(audio_out_drivers[i++])
		{
			if (!strcmp(audio_out_drivers[i-1]->info->short_name, name))
			{
				set(data->LI_AudioDrivers, MUIA_List_Active, i-1);
				break;
			}
		}
	}
	else
	{
		set(data->LI_AudioDrivers, MUIA_List_Active, 0);
	}

	DoMethod(obj, MM_PrefsGroup_RefreshDrivers);

	/* devices */
	set(data->ST_DVDDevice, MUIA_String_Contents, dvd_device ? dvd_device : DEFAULT_DVD_DEVICE);

	/* framedrop */
	set(data->CH_FrameDrop, MUIA_Selected, frame_dropping);

	/* autosync */
	set(data->CH_AutoSync, MUIA_Selected, gtkAutoSyncOn);
	set(data->ST_AutoSync, MUIA_Disabled, gtkAutoSyncOn == FALSE);
	set(data->ST_AutoSync, MUIA_String_Integer, gtkAutoSync);

	/* cache */
	set(data->CH_CacheFile, MUIA_Selected, gtkCacheSizeFile != 0);
	set(data->ST_CacheFile, MUIA_Disabled, gtkCacheSizeFile == 0);
	set(data->ST_CacheFile, MUIA_String_Integer, gtkCacheSizeFile);
	set(data->CH_CacheStream, MUIA_Selected, gtkCacheSizeNet != 0);
	set(data->ST_CacheStream, MUIA_Disabled, gtkCacheSizeNet == 0);
	set(data->ST_CacheStream, MUIA_String_Integer, gtkCacheSizeNet);
	set(data->CH_CacheDVD, MUIA_Selected, gtkCacheSizeDVD != 0);
	set(data->ST_CacheDVD, MUIA_Disabled, gtkCacheSizeDVD == 0);
	set(data->ST_CacheDVD, MUIA_String_Integer, gtkCacheSizeDVD);

	set(data->CH_RememberPath,     MUIA_Selected, remember_path);
	set(data->CH_RememberPlaylist, MUIA_Selected, remember_playlist);
	set(data->ST_DumpPath, MUIA_String_Contents, stream_dump_name);

/* subtitles */
#if 0
	if(font_name)
	{
		set(data->ST_FontName, MUIA_String_Contents, font_name);
	}
	set(data->CY_AutoScale, MUIA_Cycle_Active, subtitle_autoscale);
	if(sub_cp)
	{
		for(i=0; encodings[i].name; i++)
		{
			if(!strcmp(sub_cp, encodings[i].name))
			{
			   set(data->CY_Encoding, MUIA_Cycle_Active, i);
				break;
			}
		}
	}
	else
	{
		set(data->CY_Encoding, MUIA_Cycle_Active, 0);
	}
#endif
	return 0;
}

DEFTMETHOD(PrefsGroup_RefreshDrivers)
{
	struct PrefsGeneralGroupData *data = INST_DATA(cl, obj);
	STRPTR entry;

	DoMethod(data->LI_VideoDrivers, MUIM_List_GetEntry, MUIV_List_GetEntry_Active, &entry);
	if(entry)
	{
		set(data->ST_VideoDrivers, MUIA_Text_Contents, entry);
	}

	DoMethod(data->LI_AudioDrivers, MUIM_List_GetEntry, MUIV_List_GetEntry_Active, &entry);
	if(entry)
	{
		set(data->ST_AudioDrivers, MUIA_Text_Contents, entry);
	}

	return 0;
}

DEFTMETHOD(PrefsGroup_Store)
{
	struct PrefsGeneralGroupData *data = INST_DATA(cl, obj);

/* general */

	/* output drivers */
	listSet(&video_driver_list, (STRPTR) getv(data->ST_VideoDrivers, MUIA_Text_Contents));
	if (strcmp((STRPTR) getv(data->ST_VideoDrivers, MUIA_Text_Contents), "cgx_wpa") != 0)
		listRepl(&video_driver_list, NULL, "cgx_wpa"); // Always add wpa as fallback

	/* update embedded mode */
	mygui->embedded = FALSE;
	if(!strcmp(video_driver_list[0], "cgx_overlay_gui") || !strcmp(video_driver_list[0], "cgx_wpa_gui"))
	{
		mygui->embedded = TRUE;
	}

	listSet(&audio_driver_list, (STRPTR) getv(data->ST_AudioDrivers, MUIA_Text_Contents));

	/* devices */
	free(dvd_device);
	dvd_device = strdup((STRPTR) getv(data->ST_DVDDevice, MUIA_String_Contents));
	stccpy(cfg_dvd_device, dvd_device, MAX_PATH);

	/* framedrop */
	frame_dropping = getv(data->CH_FrameDrop, MUIA_Selected) ? 1 : 0;

	/* autosync */
	gtkAutoSyncOn = getv(data->CH_AutoSync, MUIA_Selected);
	gtkAutoSync   = getv(data->ST_AutoSync, MUIA_String_Integer);

	/* cache */
	gtkCacheSizeFile = getv(data->CH_CacheFile, MUIA_Selected) ? getv(data->ST_CacheFile, MUIA_String_Integer) : 0;
	gtkCacheSizeNet  = getv(data->CH_CacheStream, MUIA_Selected) ? getv(data->ST_CacheStream, MUIA_String_Integer) : 0;
	gtkCacheSizeDVD  = getv(data->CH_CacheDVD, MUIA_Selected) ? getv(data->ST_CacheDVD, MUIA_String_Integer) : 0;

	free(stream_dump_name);
	stream_dump_name = strdup((STRPTR) getv(data->ST_DumpPath, MUIA_String_Contents));

	remember_path     = getv(data->CH_RememberPath, MUIA_Selected);
	remember_playlist = getv(data->CH_RememberPlaylist, MUIA_Selected);

/* subtitles */
#if 0
	{
		STRPTR ptr = (STRPTR) getv(data->ST_FontName, MUIA_String_Contents);
		if(ptr && *ptr)
		{
			free(font_name);
			font_name = strdup(ptr);
		}
		else
		{
			font_name = NULL;
		}
		subtitle_autoscale = getv(data->CY_AutoScale, MUIA_Cycle_Active);	 
		free(sub_cp);
		sub_cp = strdup(encodings[getv(data->CY_Encoding, MUIA_Cycle_Active)].name);
	}
#endif
	return 0;
}

BEGINMTABLE2(prefsgeneralgroupclass)
DECNEW(PrefsGeneralGroup)
DECTMETHOD(PrefsGroup_Store)
DECTMETHOD(PrefsGroup_Update)
DECTMETHOD(PrefsGroup_RefreshDrivers)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Group, prefsgeneralgroupclass, PrefsGeneralGroupData)
