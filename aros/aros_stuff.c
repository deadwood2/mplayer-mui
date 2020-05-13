#ifdef __AROS__

#include <utility/tagitem.h>
#include <proto/intuition.h>
#include <stdlib.h>

IPTR DoSuperNew(struct IClass *cl, Object *obj, ULONG tag1, ...)
{
    AROS_SLOWSTACKTAGS_PRE(tag1)
    retval = (IPTR)DoSuperMethod(cl, obj, OM_NEW, AROS_SLOWSTACKTAGS_ARG(tag1));
    AROS_SLOWSTACKTAGS_POST
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
