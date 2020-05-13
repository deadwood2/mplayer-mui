#include <clib/macros.h>

#include "command.h"

#include "gui/interface.h"
#include "gui.h"

/******************************************************************
 * scalegroupclass
 *****************************************************************/

#define PROPERTY(attribute) property_expand_string(guiInfo.mpcontext, attribute)

struct ScaleGroupData
{
	APTR BT_OK;
	APTR BT_Cancel;
	APTR BT_Reset;
	APTR SL_Width;
	APTR ST_Width;
	APTR SL_Height;
	APTR ST_Height;
};

DEFNEW(ScaleGroup)
{
	APTR BT_OK, BT_Cancel, BT_Reset, ST_Width, ST_Height, ST_Left, ST_Top;
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
			ColGroup(2),
				Child, TextObject, MUIA_Text_Contents, "Width:", End,
				Child, ST_Width = MakeNumericString(str_width),
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
		struct ScaleGroupData *data = INST_DATA(cl, obj);

		data->BT_OK     = BT_OK;
		data->BT_Reset  = BT_Reset;
		data->BT_Cancel = BT_Cancel;
		data->ST_Width  = ST_Width;
		data->ST_Height = ST_Height;

		DoMethod(data->BT_OK, MUIM_Notify, MUIA_Pressed, FALSE,
				 obj, 1, MM_ScaleGroup_Apply);

		DoMethod(data->BT_Reset, MUIM_Notify, MUIA_Pressed, FALSE,
				 obj, 1, MM_ScaleGroup_Reset);

		DoMethod(data->BT_Cancel, MUIM_Notify, MUIA_Pressed, FALSE,
				 obj, 1, MM_ScaleGroup_Cancel);
	}

	return (IPTR)obj;
}

DEFTMETHOD(ScaleGroup_UpdateDimensions)
{
	struct ScaleGroupData *data = INST_DATA(cl, obj);

	set(data->ST_Width, MUIA_String_Integer, guiInfo.VideoWidth);
	set(data->ST_Height, MUIA_String_Integer, guiInfo.VideoHeight);

	return 0;
}

DEFTMETHOD(ScaleGroup_Apply)
{
	struct ScaleGroupData *data = INST_DATA(cl, obj);

	LONG width  = getv(data->ST_Width, MUIA_String_Integer);
	LONG height = getv(data->ST_Height, MUIA_String_Integer);

	DoMethod(mygui->maingroup, MM_MPlayerGroup_Scale, width, height, TRUE);

	set(mygui->scalewindow, MUIA_Window_Open, FALSE);

	return 0;
}

DEFTMETHOD(ScaleGroup_Reset)
{
	DoMethod(mygui->maingroup, MM_MPlayerGroup_Scale, 0, 0, FALSE);

	set(mygui->scalewindow, MUIA_Window_Open, FALSE);

	return 0;
}

DEFTMETHOD(ScaleGroup_Cancel)
{
	set(mygui->scalewindow, MUIA_Window_Open, FALSE);

	return 0;
}

BEGINMTABLE2(scalegroupclass)
DECNEW(ScaleGroup)
DECTMETHOD(ScaleGroup_Apply)
DECTMETHOD(ScaleGroup_Reset)
DECTMETHOD(ScaleGroup_Cancel)
DECTMETHOD(ScaleGroup_UpdateDimensions)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Group, scalegroupclass, ScaleGroupData)
