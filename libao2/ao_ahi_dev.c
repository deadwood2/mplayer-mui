#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <osdep/timer.h>

#include "mp_msg.h"

// NOTE: all OS Specific stuff have not been protected by #ifndef ... #else .. #endif
// because il you compile ahi, you are using on MorphOS (or AmigaOS)

// Some OS Specific include
#include <proto/alib.h>
#include <proto/ahi.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include <dos/dostags.h>
#include <dos/dos.h>

#ifndef __AROS__
#include <hardware/byteswap.h>
#endif

#ifdef __AROS__
#define SWAPWORD(w) AROS_SWAP_BYTES_WORD(w)
#define SWAPLONG(l) AROS_SWAP_BYTES_LONG(l)
#endif
// End OS Specific

#include "../libaf/af_format.h"
#include "libavutil/libm.h"
#include "audio_out.h"
#include "audio_out_internal.h"

// DEBUG
#define kk(x)

//some define for this nice AHI driver !
#define AHI_CHUNKSIZE         65536
#define AHI_CHUNKMAX          131072
#define AHI_DEFAULTUNIT       0


/****************************************************************************/

static struct MsgPort *     AHIMsgPort  = NULL;     // The msg port we will use for the ahi.device
static struct AHIRequest *  AHIio       = NULL;     // First AHIRequest structure
static struct AHIRequest *  AHIio2      = NULL;     // second one. Double buffering !
static struct AHIRequest *  AHIio_orig       = NULL;     // First AHIRequest structure
static struct AHIRequest *  AHIio2_orig      = NULL;     // second one. Double buffering !
static struct AHIRequest *link=NULL;
static BYTE AHIDevice                   = -1;        // Is our AHI device opened ? 1 -> no, 0 -> Yes
static ULONG AHIType;
static LONG AHI_Volume=0x10000;

// some AmigaOS Signals
static BYTE sig_taskready=-1;
static BYTE sig_taskend=-1;
 
static struct Process *PlayerTask_Process = NULL;
static struct Task    *MainTask = NULL;           // Pointer to the main task;

#define PLAYERTASK_NAME       "MPlayer Audio Thread"
#define PLAYERTASK_PRIORITY   2

static unsigned int tv_write = 0;
static float audio_bps = 1.0f;

static BOOL running=TRUE; // If false -> PlayerTask exit !

// Buffer management
static struct SignalSemaphore LockSemaphore;
static BOOL buffer_full=FALSE;     // Indicate if the buffer is full or not
static ULONG buffer_get=0; // place where we have to get the buffer to play
static ULONG buffer_put=0; // Place where we have to put the buffer we will play


static unsigned int written_byte=0;
static BYTE *buffer=NULL;    // Our audio buffer

typedef enum
{
	CO_NONE,
	CO_8_U2S,
	CO_16_U2S,
	CO_16_LE2BE,
	CO_16_LE2BE_U2S,
	CO_24_BE2BE,
	CO_24_LE2BE,
	CO_32_LE2BE,
	CO_FLOAT_S32BE,
	CO_FLOAT_S32LE,
	CO_FLOAT_S16BE,
	CO_FLOAT_S16LE
} convop_t;
convop_t convop;

//Custom function internal to this driver
inline static void cleanup(void); // Clean !
static void PlayerTask (void);

static ao_info_t info = 
{
#if defined(__AROS__)
   "AHI audio output using high-level API (AROS)",
#elif defined(__MORPHOS__)
   "AHI audio output using high-level API (MorphOS)",
#else
   "AHI audio output using high-level API (AmigaOS)",
#endif
   "ahi_dev",
   "DET Nicolas",
#if defined(__AROS__)
   "AROS No schedule 'n' rocking"
#elif defined(__MORPHOS__)
   "MorphOS Rulez :-)"
#else
   "Amiga Survivor !",
#endif
};

// define the standart functions that mplayer will use.
// see define in audio_aout_internal.h
LIBAO_EXTERN(ahi_dev)

