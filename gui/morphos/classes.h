#ifndef __CLASSES_H__
#define __CLASSES_H__

#include "include/macros/vapor.h"
#include "global.h"

#include <proto/alib.h>
#include <exec/exec.h>
#include <proto/exec.h>
#include <utility/tagitem.h>

#include <intuition/intuition.h>
#include <proto/intuition.h>

#include <libraries/gadtools.h>
#if defined(__AROS__)
#define MUI_OBSOLETE
#endif
#include <libraries/mui.h>
#include <proto/muimaster.h>

#define DEFCLASS(s) IPTR create_##s##class(void); \
	APTR get##s##class(void); \
	APTR get##s##classroot(void); \
	void delete_##s##class(void)

ULONG classes_init(void);
void  classes_cleanup(void);

/************************************************************/

/* Classes */
DEFCLASS(mplayerapp);
DEFCLASS(mplayerwindow);
DEFCLASS(mplayergroup);
DEFCLASS(playlistgroup);
DEFCLASS(urlgroup);
DEFCLASS(dvddirgroup);
DEFCLASS(propertiesgroup);
DEFCLASS(prefsgroup);
DEFCLASS(prefsgeneralgroup);
DEFCLASS(playlistlist);
DEFCLASS(prefslist);
DEFCLASS(seekslider);
DEFCLASS(volumeslider);
DEFCLASS(picturebutton);
DEFCLASS(spacer);
DEFCLASS(videoarea);
DEFCLASS(poplist);
DEFCLASS(popstring);
DEFCLASS(urlpopstring);
DEFCLASS(prefspopstring);
DEFCLASS(cropgroup);
DEFCLASS(scalegroup);
DEFCLASS(audiogaingroup);
DEFCLASS(consolegroup);
DEFCLASS(consolelist);
DEFCLASS(equalizergroup);

#define MTAGBASE (TAG_USER|((0xDEADL<<16)+0))

/* Attributes */
enum {
	MA_dummy = (int)(MTAGBASE),

	MM_MPlayer_OpenStream,

	/* MPlayerApp */
	MM_MPlayerApp_DisposeWindow,

	/* MPlayerGroup */
	MA_MPlayerGroup_Update,

	MM_MPlayerGroup_Show,

	MM_MPlayerGroup_ShowStatus,
	MM_MPlayerGroup_ShowToolbar,
	MM_MPlayerGroup_ShowControl,
	MM_MPlayerGroup_ShowDVDBrowser,
	MM_MPlayerGroup_ShowChapterBrowser,
	MM_MPlayerGroup_ShowPlaylist,
	MM_MPlayerGroup_ShowProperties,
	MM_MPlayerGroup_ShowPreferences,

	MM_MPlayerGroup_ShowPanels,

	MM_MPlayerGroup_Play,
	MM_MPlayerGroup_Pause,
	MM_MPlayerGroup_Stop,

	MM_MPlayerGroup_IncreaseSpeed,
	MM_MPlayerGroup_MultiplySpeed,

	MM_MPlayerGroup_Prev,
	MM_MPlayerGroup_Next,

	MM_MPlayerGroup_Loop,

	MM_MPlayerGroup_Record,

	MM_MPlayerGroup_OpenFileRequester,
	MM_MPlayerGroup_OpenPlaylistRequester,
	MM_MPlayerGroup_OpenDirectoryRequester,
	MM_MPlayerGroup_OpenDVDDirectoryRequester,
	MM_MPlayerGroup_OpenURLRequester,
	MM_MPlayerGroup_Open,
	MM_MPlayerGroup_Add,

	MM_MPlayerGroup_Seek,
	MM_MPlayerGroup_SmartSeek,
	MM_MPlayerGroup_ToggleFullScreen,
	MM_MPlayerGroup_Dimensions,
	MM_MPlayerGroup_Volume,
	MM_MPlayerGroup_Mute,
	MM_MPlayerGroup_Screenshot,
	MM_MPlayerGroup_Aspect,
	MM_MPlayerGroup_Deinterlacer,
	MM_MPlayerGroup_VideoFilter,
	MM_MPlayerGroup_Rotation,
	MM_MPlayerGroup_Flip,
	MM_MPlayerGroup_Mirror,
	MM_MPlayerGroup_Crop,
	MM_MPlayerGroup_Scale,
	
