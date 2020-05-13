#include "gui.h"

#include <libraries/asl.h>

IPTR mui_getv(APTR obj, ULONG attr)
{
	IPTR v;

	GetAttr(attr, obj, &v);
	return (v);
}

APTR MakeButton(CONST_STRPTR msg)
{
	APTR obj;

	if ((obj = MUI_MakeObject(MUIO_Button, (IPTR) msg)))
		SetAttrs(obj, MUIA_CycleChain, TRUE, TAG_DONE);

	return obj;
}

APTR MakeCheck(CONST_STRPTR str, ULONG checked)
{
	APTR obj;

	obj = MUI_MakeObject(MUIO_Checkmark, (IPTR) str);

	if (obj)
		SetAttrs(obj,
			MUIA_CycleChain	, TRUE,
			MUIA_Selected	, checked,
			TAG_DONE);

	return (obj);
}

APTR MakeCycle(CONST_STRPTR label, const CONST_STRPTR *entries)
{
    APTR obj = MUI_MakeObject(MUIO_Cycle, (IPTR)label, (IPTR)entries);

	if(obj)
		SetAttrs(obj, MUIA_CycleChain, 1, MUIA_Weight, 0, TAG_DONE);

    return obj;
}

APTR MakeString(CONST_STRPTR def)
{
	return StringObject, StringFrame, MUIA_CycleChain, 1, MUIA_String_Contents, def, TAG_DONE);
}

APTR MakeNumericString(CONST_STRPTR def)
{
	return StringObject,
			StringFrame,
			MUIA_CycleChain, 1,
			MUIA_String_Contents, def,
			MUIA_String_Accept , "0123456879",
			MUIA_String_Integer, 0,
			TAG_DONE);
}

APTR MakeDirString(CONST_STRPTR str)
{
	APTR obj, pop;

	pop = PopButton(MUII_PopDrawer);

	obj = PopaslObject,
		ASLFR_DrawersOnly, TRUE,
		ASLFR_InitialShowVolumes, TRUE,
		MUIA_Popstring_Button, (IPTR)pop,
		MUIA_Popstring_String, (IPTR)MakeString(str),
		MUIA_Popasl_Type, ASL_FileRequest,
		TAG_DONE);

	if (obj)
		SetAttrs(pop, MUIA_CycleChain, 1, TAG_DONE);

	return obj;
}

APTR MakeFileString(CONST_STRPTR str)
{
	APTR obj, pop;

	pop = PopButton(MUII_PopFile);

	obj = PopaslObject,
		ASLFR_InitialFile, str,
		MUIA_Popstring_Button, (IPTR)pop,
		MUIA_Popstring_String, (IPTR)MakeString(str),
		MUIA_Popasl_Type, ASL_FileRequest,
		TAG_DONE);

	if (obj)
		SetAttrs(pop, MUIA_CycleChain, 1, TAG_DONE);

	return obj;
}