/*******************************************************************/
static void cleanup_task(void) {

   kk(KPrintF("Closing device\n");)
   if (! AHIDevice )
   {
	if (link)
	{
	  AbortIO( (struct IORequest *) link);
	  WaitIO( (struct IORequest *) link);
	}
	  CloseDevice ( (struct IORequest *) AHIio_orig);
   }

   kk(KPrintF("Deleteting AHIio_orig\n");)
   if (AHIio_orig) DeleteIORequest( (struct IORequest *)AHIio_orig);

   kk(KPrintF("Freeing AHIio2_orig\n");)
   if (AHIio2_orig) FreeMem( AHIio2_orig,  sizeof(struct AHIRequest) );

   kk(KPrintF("Deleting AHIMsgPort\n");)
   if (AHIMsgPort) DeleteMsgPort(AHIMsgPort);

   kk(KPrintF("Freeing Buffer\n");)
   if (buffer) FreeVec(buffer );

   kk(KPrintF("The end\n");)
   link = NULL;
   buffer = NULL;
   AHIMsgPort = NULL;
   AHIio = NULL;
   AHIio_orig = NULL;
   AHIio2 = NULL;
   AHIio2_orig = NULL;
   AHIDevice = -1;       // -1 -> Not opened !
}

/*******************************************************************/
static void cleanup(void) {
	if (buffer) 
	{
		FreeVec(buffer);
		buffer = NULL;
	}
	if (sig_taskready != -1)
	{
		FreeSignal(sig_taskready);
		sig_taskready = -1;
	}

	if (sig_taskend != -1)
	{
		FreeSignal(sig_taskend);
		sig_taskend = -1;
	}

	buffer_get=0;
	buffer_put=0;
	written_byte=0;
}

/******************** PLAYER TASK ********************/
static void PlayerTask (void) {
int buffer_to_play_len;

// for AHI
struct AHIRequest *tempRequest;

// Code starts here
kk(KPrintF("TASK: *********************************************\n");)
kk(KPrintF("TASK: PlayerTask starts\n");)


kk(KPrintF("TASK: sig_taskready: %d   sig_taskready %d\n", sig_taskready, sig_taskend );)

// Init AHI

   if( ( AHIMsgPort=CreateMsgPort() )) {
	  if ( (AHIio=(struct AHIRequest *)CreateIORequest(AHIMsgPort,sizeof(struct AHIRequest)))) {
		 AHIio->ahir_Version = 4;
		 AHIDevice=OpenDevice(AHINAME, AHI_DEFAULTUNIT,(struct IORequest *)AHIio,NULL);
	  }
   }

   if(AHIDevice) {
	  kk(KPrintF("Unable to open %s/0 version 4\n",AHINAME);)
	  goto end_playertask;
   }

   // Make a copy of the request (for double buffering)

   AHIio2 = AllocMem(sizeof(struct AHIRequest), MEMF_ANY);
   if(! AHIio2) {
	  goto end_playertask;
   }
  
   // Backup original pointer 
   AHIio_orig = AHIio;
   AHIio2_orig= AHIio2;

   memcpy(AHIio2, AHIio, sizeof(struct AHIRequest));

   kk(KPrintF("TASK: Sends task ready signal to MainTask\n");)
   Signal( (struct Task *) MainTask, 1L << sig_taskready );

kk(KPrintF("TASK: Starting main loop\n");)
while(running) {

   ObtainSemaphore(&LockSemaphore); 
   kk(KPrintF("TASK: Semaphore obtained\n");)

   buffer_to_play_len =  buffer_put - buffer_get;
   if ( (!buffer_to_play_len) && (!buffer_full)  ) {
			kk(KPrintF("TASK: BufferEmpty\n");) 
			ReleaseSemaphore(&LockSemaphore); 
			kk(KPrintF("TASK: Semaphore released\n");)
			Delay(1);
			continue; 
   } // buffer empty -> do nothing
   

   if ( buffer_to_play_len > 0) {
	  kk(KPrintF("TASK: buffer_get: %d buffer_to_play_len: %d\n", buffer_get, buffer_to_play_len);)
	  //if (buffer_to_play_len > AHI_CHUNKSIZE) buffer_to_play_len = AHI_CHUNKSIZE;

	  AHIio->ahir_Std.io_Message.mn_Node.ln_Pri = 0;
	  AHIio->ahir_Std.io_Command  = CMD_WRITE;
	  AHIio->ahir_Std.io_Offset   = 0;
	  AHIio->ahir_Frequency       = (ULONG) ao_data.samplerate;
	  AHIio->ahir_Volume          = AHI_Volume;       
	  AHIio->ahir_Position        = 0x8000;           // Centered
	  AHIio->ahir_Std.io_Data     = buffer + buffer_get; 
	  AHIio->ahir_Std.io_Length   = (ULONG) buffer_to_play_len; 
	  AHIio->ahir_Type = AHIType;
	  AHIio->ahir_Link            = link;     

   }
   else {
	  kk(KPrintF("TASK: buffer_get: %d buffer_to_play_len: %d\n", buffer_get, AHI_CHUNKMAX - buffer_get);)
	  buffer_to_play_len = AHI_CHUNKMAX - buffer_get;
	  if (buffer_to_play_len > AHI_CHUNKSIZE) buffer_to_play_len = AHI_CHUNKSIZE;

	  AHIio->ahir_Std.io_Message.mn_Node.ln_Pri = 0;
	  AHIio->ahir_Std.io_Command  = CMD_WRITE;
	  AHIio->ahir_Std.io_Offset   = 0;
	  AHIio->ahir_Frequency       = (ULONG) ao_data.samplerate;
	  AHIio->ahir_Volume          = AHI_Volume;       
	  AHIio->ahir_Position        = 0x8000;           // Centered
	  AHIio->ahir_Std.io_Data     = buffer + buffer_get; 
	  AHIio->ahir_Std.io_Length   = (ULONG) buffer_to_play_len;
	  AHIio->ahir_Link            = link;
	  AHIio->ahir_Type = AHIType;
   }

   buffer_get += buffer_to_play_len;
   buffer_get %= AHI_CHUNKMAX;

   kk(KPrintF("TASK: Semaphore released (next line)\n");)
   ReleaseSemaphore(&LockSemaphore);
   //tv_write = GetTimerMS();
   SendIO( (struct IORequest *) AHIio);

   buffer_full=FALSE;
   written_byte -= buffer_to_play_len;    

   if (link) {
	  WaitIO ( (struct IORequest *) link);

	  if (CheckIO((struct IORequest *)AHIio))
	  {
		/* Playback caught up with us, rebuffer... */
		WaitIO((struct IORequest *)AHIio);
		link = NULL;
		continue;
	  }
   }

   link = AHIio;

   // Swap requests
   tempRequest = AHIio;
   AHIio  = AHIio2;
   AHIio2 = tempRequest;


} // while(running)

end_playertask:

kk(KPrintF("Starting Cleanup\n");)
cleanup_task();
kk(KPrintF("Cleanup OK\n");)
Signal( (struct Task *) MainTask, 1L << sig_taskend );

}



