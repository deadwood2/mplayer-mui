#include <proto/asl.h>
#include <clib/macros.h>
#include <proto/dos.h>

#include "gui/interface.h"
#include "gui.h"

CONST_STRPTR categories[] = {
	"General",
/*	  "Subtitles",*/
	NULL
};

typedef APTR (*GRPFUNC)(void);

struct prefsgroup
{
	STRPTR name;
	GRPFUNC class;
};

struct prefsgroup prefsgroups[] =
{
	{ "General", getprefsgeneralgroupclass },
/*	  { "Subtitles", getprefssubtitlesgroupclass }, */
	{ NULL, NULL }
};

/******************************************************************
 * playlistgroupclass
 *****************************************************************/

struct PrefsGroupData
{
	APTR LI_Categories;
	APTR GR_Container;
	APTR GR_Content;
	APTR BT_Use;
	APTR BT_Save;
	APTR BT_Cancel;
};

DEFNEW(PrefsGroup)
{
	APTR BT_Save, BT_Cancel, BT_Use, LI_Categories, GR_Container, GR_Content;

	obj = (Object *)DoSuperNew(cl, obj,
		Child, HGroup,
			Child,
				LI_Categories = NewObject(getprefslistclass(), NULL, TAG_DONE),

			Child,
				GR_Container = VGroup,
									MUIA_Frame,      MUIV_Frame_Page,
									MUIA_Background, MUII_PageBack,
									Child, GR_Content = VCenter((TextObject, TextFrame, MUIA_Text_Contents, "Initializing prefs", End)),
									End,
			End,

		Child,
			HGroup,
				Child, BT_Save     = KeyButton("Save",'s'),
				Child, BT_Use      = KeyButton("Use", 'u'),
				Child, BT_Cancel   = KeyButton("Cancel", 'c'),
				End,

		TAG_DONE
	);

	if (obj)
	{
		int i;

		struct PrefsGroupData *data = INST_DATA(cl, obj);

		data->BT_Save = BT_Save;
		data->BT_Use = BT_Use;
		data->BT_Cancel = BT_Cancel;

		data->LI_Categories = LI_Categories;
		data->GR_Container = GR_Container;
		data->GR_Content = GR_Content;

		for (i = 0; categories[i]; i++)
		{
			DoMethod(data->LI_Categories, MUIM_List_InsertSingle, categories[i], MUIV_List_Insert_Bottom);
		}

		DoMethod(data->LI_Categories, MUIM_Notify, MUIA_List_Active, MUIV_EveryTime,
				 obj, 2, MM_PrefsGroup_SelectChange, MUIV_TriggerValue);

		set(data->LI_Categories, MUIA_List_Active, 0);

		set(data->LI_Categories, MUIA_ShowMe, FALSE);

		/* buttons notifications */

		DoMethod(data->BT_Save, MUIM_Notify, MUIA_Pressed, FALSE, obj, 1, MM_PrefsGroup_Save);
		DoMethod(data->BT_Use, MUIM_Notify, MUIA_Pressed, FALSE, obj, 1, MM_PrefsGroup_Use);
		DoMethod(data->BT_Cancel, MUIM_Notify, MUIA_Pressed, FALSE, obj, 1, MM_PrefsGroup_Cancel);
	}

	return (IPTR)obj;
}

DEFDISP(PrefsGroup)
{
	return (DOSUPER);
}

DEFSMETHOD(PrefsGroup_SelectChange)
{
	struct PrefsGroupData *data = INST_DATA(cl, obj);

	DoMethod(data->GR_Container, MUIM_Group_InitChange);

	DoMethod(data->GR_Container, OM_REMMEMBER, data->GR_Content);

	MUI_DisposeObject(data->GR_Content);

	data->GR_Content = NULL;

	if (msg->listentry >= 0)
	{
		data->GR_Content = NewObject(prefsgroups[msg->listentry].class(), NULL, TAG_DONE);
	}

	if (data->GR_Content)
	{
		DoMethod(data->GR_Container, OM_ADDMEMBER, data->GR_Content);
	}
	else
	{
		data->GR_Content = VCenter((TextObject, TextFrame, MUIA_Text_Contents, "Error loading prefs group", End));
	}

	DoMethod(data->GR_Container, MUIM_Group_ExitChange);

	return (0);
}

DEFTMETHOD(PrefsGroup_Update)
{
	struct PrefsGroupData *data = INST_DATA(cl, obj);

	DoMethod(data->GR_Content, MM_PrefsGroup_Update);

	return 0;
}

DEFTMETHOD(PrefsGroup_Save)
{
	struct PrefsGroupData *data = INST_DATA(cl, obj);

	DoMethod(data->GR_Content, MM_PrefsGroup_Store);

	cfg_write();

	set(mygui->prefswindow, MUIA_Window_Open, FALSE);

	return 0;
}

DEFTMETHOD(PrefsGroup_Use)
{
	struct PrefsGroupData *data = INST_DATA(cl, obj);

	DoMethod(data->GR_Content, MM_PrefsGroup_Store);

	set(mygui->prefswindow, MUIA_Window_Open, FALSE);

	return 0;
}

DEFTMETHOD(PrefsGroup_Cancel)
{
	set(mygui->prefswindow, MUIA_Window_Open, FALSE);

	return 0;
}

BEGINMTABLE2(prefsgroupclass)
DECNEW(PrefsGroup)
DECDISP(PrefsGroup)
DECSMETHOD(PrefsGroup_SelectChange)
DECTMETHOD(PrefsGroup_Save)
DECTMETHOD(PrefsGroup_Use)
DECTMETHOD(PrefsGroup_Cancel)
DECTMETHOD(PrefsGroup_Update)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Group, prefsgroupclass, PrefsGroupData)