	MM_MPlayerGroup_ShowDVDMenu,

	MM_MPlayerGroup_BuildChapterList,
	MM_MPlayerGroup_SetChapter,

	MM_MPlayerGroup_BuildAngleList,
	MM_MPlayerGroup_SetAngle,

	MM_MPlayerGroup_BuildTitleList,
	MM_MPlayerGroup_SetTitle,

	MM_MPlayerGroup_CycleSubtitles,
	MM_MPlayerGroup_OpenSubtitleRequester,
	MM_MPlayerGroup_UnloadSubtitles,
	MM_MPlayerGroup_SelectSubtitle,
	MM_MPlayerGroup_RefreshSubtitles,
	MM_MPlayerGroup_IncreaseSubtitleDelay,
	MM_MPlayerGroup_DecreaseSubtitleDelay,

	MM_MPlayerGroup_SelectAudio,
	MM_MPlayerGroup_RefreshAudio,
	MM_MPlayerGroup_IncreaseVolume,
	MM_MPlayerGroup_DecreaseVolume,
	MM_MPlayerGroup_IncreaseAVDelay,
	MM_MPlayerGroup_DecreaseAVDelay,
	MM_MPlayerGroup_AudioGain,
	MM_MPlayerGroup_AudioFilter,
	MM_MPlayerGroup_Equalizer,

	MM_MPlayerGroup_Update,
	MM_MPlayerGroup_UpdateAll,
	MM_MPlayerGroup_SetWindow,
	MM_MPlayerGroup_SetValues,

	MM_MPlayerGroup_OpenFullScreen,
	MM_MPlayerGroup_Iconify,

	MM_MPlayerGroup_HandleMenu,

	/* PlaylistGroup */
	MA_PlaylistGroup_Current,

	MM_PlaylistGroup_Clear,
	MM_PlaylistGroup_AddRequester,
	MM_PlaylistGroup_Add,
	MM_PlaylistGroup_Remove,
	MM_PlaylistGroup_RemoveAll,
	MM_PlaylistGroup_MoveUp,
	MM_PlaylistGroup_MoveDown,
	MM_PlaylistGroup_Play,
	MM_PlaylistGroup_Shuffle,
	MM_PlaylistGroup_SaveRequester,
	MM_PlaylistGroup_Load,
	MM_PlaylistGroup_Save,

	MM_PlaylistGroup_Refresh,

	/* URLPopString */
	MA_URLPopString_ActivateString,

	MM_URLPopString_Insert,

	/* URLGroup */
	MA_URLGroup_Target,

	MM_URLGroup_Open,
	MM_URLGroup_Cancel,

	/* DVDDirGroup */
	MM_DVDDirGroup_Open,
	MM_DVDDirGroup_Cancel,

	/* PropertiesGroup */
	MM_PropertiesGroup_Update,
	MM_PropertiesGroup_Acknowledge,

	/* PrefsGroup */
	MM_PrefsGroup_Save,
	MM_PrefsGroup_Use,
	MM_PrefsGroup_Cancel,
	MM_PrefsGroup_SelectChange,
	MM_PrefsGroup_Store,
	MM_PrefsGroup_Load,
	MM_PrefsGroup_Update,
	MM_PrefsGroup_RefreshDrivers,

	/* PictureButton */
	MA_PictureButton_Name,
	MA_PictureButton_Path,
	MA_PictureButton_Hit,
	MA_PictureButton_UserData,

	/* CropGroup */
	MM_CropGroup_Apply,
	MM_CropGroup_Reset,
	MM_CropGroup_Cancel,
	MM_CropGroup_UpdateDimensions,