/***************************** CONTROL *******************************/
// to set/get/query special features/parameters
static int control(int cmd, void *arg)
{
	switch (cmd)
	{
		case AOCONTROL_GET_VOLUME:
		{
			ao_control_vol_t* vol = (ao_control_vol_t*)arg;
			vol->left = vol->right =  ( (float) (AHI_Volume >> 8 ) ) / 2.56 ;
			return CONTROL_OK;
		}

		case AOCONTROL_SET_VOLUME:
		{
			float diff;
			ao_control_vol_t* vol = (ao_control_vol_t*)arg;
			diff = (vol->left+vol->right) / 2;
			AHI_Volume =  ( (int) (diff * 2.56) ) << 8 ;
			return CONTROL_OK;
		}
	}
	return CONTROL_UNKNOWN;
}

/***************************** INIT **********************************/
// open & setup audio device
// return: 1=success 0=fail
static int init(int rate,int channels,int format,int flags){
	  // Init ao.data to make mplayer happy :-)  
   ao_data.channels=      channels;
   ao_data.samplerate=    rate;
   ao_data.format=        format;
   ao_data.bps=           channels*rate;
   if (format != AF_FORMAT_U8 && format != AF_FORMAT_S8)
   {
	 ao_data.bps*=2;
	 if (format == AF_FORMAT_S24_BE || format == AF_FORMAT_S24_LE ||
		 format == AF_FORMAT_S32_BE || format == AF_FORMAT_S32_LE ||
		 format == AF_FORMAT_FLOAT_BE || format == AF_FORMAT_FLOAT_LE)
	   ao_data.bps*=2;
   }
   audio_bps = (float)ao_data.bps;

	   // Check if we are able to do that !
   switch ( format ) {
	  case AF_FORMAT_U8:
		 convop  = CO_8_U2S;
		 AHIType = channels > 1 ? AHIST_S8S : AHIST_M8S;
		 break;
	  case AF_FORMAT_S8:
		 convop  = CO_NONE;
		 AHIType = channels > 1 ? AHIST_S8S : AHIST_M8S;
		 break;
	  case AF_FORMAT_U16_BE:
		 convop  = CO_16_U2S;
		 AHIType = channels > 1 ? AHIST_S16S : AHIST_M16S;
		 break;
	  case AF_FORMAT_S16_BE:
#if AROS_BIG_ENDIAN || !defined(__AROS__)
         convop  = CO_NONE;
#else
         convop  = CO_16_LE2BE;
#endif
		 AHIType = channels > 1 ? AHIST_S16S : AHIST_M16S;
		 break;
	  case AF_FORMAT_U16_LE:
		 convop  = CO_16_LE2BE_U2S; /* FIXME: Different conversion needed for i386 AROS */
		 AHIType = channels > 1 ? AHIST_S16S : AHIST_M16S;
		 break;
	  case AF_FORMAT_S16_LE:
#if AROS_BIG_ENDIAN || !defined(__AROS__)
         convop  = CO_16_LE2BE;
#else
         convop  = CO_NONE;
#endif
		 AHIType = channels > 1 ? AHIST_S16S : AHIST_M16S;
		 break;
	  case AF_FORMAT_S24_BE:
		 convop  = CO_24_BE2BE;
		 AHIType = channels > 1 ? AHIST_S32S : AHIST_M32S;
		 break;
	  case AF_FORMAT_S24_LE:
		 convop  = CO_24_LE2BE;
		 AHIType = channels > 1 ? AHIST_S32S : AHIST_M32S;
		 break;
	  case AF_FORMAT_S32_BE:
#if AROS_BIG_ENDIAN || !defined(__AROS__)
		 convop  = CO_NONE;
#else
		 convop  = CO_32_LE2BE;
#endif
		 AHIType = channels > 1 ? AHIST_S32S : AHIST_M32S;
		 break;
	  case AF_FORMAT_S32_LE:
#if AROS_BIG_ENDIAN || !defined(__AROS__)
		 convop  = CO_32_LE2BE;
#else
		 convop  = CO_NONE;
#endif
		 AHIType = channels > 1 ? AHIST_S32S : AHIST_M32S;
		 break;
	  case AF_FORMAT_AC3:
		 convop  = CO_NONE;
		 AHIType = AHIST_S16S;
		 rate = 48000;
		 break;

	  case AF_FORMAT_FLOAT_BE:
#if AROS_BIG_ENDIAN || !defined(__AROS__)
	 	 convop  = CO_FLOAT_S32BE;
#else
	 	 convop  = CO_FLOAT_S32LE;
#endif
	 	 AHIType = channels > 1 ? AHIST_S32S : AHIST_M32S;
	 	 break;
	  case AF_FORMAT_FLOAT_LE:
#if AROS_BIG_ENDIAN || !defined(__AROS__)
	 	 convop  = CO_FLOAT_S32LE;
#else
	 	 convop  = CO_FLOAT_S32BE;
#endif
	 	 AHIType = channels > 1 ? AHIST_S32S : AHIST_M32S;
	   	 break;
/*
	  case AF_FORMAT_FLOAT_BE:
		 convop  = CO_FLOAT_S16BE;
		 AHIType = channels > 1 ? AHIST_S16S : AHIST_M16S;
		 break;
	  case AF_FORMAT_FLOAT_LE:
		 convop  = CO_FLOAT_S16LE;
		 AHIType = channels > 1 ? AHIST_S16S : AHIST_M16S;
		 break;
*/
	  default:
		 cleanup();
		 mp_msg(MSGT_AO, MSGL_WARN, "AHI: Sound format not supported by this driver.\n");
		 return 0; // return fail !
	  }


   if ( ! (buffer = AllocVec ( AHI_CHUNKMAX  , MEMF_PUBLIC | MEMF_CLEAR ) ) ) {
	  cleanup();
	  mp_msg(MSGT_AO, MSGL_WARN, "AHI: Not enough memory.\n");
	  return 0;
   }

   memset(&LockSemaphore, 0, sizeof(LockSemaphore));
   InitSemaphore(&LockSemaphore);

//Allocate our signal
if ( ( sig_taskready = AllocSignal ( -1 ) )  == -1 ) {
   cleanup();
   return 0;
   }

	//Allocate our signal
	if ( ( sig_taskend = AllocSignal ( -1 ) )  == -1 ) {
   cleanup();
   return 0;
   }

   kk(KPrintF("sig_taskready: %d |  sig_taskready %d\n", sig_taskready, sig_taskend );)

   MainTask = FindTask(NULL);

   running = TRUE;

   kk(KPrintF("Creating Task\n");)
   if ( ! (PlayerTask_Process = CreateNewProcTags(
#ifndef __AROS__
				  NP_CodeType,         CODETYPE_PPC,
#endif
				  NP_Entry,            (ULONG) PlayerTask,
				  NP_Name,             PLAYERTASK_NAME,
				  NP_Priority,         PLAYERTASK_PRIORITY,          
				  //NP_StartupMsg,     (ULONG) msg,
				  //NP_TaskMsgPort,    (ULONG) &port,
			  TAG_DONE) ) ) {
		 
			cleanup();
			kk(KPrintF("AHI: Unable to create the audio process.\n");)
			mp_msg(MSGT_AO, MSGL_WARN, "AHI: Unable to create the audio process.\n");
			return 0;
   }

   // Here Wait for a singal from PlayerTask
   // It can bo "ok" -> sig_taskready
   // Or grrr ! -> sig_taskend
   kk(KPrintF("Waiting for signal for the PlayerTask\n");)
   if ( Wait ( (1L << sig_taskready) | (1L << sig_taskend) ) == (1L << sig_taskend) ) 
	{
		kk(KPrintF("Signal KO !\n");)
		cleanup();
		return 0; // Unable to init -> exit
	}
   kk(KPrintF("Signal received\n");)

   return 1;                                // Everything is ready to go !
}

