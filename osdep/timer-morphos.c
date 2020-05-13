/* 
 *  timer_amiga.c
 *  Precise timer routines for MorphOS/ABOX
 *  Writen by DET Nicolas
 *  (C) Genesi 2003
*/

#include <proto/dos.h>
#include <proto/timer.h>
#include <proto/exec.h>

#include "../config.h"

#include <proto/timer.h>
extern struct timerequest *TimerRequest; // declared and init in mplayer.c

int usec_sleep(int usec_delay)
{
    struct timeval tv = {0, usec_delay};
    TimerRequest->tr_node.io_Command= TR_ADDREQUEST;
    TimerRequest->tr_time = tv;
    DoIO ( (struct IORequest *) TimerRequest);

    return (int) usec_delay;
}

// Returns current time in microseconds
unsigned int GetTimer(void){

   struct timeval tv;
   GetSysTime (&tv);
   return tv.tv_secs*1000000+tv.tv_micro;

}  

// Returns current time in milliseconds
unsigned int GetTimerMS(void){
   struct timeval tv;
   GetSysTime (&tv);
   return tv.tv_secs*1000+tv.tv_micro/1000;
}  

static unsigned int RelativeTime=0;

// Returns time spent between now and last call in seconds
float GetRelativeTime(void){
unsigned int t,r;
  t=GetTimer();
//  t*=16;printf("time=%ud\n",t);
  r=t-RelativeTime;
  RelativeTime=t;
  return (float)r * 0.000001F;
}

// Initialize timer, must be called at least once at start
void InitTimer(void){
  GetRelativeTime();
}

#if 0
void main(){
  float t=0;
  InitTimer();
  while(1){ t+=GetRelativeTime();printf("time= %10.6f\r",t);fflush(stdout); }
}
#endif

