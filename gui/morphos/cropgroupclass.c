#include <clib/macros.h>

#include "command.h"

#include "gui/interface.h"
#include "gui.h"

/******************************************************************
 * cropgroupclass
 *****************************************************************/

#define PROPERTY(attribute) property_expand_string(guiInfo.mpcontext, attribute)

struct CropGroupData
{
	APTR BT_OK;
	APTR BT_Reset;
	APTR BT_Cancel;
	APTR SL_Width;
	APTR ST_Width;
	APTR SL_Height;
	APTR ST_Height;
	APTR SL_Left;
	APTR ST_Left;
	APTR SL_Top;
	APTR ST_Top;
};

DEFNEW(CropGroup)
{
	APTR BT_OK, BT_Reset, BT_Cancel, ST_Width, ST_Height, ST_Left, ST_Top;
	LONG width = 0, height = 0;
	char str_width[32], str_height[32];

	if(guiInfo.mpcontext)
	{
		width  = (LONG) PROPERTY("${width}");
		height = (LONG) PROPERTY("${height}");
	}

	snprintf(str_width, sizeof(str_width), "%ld", width);
	snprintf(str_height, sizeof(str_height), "%ld", height);

	obj = (Object *)DoSuperNew(cl, obj,
		Child,
			ColGroup(4),
				Child, TextObject, MUIA_Text_Contents, "Left:", End,
				Child, ST_Left = MakeNumericString("0"),
				Child, TextObject, MUIA_Text_Contents, "Width:", End,
				Child, ST_Width = MakeNumericString(str_width),
				Child, TextObject, MUIA_Text_Contents, "Top:", End,
				Child, ST_Top = MakeNumericString("0"),
				Child, TextObject, MUIA_Text_Contents, "Height:", End,
				Child, ST_Height = MakeNumericString(str_height),
				End,
		Child,
			HGroup,
				Child, BT_OK = KeyButton("Apply", 'a'),
				Child, HSpace(0),
				Child, BT_Reset = KeyButton("Reset", 'r'),
				Child, HSpace(0),
				Child, BT_Cancel = KeyButton("Cancel", 'c'),
				End,

		TAG_DONE
	);

	if (obj)
	{
		struct CropGroupData *data = INST_DATA(cl, obj);

		data->BT_OK     = BT_OK;
		data->BT_Reset  = BT_Reset;
		data->BT_Cancel = BT_Cancel;
		data->ST_Left   = ST_Left;
		data->ST_Top    = ST_Top;
		data->ST_Width  = ST_Width;
		data->ST_Height = ST_Height;

		DoMethod(data->BT_OK, MUIM_Notify, MUIA_Pressed, FALSE,
				 obj, 1, MM_CropGroup_Apply);

		DoMethod(data->BT_Reset, MUIM_Notify, MUIA_Pressed, FALSE,
				 obj, 1, MM_CropGroup_Reset);

		DoMethod(data->BT_Cancel, MUIM_Notify, MUIA_Pressed, FALSE,
				 obj, 1, MM_CropGroup_Cancel);
	}

	return (IPTR)obj;
}

DEFTMETHOD(CropGroup_UpdateDimensions)
{
	struct CropGroupData *data = INST_DATA(cl, obj);

	set(data->ST_Width, MUIA_String_Integer, guiInfo.VideoWidth);
	set(data->ST_Height, MUIA_String_Integer, guiInfo.VideoHeight);

	return 0;
}

DEFTMETHOD(CropGroup_Apply)
{
	struct CropGroupData *data = INST_DATA(cl, obj);

	LONG left   = getv(data->ST_Left, MUIA_String_Integer);
	LONG top    = getv(data->ST_Top, MUIA_String_Integer);
	LONG width  = getv(data->ST_Width, MUIA_String_Integer);
	LONG height = getv(data->ST_Height, MUIA_String_Integer);

	DoMethod(mygui->maingroup, MM_MPlayerGroup_Crop, width, height, left, top, TRUE);

	set(mygui->cropwindow, MUIA_Window_Open, FALSE);

	return 0;
}

DEFTMETHOD(CropGroup_Reset)
{
	DoMethod(mygui->maingroup, MM_MPlayerGroup_Crop, 0, 0, 0, 0, FALSE);

	set(mygui->cropwindow, MUIA_Window_Open, FALSE);

	return 0;
}

DEFTMETHOD(CropGroup_Cancel)
{
	set(mygui->cropwindow, MUIA_Window_Open, FALSE);

	return 0;
}

BEGINMTABLE2(cropgroupclass)
DECNEW(CropGroup)
DECTMETHOD(CropGroup_Apply)
DECTMETHOD(CropGroup_Reset)
DECTMETHOD(CropGroup_Cancel)
DECTMETHOD(CropGroup_UpdateDimensions)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Group, cropgroupclass, CropGroupData)
