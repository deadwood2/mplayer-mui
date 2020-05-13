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

#define DEFCLASS(s) ULONG create_##s##class(void); \
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

	MA_dummyend
};

/* Method Structures */

struct MP_MPlayer_OpenStream {
	ULONG MethodID;
	STRPTR url;
};

/* MPlayerApp */

struct MP_MPlayerApp_DisposeWindow {
	ULONG MethodID;
	Object *obj;
};

/* MPlayerGroup */

struct MP_MPlayerGroup_Show {
	ULONG MethodID;
	ULONG show;
};

struct MP_MPlayerGroup_ShowStatus {
	ULONG MethodID;
	ULONG show;
};

struct MP_MPlayerGroup_ShowToolbar {
	ULONG MethodID;
	ULONG show;
};

struct MP_MPlayerGroup_ShowControl {
	ULONG MethodID;
	ULONG show;
};

struct MP_MPlayerGroup_ShowPanels {
	ULONG MethodID;
	ULONG show;
};

struct MP_MPlayerGroup_ShowChapterBrowser {
	ULONG MethodID;
	ULONG show;
};

struct MP_MPlayerGroup_ShowDVDBrowser {
	ULONG MethodID;
	ULONG show;
};

struct MP_MPlayerGroup_SetTitle {
	ULONG MethodID;
	ULONG update;
};

struct MP_MPlayerGroup_SetChapter {
	ULONG MethodID;
	ULONG update;
};

struct MP_MPlayerGroup_SetAngle {
	ULONG MethodID;
	ULONG update;
};

struct MP_MPlayerGroup_Open {
	ULONG MethodID;
	ULONG mode;
	TEXT * url;
};

struct MP_MPlayerGroup_Seek {
	ULONG MethodID;
	ULONG mode;
	ULONG value;
};

struct MP_MPlayerGroup_Update {
	ULONG MethodID;
	ULONG mode;
	APTR  data;
};

struct MP_MPlayerGroup_SetWindow {
	ULONG MethodID;
	APTR window;
};

struct MP_MPlayerGroup_OpenFullScreen {
	ULONG MethodID;
	APTR enable;
};

struct MP_MPlayerGroup_Dimensions {
	ULONG MethodID;
	ULONG which;
};

struct MP_MPlayerGroup_Aspect {
	ULONG MethodID;
	ULONG ratio;
};

struct MP_MPlayerGroup_Deinterlacer {
	ULONG MethodID;
	ULONG which;
};

struct MP_MPlayerGroup_VideoFilter {
	ULONG MethodID;
	ULONG which;
};

struct MP_MPlayerGroup_Rotation {
	ULONG MethodID;
	ULONG which;
};

struct MP_MPlayerGroup_Flip {
	ULONG MethodID;
	ULONG which;
};

struct MP_MPlayerGroup_Mirror {
	ULONG MethodID;
	ULONG which;
};

struct MP_MPlayerGroup_Crop {
	ULONG MethodID;
	LONG width;
	LONG height;
	LONG left;
	LONG top;
	LONG enable;
};

struct MP_MPlayerGroup_Scale {
	ULONG MethodID;
	LONG width;
	LONG height;
	LONG enable;
};

struct MP_MPlayerGroup_AudioFilter {
	ULONG MethodID;
	ULONG which;
};

struct MP_MPlayerGroup_AudioGain {
	ULONG MethodID;
	LONG gain;
	LONG enable;
};

struct MP_MPlayerGroup_HandleMenu {
	ULONG MethodID;
	APTR userdata;
};

struct MP_MPlayerGroup_UnloadSubtitles
{
	ULONG MethodID;
	LONG which;
};

struct MP_MPlayerGroup_SelectSubtitle {
	ULONG MethodID;
	LONG which;
};

struct MP_MPlayerGroup_SelectAudio {
	ULONG MethodID;
	LONG which;
};

struct MP_MPlayerGroup_SetValues {
	ULONG MethodID;
	LONG available;
};

struct MP_MPlayerGroup_IncreaseSpeed {
	ULONG MethodID;
	ULONG value;
};

struct MP_MPlayerGroup_MultiplySpeed {
	ULONG MethodID;
	ULONG value;
};

struct MP_MPlayerGroup_Loop {
	ULONG MethodID;
	LONG which;
};

/* PlaylistGroup */
struct MP_PlaylistGroup_Add {
	ULONG MethodID;
	APTR * entry;
};

struct MP_PlaylistGroup_Refresh {
	ULONG MethodID;
	ULONG rebuild;
};

struct MP_PlaylistGroup_Load {
	ULONG MethodID;
	STRPTR path;
};

struct MP_PlaylistGroup_Save {
	ULONG MethodID;
	STRPTR path;
};

/* URLPopString */

struct MP_URLPopString_Insert {
	ULONG MethodID;
	STRPTR url;
};

/* PrefsGroup */

struct MP_PrefsGroup_SelectChange {
	ULONG MethodID;
	LONG listentry;
};

/* ConsoleGroup */

struct MP_ConsoleGroup_AddMessage {
	ULONG MethodID;
	STRPTR message;
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




