#include <clib/macros.h>

#include "gui/interface.h"
#include "gui.h"

/******************************************************************
 * consolelistclass
 *****************************************************************/

struct ConsoleListEntry
{
	char *message;
};

struct ConsoleListData
{
};

DEFNEW(ConsoleList)
{
	obj = (Object *)DoSuperNew(cl, obj,
					MUIA_Frame, MUIV_Frame_InputList,
					MUIA_CycleChain, 1,
					MUIA_List_Format, "MIW=-1 MAW=-2",
					MUIA_Listview_MultiSelect, MUIV_Listview_MultiSelect_None,
					MUIA_List_AutoVisible, TRUE,
					End;

	if(obj)
	{
		struct ConsoleListData *data = INST_DATA(cl, obj);
	}

	return (IPTR)obj;
}

DEFDISP(ConsoleList)
{
	return (DOSUPER);
}

DEFMMETHOD(List_Display)
{
	struct ConsoleListData *data = INST_DATA(cl, obj);
	struct ConsoleListEntry *entry = (struct ConsoleListEntry *) msg->entry;

	if (entry)
	{
		LONG pos = (LONG) msg->array[-1];

		if(pos%2)
		{
			msg->array[ -9 ] = (STRPTR) 10;
		}

		msg->array[0] = entry->message;
	}

	return 0;
}

DEFMMETHOD(List_Construct)
{
	char * message = (char *) msg->entry;
	struct ConsoleListEntry * new_entry = NULL;

	if(message)
	{
		new_entry = (struct ConsoleListEntry *) AllocVecPooled(msg->pool, sizeof(struct ConsoleListEntry));

		if(new_entry)
		{
			memset(new_entry, 0, sizeof(struct ConsoleListEntry));
			new_entry->message = (char *) AllocVecPooled(msg->pool, strlen(message) + 1);

			if(new_entry->message)
			{
				stccpy(new_entry->message, message, strlen(message) + 1);
			}
		}
	}

	return (IPTR) new_entry;
}

DEFMMETHOD(List_Destruct)
{
	struct ConsoleListEntry *entry = (struct ConsoleListEntry *) msg->entry;

	if(entry)
	{
		if(entry->message)
			FreeVecPooled(msg->pool, (void *) entry->message);
		FreeVecPooled(msg->pool, (void *) entry);
	}

	return 0;
}

BEGINMTABLE2(consolelistclass)
DECNEW(ConsoleList)
DECDISP(ConsoleList)
DECMMETHOD(List_Display)
DECMMETHOD(List_Construct)
DECMMETHOD(List_Destruct)
ENDMTABLE

DECSUBCLASS_NC(MUIC_List, consolelistclass, ConsoleListData)
