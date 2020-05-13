#include "gui/interface.h"
#include "gui.h"

struct PrefsListData {
};

DEFNEW(PrefsList)
{
	obj = (Object *)DoSuperNew(cl, obj,
		InputListFrame,
		MUIA_List_ConstructHook, MUIV_List_ConstructHook_String,
		MUIA_List_DestructHook, MUIV_List_DestructHook_String,
//		  MUIA_List_MinLineHeight, 24, /* XXX: check.. */
		MUIA_List_AdjustWidth, TRUE,
		MUIA_List_AutoVisible, TRUE,
	End;

	return ((IPTR)obj);
}

BEGINMTABLE2(prefslistclass)
DECNEW(PrefsList)
ENDMTABLE

DECSUBCLASS_NC(MUIC_List, prefslistclass, PrefsListData)
