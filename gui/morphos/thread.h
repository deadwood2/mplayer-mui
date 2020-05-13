#ifndef __THREAD_H__
#define __THREAD_H__

#include "global.h"

#include <exec/exec.h>
#include <exec/types.h>
#include <dos/dos.h>

struct Thread
{
	struct startupmsg *msg;
	struct startupmsg *reply;
	struct MsgPort *replyport;
	struct Process * task;
	ULONG  finished;
};

struct startupmsg
{
	struct Message msg;
	APTR obj;
	APTR arg;
};

struct Thread *	RunThread(int (*func) (void), const char * threadname, APTR obj, APTR arg, ULONG argsize);
int CheckThreadAbort(void);
void SignalThread(struct Thread * t);
void WaitForThread(struct Thread * t);

#endif
