#include <proto/asl.h>
#include <clib/macros.h>

#include "gui/interface.h"
#include "gui.h"

/******************************************************************
 * dvddirgroupclass
 *****************************************************************/

struct DVDDirGroupData
{
	APTR BT_OK;
	APTR BT_Cancel;
	APTR ST_Dir;
};

DEFNEW(DVDDirGroup)
{
	APTR BT_OK, BT_Cancel, PO_Dir, ST_Dir;

	obj = (Object *)DoSuperNew(cl, obj,
		Child,
			TextObject,
			  MUIA_Text_Contents, "A DVD can be played from hard disk by selecting the directory\n"
								  "containing VIDEO_TS and AUDIO_TS directories.",
			  End,
		Child,
			PO_Dir = PopaslObject,
						MUIA_Popstring_String, ST_Dir = String("", 8192),
						MUIA_Popstring_Button, PopButton(MUII_PopFile),
						ASLFR_TitleText, "Select a directory...",
						ASLFR_DrawersOnly, TRUE,
						End,
		Child,
			HGroup,
				Child, BT_OK = KeyButton("Ok", 'o'),
				Child, HSpace(0),
				Child, BT_Cancel = KeyButton("Cancel", 'c'),
				End,

		TAG_DONE
	);

	if (obj)
	{
		struct DVDDirGroupData *data = INST_DATA(cl, obj);

		data->BT_OK = BT_OK;
		data->BT_Cancel = BT_Cancel;
		data->ST_Dir = ST_Dir;

		set(data->ST_Dir, MUIA_CycleChain, 1);

		DoMethod(data->BT_OK, MUIM_Notify, MUIA_Pressed, FALSE,
				 obj, 1, MM_DVDDirGroup_Open);

		DoMethod(data->BT_Cancel, MUIM_Notify, MUIA_Pressed, FALSE,
				 obj, 1, MM_DVDDirGroup_Cancel);

		DoMethod(data->ST_Dir, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime,
				 obj, 1, MM_DVDDirGroup_Open);
	}

	return (IPTR)obj;
}

DEFTMETHOD(DVDDirGroup_Open)
{
	struct DVDDirGroupData *data = INST_DATA(cl, obj);

	STRPTR dir = (STRPTR) getv(data->ST_Dir, MUIA_String_Contents);

	DoMethod(mygui->maingroup, MM_MPlayerGroup_Open, MV_MPlayerGroup_Open_DVD_Directory, dir);

	set(mygui->dvddirwindow, MUIA_Window_Open, FALSE);

	return 0;
}

DEFTMETHOD(DVDDirGroup_Cancel)
{
	set(mygui->dvddirwindow, MUIA_Window_Open, FALSE);

	return 0;
}

BEGINMTABLE2(dvddirgroupclass)
DECNEW(DVDDirGroup)
DECTMETHOD(DVDDirGroup_Open)
DECTMETHOD(DVDDirGroup_Cancel)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Group, dvddirgroupclass, DVDDirGroupData)
