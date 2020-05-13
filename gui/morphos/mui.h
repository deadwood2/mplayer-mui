#ifndef __MUI_H__

/* MUI helpers */
IPTR mui_getv(APTR o, ULONG a);

#define getv(_o,_a) mui_getv(_o,_a)
#ifndef nnset
#define nnset(obj,attr,value) SetAttrs(obj,MUIA_NoNotify,TRUE,attr,value,TAG_DONE)
#endif

#define _between(_a,_x,_b) ((_x)>=(_a) && (_x)<=(_b))
#define _isinobject(_o,_x,_y) (_between(_left(_o),(_x),_right(_o)) && _between(_top(_o),(_y),_bottom(_o)))

#define INITTAGS (((struct opSet *)msg)->ops_AttrList)

#define FORTAG(_tagp) \
	{ \
		struct TagItem *tag, *_tags = (struct TagItem *)(_tagp); \
		while ((tag = NextTagItem(&_tags))) switch ((int)tag->ti_Tag)
#define NEXTTAG }

#define FORCHILD(_o, _a) \
	{ \
		APTR child, _cstate = (APTR)((struct MinList *)getv(_o, _a))->mlh_Head; \
		while ((child = NextObject(&_cstate)))

#define NEXTCHILD }

#ifndef MUIM_Application_KillPushMethod
#define MUIM_Application_KillPushMethod     0x80429954 /* private */ /* V15 */
struct  MUIP_Application_KillPushMethod     { STACKED LONG MethodID; Object *o; STACKED ULONG id; STACKED ULONG method; };
#endif

#ifndef MUIF_PUSHMETHOD_SINGLE
#define MUIF_PUSHMETHOD_SINGLE       (1<<28UL)
#endif

#undef KeyCheckMark
#define KeyCheckMark(selected,control)\
	ImageObject,\
		ImageButtonFrame,\
		MUIA_InputMode        , MUIV_InputMode_Toggle,\
		MUIA_Image_Spec       , MUII_CheckMark,\
		MUIA_Image_FreeVert   , TRUE,\
		MUIA_Selected         , selected,\
		MUIA_Background       , MUII_ButtonBack,\
		MUIA_ShowSelState     , FALSE,\
		MUIA_ControlChar      , control,\
		MUIA_CycleChain       , 1,\
		End

#undef KeyButton
#define KeyButton(name,key)\
	TextObject,\
		ButtonFrame,\
		MUIA_Font, MUIV_Font_Button,\
		MUIA_Text_Contents, name,\
		MUIA_Text_PreParse, "\33c",\
		MUIA_Text_HiChar  , key,\
		MUIA_ControlChar  , key,\
		MUIA_InputMode    , MUIV_InputMode_RelVerify,\
		MUIA_Background   , MUII_ButtonBack,\
		MUIA_CycleChain   , 1,\
		End

#define PictureButton(name, imagepath)\
		NewObject(getpicturebuttonclass(), NULL,\
				MA_PictureButton_Name, name,\
				MA_PictureButton_Path, imagepath,\
				MA_PictureButton_UserData, NULL,\
				MUIA_ShortHelp, name,\
				TAG_DONE)

APTR MakeButton(CONST_STRPTR msg);
APTR MakeString(CONST_STRPTR def);
APTR MakeCheck(CONST_STRPTR str, ULONG checked);
APTR MakeCycle(CONST_STRPTR label, const CONST_STRPTR *entries);
APTR MakeNumericString(CONST_STRPTR def);
APTR MakeFileString(CONST_STRPTR str);
APTR MakeDirString(CONST_STRPTR str);

#endif