/***************************** UNINIT ********************************/
// close audio device
static void uninit(int immed){
   kk(KPrintF("Uninit\n");)
   running=FALSE;

   kk(KPrintF("Freeing signals\n");)   
   if (sig_taskend != -1) {
	  if (PlayerTask_Process)
	  {
		 Wait(1L << sig_taskend ); // Wait for the PlayerTask to be finished before leaving !
	  }
	  FreeSignal ( sig_taskend );   
	  sig_taskend=-1; 
   }
   if (sig_taskready != -1) {
	  FreeSignal (sig_taskready );
	  sig_taskready =-1;
   }  
}

/***************************** RESET **********************************/
// stop playing and empty buffers (for seeking/pause)
static void reset(){
/*
kk(KPrintF("RESET AbortIO\n");)
 
   if (AHIio) {
		 AbortIO( (struct IORequest *) AHIio);
		 WaitIO( (struct IORequest *) AHIio);
   }
   if (AHIio2) {
	  AbortIO( (struct IORequest *) AHIio2);
	  WaitIO( (struct IORequest *) AHIio2);
   }   
kk(KPrintF("RESET: END AbortIO\n");) 
*/



   buffer_get=0;
   buffer_put=0;
   buffer_full=0;
   written_byte=0;
}

/***************************** PAUSE **********************************/
// stop playing, keep buffers (for pause)
static void audio_pause()
{
//	  AHIio->ahir_Std.io_Command  = CMD_STOP;
//	  DoIO( (struct IORequest *) AHIio);
}

