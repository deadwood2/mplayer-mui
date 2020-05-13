#include "thread.h"

#include <string.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dostags.h>

struct Thread *	RunThread(int (*func) (void), const char * threadname, APTR obj, APTR arg, ULONG argsize)
{
	struct Thread * thread = (struct Thread *) AllocVec(sizeof(*thread), MEMF_PUBLIC);

	if(thread)
	{
		thread->finished = FALSE;
		thread->replyport = CreateMsgPort();
		if (thread->replyport)
		{
			thread->msg = AllocVec(sizeof(struct startupmsg), MEMF_PUBLIC);
			if (thread->msg)
			{
				struct MsgPort *port;

				thread->msg->obj = obj;
				if(arg)
				{
					thread->msg->arg  = AllocVec(argsize, MEMF_PUBLIC);
					if(thread->msg->arg)
					{
						memcpy(thread->msg->arg, arg, argsize);
					}
				}
				else
				{
					thread->msg->arg = NULL;
				}

				thread->msg->msg.mn_Node.ln_Type = NT_MESSAGE;
				thread->msg->msg.mn_ReplyPort    = thread->replyport;
				thread->msg->msg.mn_Length       = sizeof(struct startupmsg);

#if !defined(__AROS__)
				thread->task = CreateNewProcTags(NP_CodeType,    CODETYPE_PPC,
#else
                thread->task = CreateNewProcTags(
#endif
										 NP_Name,		 (IPTR) threadname,
										 NP_Entry,       (IPTR) func,
#if !defined(__AROS__)
										 NP_StartupMsg,  (ULONG) thread->msg,
										 NP_TaskMsgPort, (ULONG) &port,
#else
                                         NP_UserData,    (IPTR) thread->msg,
#endif
										 NP_CloseOutput, FALSE,
										 NP_Output,      Output(),
										 NP_CloseInput,  FALSE,
										 NP_Input,       Input(),
										 NP_Priority,    (LONG) 1,
#if !defined(__AROS__)
										 NP_PPCStackSize, 65536,
#else
                                         NP_StackSize,   65536,
#endif
										 TAG_DONE);
			}
		}

		if(!thread->task)
		{
			if(thread->msg)
			{
				if(thread->msg->arg)
				{
					FreeVec(thread->msg->arg);
				}

				FreeVec(thread->msg);
				thread->msg = NULL;
			}

			if(thread->replyport)
			{
				DeleteMsgPort(thread->replyport);
				thread->replyport = NULL;
			}

			FreeVec(thread);
			thread = NULL;
		}
	}

	return thread;
}

int CheckThreadAbort(void)
{
	if (SetSignal(0L, 0L) & SIGBREAKF_CTRL_E)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

void SignalThread(struct Thread * t)
{
	if(t && !t->finished)
	{
		Signal((struct Task *)t->task, SIGBREAKF_CTRL_E);
	}
}

void WaitForThread(struct Thread * t)
{
	if(t)
	{
		WaitPort(t->replyport);
		while(GetMsg(t->replyport));

		if(t->msg->arg)
		{
			FreeVec(t->msg->arg);
		}

		FreeVec(t->msg);

		DeleteMsgPort(t->replyport);

		FreeVec(t);
	}
}
