#include <clib/macros.h>
#include <proto/dos.h>

#include "poplistclass.h"
#include "gui/interface.h"
#include "gui.h"

/******************************************************************
 * poplistclass
 *****************************************************************/

struct PopListData
{
};

DEFNEW(PopList)
{
	obj = (Object *)DoSuperNew(cl, obj,
					InputListFrame,
					End;

	return (IPTR)obj;
}

DEFDISP(PopList)
{
	return (DOSUPER);
}

DEFMMETHOD(List_Display)
{
	struct PopListEntry *entry = (struct PopListEntry *) msg->entry;

	if (entry)
	{
		msg->array[0] = entry->label;
	}

	return 0;
}

DEFMMETHOD(List_Construct)
{
	char * label = (char *)  msg->entry;
	struct PopListEntry * new_entry = (struct PopListEntry *) AllocVecPooled(msg->pool, sizeof(struct PopListEntry));

	if (new_entry)
	{
		stccpy(new_entry->label, label, sizeof(new_entry->label));
	}
	return (IPTR) new_entry;
}

DEFMMETHOD(List_Destruct)
{
	struct PopListEntry *entry = (struct PopListEntry *) msg->entry;

	if (entry)
	{
		FreeVecPooled(msg->pool, (void *) entry);
	}

	return 0;
}

BEGINMTABLE2(poplistclass)
DECNEW(PopList)
DECDISP(PopList)
DECMMETHOD(List_Display)
DECMMETHOD(List_Construct)
DECMMETHOD(List_Destruct)
ENDMTABLE

DECSUBCLASS_NC(MUIC_List, poplistclass, PopListData)