	/* ScaleGroup */
	MM_ScaleGroup_Apply,
	MM_ScaleGroup_Reset,
	MM_ScaleGroup_Cancel,
	MM_ScaleGroup_UpdateDimensions,

	/* AudioGainGroup */
	MM_AudioGainGroup_Apply,
	MM_AudioGainGroup_Reset,
	MM_AudioGainGroup_Cancel,

	/* ConsoleGroup */
	MM_ConsoleGroup_AddMessage,
	MM_ConsoleGroup_Clear,

	/* EqualizerGroup */
	MM_EqualizerGroup_Apply,
	MM_EqualizerGroup_Reset,
	MM_EqualizerGroup_Close,

	MA_dummyend
};

/* Method Structures */

struct MP_MPlayer_OpenStream {
	STACKED STACKED ULONG MethodID;
	STACKED STRPTR url;
};

/* MPlayerApp */

struct MP_MPlayerApp_DisposeWindow {
	STACKED STACKED ULONG MethodID;
	STACKED Object *obj;
};

/* MPlayerGroup */

struct MP_MPlayerGroup_Show {
	STACKED ULONG MethodID;
	STACKED ULONG show;
};

struct MP_MPlayerGroup_ShowStatus {
	STACKED ULONG MethodID;
	STACKED ULONG show;
};

struct MP_MPlayerGroup_ShowToolbar {
	STACKED ULONG MethodID;
	STACKED ULONG show;
};

struct MP_MPlayerGroup_ShowControl {
	STACKED ULONG MethodID;
	STACKED ULONG show;
};

struct MP_MPlayerGroup_ShowPanels {
	STACKED ULONG MethodID;
	STACKED ULONG show;
};

struct MP_MPlayerGroup_ShowChapterBrowser {
	STACKED ULONG MethodID;
	STACKED ULONG show;
};

struct MP_MPlayerGroup_ShowDVDBrowser {
	STACKED ULONG MethodID;
	STACKED ULONG show;
};

struct MP_MPlayerGroup_SetTitle {
	STACKED ULONG MethodID;
	STACKED ULONG update;
};

struct MP_MPlayerGroup_SetChapter {
	STACKED ULONG MethodID;
	STACKED ULONG update;
};

struct MP_MPlayerGroup_SetAngle {
	STACKED ULONG MethodID;
	STACKED ULONG update;
};

struct MP_MPlayerGroup_Open {
	STACKED STACKED ULONG MethodID;
	STACKED STACKED ULONG mode;
	STACKED TEXT * url;
};

struct MP_MPlayerGroup_Seek {
	STACKED ULONG MethodID;
	STACKED ULONG mode;
	STACKED ULONG value;
};

struct MP_MPlayerGroup_Update {
	STACKED ULONG MethodID;
	STACKED ULONG mode;
	STACKED APTR  data;
};

struct MP_MPlayerGroup_SetWindow {
	STACKED ULONG MethodID;
	STACKED APTR window;
};

struct MP_MPlayerGroup_OpenFullScreen {
	STACKED ULONG MethodID;
	STACKED APTR enable;
};

struct MP_MPlayerGroup_Dimensions {
	STACKED ULONG MethodID;
	STACKED ULONG which;
};

struct MP_MPlayerGroup_Aspect {
	STACKED ULONG MethodID;
	STACKED ULONG ratio;
};

struct MP_MPlayerGroup_Deinterlacer {
	STACKED ULONG MethodID;
	STACKED ULONG which;
};

struct MP_MPlayerGroup_VideoFilter {
	STACKED ULONG MethodID;
	STACKED ULONG which;
};

struct MP_MPlayerGroup_Rotation {
	STACKED ULONG MethodID;
	STACKED ULONG which;
};

struct MP_MPlayerGroup_Flip {
	STACKED ULONG MethodID;
	STACKED ULONG which;
};

struct MP_MPlayerGroup_Mirror {
	STACKED ULONG MethodID;
	STACKED ULONG which;
};

struct MP_MPlayerGroup_Crop {
	STACKED ULONG MethodID;
	STACKED LONG width;
	STACKED LONG height;
	STACKED LONG left;
	STACKED LONG top;
	STACKED LONG enable;
};