/***************************** RESUME **********************************/
// resume playing, after audio_pause()
static void audio_resume()
{
//	 AHIio->ahir_Std.io_Command  = CMD_START;
//	 DoIO( (struct IORequest *) AHIio);
}

/***************************** GET_SPACE *******************************/
// return: how many bytes can be played without blocking
static int get_space(){
   return AHI_CHUNKMAX - written_byte; // aka total - used 
}

/***************************** PLAY ***********************************/
// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(void* data,int len,int flags){
   // This function just fill a buffer which is played by another task
   int data_to_put, data_put;   
   int i, imax;

   kk(KPrintF("play\n");)

   if(buffer == NULL)
   {
	kk(KPrintF("NULL buffer ?!\n"));
	return 0;
   }
   
   ObtainSemaphore(&LockSemaphore); // Semaphore to protect our variable as we have 2 tasks
   kk(KPrintF("Semaphore Obtained\n");)

   kk(KPrintF("buffer_get=%d buffer_put=%d buffer_full=%d len=%d\n", buffer_get, buffer_put, buffer_full, len);)

   if ( buffer_full ) { 
	  kk(KPrintF("Buffer Full\n");)
	  kk(KPrintF("Semaphore Released (next line)\n");)
	  ReleaseSemaphore(&LockSemaphore);
	  return 0;
   }
   
   // Buffer not full -> we put data inside
   data_to_put = buffer_get - buffer_put;
   if ( (data_to_put < 0) || ( (!data_to_put) && (!buffer_full) ) ) data_to_put = AHI_CHUNKMAX - buffer_put;
   if ( data_to_put > len) data_to_put = len ;
   data_put = data_to_put;

   kk(KPrintF("Filling the buffer: data_to_put:%d\n", data_to_put);) 
   switch (convop)
   {
	 case CO_NONE:
	   CopyMem( data, buffer + buffer_put, data_to_put);    
	   break;

	 case CO_8_U2S:
	   imax = data_to_put;
	   for (i = 0; i < imax ; i++)
	   {
		 ((BYTE *) (buffer + buffer_put))[i] = ((UBYTE *)data)[i] - 128;
	   }
	   break;

	 case CO_16_U2S:
	   imax = data_to_put / 2;
	   for (i = 0; i < imax; i++)
	   {
		 ((WORD *) (buffer + buffer_put))[i] = ((UWORD *)data)[i] - 32768;
	   }
	   break;

	 case CO_16_LE2BE_U2S:
	   imax = data_to_put / 2;
	   for (i = 0; i < imax; i++)
	   {
		 ((WORD *) (buffer + buffer_put))[i] = SWAPWORD(((UWORD *)data)[i]) - 32768;
	   }
	   break;

	 case CO_16_LE2BE:
	   imax = data_to_put / 2;
	   for (i = 0; i < imax; i++)
	   {
		 ((WORD *) (buffer + buffer_put))[i] = SWAPWORD(((WORD *)data)[i]);
	   }
	   break;

	 case CO_24_BE2BE:
	   imax = data_to_put / 4;
	   data_to_put = imax * 4;
	   data_put = imax * 3;
	   for (i = 0; i < imax; i++)
	   {
		 ((LONG *) (buffer + buffer_put))[i] = (LONG)(*((ULONG *)(((UBYTE *)data)+((i*3)-1))) << 8);
	   }
	   break;

	 case CO_24_LE2BE:
	   imax = data_to_put / 4;
	   data_to_put = imax * 4;
	   data_put = imax * 3;
	   for (i = 0; i < imax; i++)
	   {
		 ((LONG *) (buffer + buffer_put))[i] = (LONG)(SWAPLONG(*((ULONG *)(((UBYTE *)data)+(i*3)))) << 8);
	   }
	   break;

	 case CO_32_LE2BE:
	   imax = data_to_put / 4;
	   for (i = 0; i < imax; i++)
	   {
		 ((LONG *) (buffer + buffer_put))[i] = SWAPLONG(((LONG *)data)[i]);
	   }
	   break;

	 case CO_FLOAT_S32BE:
	   imax = data_to_put / 4;
   	   for (i = 0; i < imax; i++)
	   {
		   ((LONG *) (buffer + buffer_put))[i] = lrintf(2147483647.0 * (((float *)data)[i]));
	   }
	   break;

	 case CO_FLOAT_S32LE:
	   imax = data_to_put / 4;
	   for (i = 0; i < imax; i++)
	   {
		   ((LONG *) (buffer + buffer_put))[i] = SWAPLONG(lrintf(2147483647.0 * (((float *)data)[i])));
	   }
	   break;

 	 case CO_FLOAT_S16BE:
	   imax = data_to_put / 2;
   	   for (i = 0; i < imax; i++)
   	   {
   		   ((WORD *) (buffer + buffer_put))[i] = lrintf(32767.0 * (((float *)data)[i]));
   	   }
   	   break;

 	 case CO_FLOAT_S16LE:
	   imax = data_to_put / 2;
   	   for (i = 0; i < imax; i++)
   	   {
       	   ((WORD *) (buffer + buffer_put))[i] = SWAPWORD(lrintf(32767.0 * (((float *)data)[i])));
	   }
	   break;
   }
   
   // update buffer_put
   buffer_put += data_to_put;
   buffer_put %= AHI_CHUNKMAX;

   if (buffer_put==buffer_get) buffer_full = TRUE; // Our buffer is full !
   kk(if (buffer_put==buffer_get) kk(KPrintF( (buffer_full) ? "Buffer full !\n" : "Buffer Empty !\n");))
  
   written_byte += data_to_put;

   tv_write = GetTimerMS();

   kk(KPrintF("Semaphore Released (next line);\n");)
   ReleaseSemaphore(&LockSemaphore); // We have finished to use varaibles which need protection
  
   return data_put;
}

/***************************** GET_DELAY **********************************/
// return: delay in seconds between first and last sample in buffer
static float get_delay(){
   float t = tv_write ? (float)(GetTimerMS() - tv_write) / 1000.0f : 0.0f;

   //printf("get delay written_byte %d %f\n", written_byte, ( (written_byte / audio_bps ) - ( (float) t / 1000.0 ) ) );
   return ( ( written_byte / audio_bps ) - t ); // return (written_byte in s) - (played byte in s) 
}
