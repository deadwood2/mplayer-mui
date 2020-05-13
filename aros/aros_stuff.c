#ifdef __AROS__

#include <utility/tagitem.h>
#include <aros/preprocessor/variadic/cast2iptr.hpp>
#include <proto/alib.h>
#include <stdlib.h>

IPTR DoSuperNew(Class *cl, Object *obj, Tag tag1, ...)
{
    AROS_SLOWSTACKMETHODS_PRE(tag1)
    retval = (IPTR)DoSuperNewTagList(cl, obj, NULL, (struct TagItem *)AROS_SLOWSTACKMETHODS_ARG(tag1));
    AROS_SLOWSTACKMETHODS_POST
}

APTR AllocVecTaskPooled(ULONG byteSize)
{
    return calloc(1, byteSize);
}

VOID FreeVecTaskPooled(APTR memory)
{
    free(memory);
}
#endif
