#include <clib/macros.h>

#include "morphos_stuff.h"

#include "gui/interface.h"
#include "gui.h"

/******************************************************************
 * consolegroupclass
 *****************************************************************/

struct ConsoleGroupData
{
	APTR LI_Messages;
	APTR BT_Clear;
};

DEFNEW(ConsoleGroup)
{
	APTR LI_Messages, BT_Clear;

	obj = (Object *)DoSuperNew(cl, obj,
		Child,
			LI_Messages = NewObject(getconsolelistclass(), NULL, TAG_DONE),

		Child,
			VGroup, MUIA_Weight, 0,
				Child, BT_Clear = MakeButton("_Clear"),
				End,

		TAG_DONE
	);

	if (obj)
	{
		struct ConsoleGroupData *data = INST_DATA(cl, obj);

		data->LI_Messages = LI_Messages;
		data->BT_Clear    = BT_Clear;

		/* buttons notifications */

		DoMethod(data->BT_Clear, MUIM_Notify, MUIA_Pressed, FALSE,
				 obj, 1, MM_ConsoleGroup_Clear);

	}

	return (IPTR)obj;
}

DEFDISP(ConsoleGroup)
{
	return (DOSUPER);
}

DEFSMETHOD(ConsoleGroup_AddMessage)
{
	struct ConsoleGroupData *data = INST_DATA(cl, obj);

	DoMethod(data->LI_Messages, MUIM_List_InsertSingle, msg->message, MUIV_List_Insert_Bottom);
	set(data->LI_Messages, MUIA_List_Active, MUIV_List_Active_Bottom);

	free(msg->message); // Allocated by a pushmethod call, so free it here.

	return 0;
}

DEFTMETHOD(ConsoleGroup_Clear)
{
	struct ConsoleGroupData *data = INST_DATA(cl, obj);
	DoMethod(data->LI_Messages, MUIM_List_Clear);
	return 0;
}

BEGINMTABLE2(consolegroupclass)
DECNEW(ConsoleGroup)
DECDISP(ConsoleGroup)
DECSMETHOD(ConsoleGroup_AddMessage)
DECTMETHOD(ConsoleGroup_Clear)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Group, consolegroupclass, ConsoleGroupData)