struct MP_MPlayerGroup_Scale {
	STACKED ULONG MethodID;
	STACKED LONG width;
	STACKED LONG height;
	STACKED LONG enable;
};

struct MP_MPlayerGroup_AudioFilter {
	STACKED ULONG MethodID;
	STACKED ULONG which;
};

struct MP_MPlayerGroup_AudioGain {
	STACKED ULONG MethodID;
	STACKED LONG gain;
	STACKED LONG enable;
};

struct MP_MPlayerGroup_Equalizer {
	STACKED ULONG MethodID;
	STACKED TEXT *param;
	STACKED LONG enable;
};

struct MP_MPlayerGroup_HandleMenu {
	STACKED ULONG MethodID;
	STACKED APTR userdata;
};

struct MP_MPlayerGroup_UnloadSubtitles
{
	STACKED ULONG MethodID;
	STACKED LONG which;
};

struct MP_MPlayerGroup_SelectSubtitle {
	STACKED ULONG MethodID;
	STACKED LONG which;
};

struct MP_MPlayerGroup_SelectAudio {
	STACKED ULONG MethodID;
	STACKED LONG which;
};

struct MP_MPlayerGroup_SetValues {
	STACKED ULONG MethodID;
	STACKED LONG available;
};

struct MP_MPlayerGroup_IncreaseSpeed {
	STACKED ULONG MethodID;
	STACKED ULONG value;
};

struct MP_MPlayerGroup_MultiplySpeed {
	STACKED ULONG MethodID;
	STACKED ULONG value;
};

struct MP_MPlayerGroup_Loop {
	STACKED ULONG MethodID;
	STACKED LONG which;
};

/* PlaylistGroup */
struct MP_PlaylistGroup_Add {
	STACKED ULONG MethodID;
	STACKED APTR * entry;
};

struct MP_PlaylistGroup_Refresh {
	STACKED ULONG MethodID;
	STACKED ULONG rebuild;
};

struct MP_PlaylistGroup_Load {
	STACKED ULONG MethodID;
	STACKED STRPTR path;
};

struct MP_PlaylistGroup_Save {
	STACKED ULONG MethodID;
	STACKED STRPTR path;
};

/* URLPopString */

struct MP_URLPopString_Insert {
	STACKED ULONG MethodID;
	STACKED STRPTR url;
};

/* PrefsGroup */

struct MP_PrefsGroup_SelectChange {
	STACKED ULONG MethodID;
	STACKED LONG listentry;
};

/* ConsoleGroup */

struct MP_ConsoleGroup_AddMessage {
	STACKED ULONG MethodID;
	STACKED STRPTR message;
};

/* Variables */

/* MPlayerGroup	*/

#define MV_MPlayerGroup_Open_File       0
#define MV_MPlayerGroup_Open_DVD        1
#define MV_MPlayerGroup_Open_Playlist   2
#define MV_MPlayerGroup_Open_Stream     3
#define MV_MPlayerGroup_Open_Directory  4
#define MV_MPlayerGroup_Open_DVD_Directory 5

#define MV_MPlayerGroup_Show_Off        0
#define MV_MPlayerGroup_Show_On         1
#define MV_MPlayerGroup_Show_Toggle     2

#define MV_MPlayerGroup_SeekFromSlider  0
#define MV_MPlayerGroup_SeekFromRewind  1
#define MV_MPlayerGroup_SeekFromForward 2

#define MV_MPlayerGroup_IncreaseSpeed   0
#define MV_MPlayerGroup_DecreaseSpeed   1

#define MV_MPlayerGroup_Update_All      0
#define MV_MPlayerGroup_Update_Position 1
#define MV_MPlayerGroup_Update_Volume   2
#define MV_MPlayerGroup_Update_Time     3
#define MV_MPlayerGroup_Update_Status   4
#define MV_MPlayerGroup_Update_Lists    5

#define MV_MPlayerGroup_UnloadSubtitles_All -1

#endif




