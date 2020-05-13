/* 
 *  ao_ahi.c
 *  libao2 module for MPlayer MorphOS 
 *  using AHI/low level API
 *  Written by DET Nicolas
*/


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

#include <clib/debug_protos.h>
#include <dos/dostags.h>
#include <dos/dos.h>

#include <hardware/byteswap.h>
// End OS Specific

#include "../libaf/af_format.h"
#include "libavutil/libm.h"
#include "audio_out.h"
#include "audio_out_internal.h"

// DEBUG
#define KPRINTF kprintf
#define DEBUG(x)
#define D(x)

/****************************************************************************/

//set and define by morphos_stuff.c
extern ULONG AHI_AudioID;

struct Library *  AHIBase = NULL;

static struct MsgPort *AHIMsgPort  = NULL; // The msg port we will use for the ahi.device
static struct AHIRequest *AHIio    = NULL; // First AHIRequest structure
static BYTE AHIDevice              = -1;   // Is our AHI device opened ? 1 -> no, 0 -> Yes
static ULONG AHIType;

#define PLAYERTASK_NAME       "MPlayer Audio Thread"
#define PLAYERTASK_PRIORITY   2


static struct Task *PlayerTask_Task = NULL;
static struct Task *MainTask = NULL;       // Pointer to the main task;

static unsigned int tv_write = 0;

// some AmigaOS Signals
static BYTE sig_taskend		= -1;
static BYTE sigsound		= -1;

static struct AHIAudioCtrl *actrl = NULL;

static LONG AHI_Volume=0x10000;
static struct AHISampleInfo Sample0 = { 0, NULL, NULL };
static struct AHISampleInfo Sample1 = { 0, NULL, NULL };

static BOOL AHI_SampleLoaded = FALSE;

// Buffer management
static struct SignalSemaphore LockSemaphore;
static ULONG buffer_get = 0; // place where we have to get the buffer to play
static ULONG buffer_put = 0; // Place where we have to put the buffer we will play
static int ChunkLen = 0; // Size of each chuck we will play
static int TotalLen = 0; // Total size of the buffer

static unsigned int written_byte = 0;
static BYTE *buffer = NULL;    // Our audio buffer

static float audio_bps = 1.0f;

static BOOL DBflag = FALSE;  // double buffer flag
static BOOL running = TRUE; // switching to FALSE will stop and kill the PlayerTask

// AmigaAMP plugin message
struct PluginMessage {
	struct Message msg;
	ULONG          PluginMask;
	struct Process *PluginTask;
	UWORD          **SpecRawL;
	UWORD          **SpecRawR;
	WORD           Accepted;     /* v1.3: Don't use BOOL any more because  */
	WORD           reserved0;    /* it causes alignment problems!          */

	/* All data beyond this point is new for v1.2.                         */
	/* AmigaAMP v2.5 and up will detect this new data and act accordingly. */
	/* Older versions of AmigaAMP will simply ignore it.                   */

	ULONG            InfoMask;
	struct TrackInfo **tinfo;
	struct MsgPort   *PluginWP;  /* v1.4 (used to be reserved)             */
                               /* If not used set to NULL to avoid       */
                               /* unnessecary overhead!                  */

	/* All data beyond this point is new for v1.3.                         */
	/* AmigaAMP v2.7 and up will detect this new data and act accordingly. */
	/* Older versions of AmigaAMP will simply ignore it.                   */

	WORD             **SampleRaw;
};

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

static ao_info_t info = 
{
#ifdef __MORPHOS__
	"AHI audio output using low-level API (MorphOS)",
#else
   "AHI audio output using low-level API (AmigaOS)",
#endif
	"ahi",
	"DET Nicolas",
#ifdef __MORPHOS__
	  "MorphOS Rulez :-)"
#else
	  "Amiga Survivor !",
#endif
};

// define the standart functions that mplayer will use.
// see define in audio_aout_internal.h
LIBAO_EXTERN(ahi)

struct AHIChannelInfo
{
	struct AHIEffChannelInfo aeci;
	ULONG offset;
};

unsigned int sample_position = 0;
struct AHIChannelInfo aci;
struct Hook EffectHook;

static ULONG EffectFunc(void)
{
	struct AHIEffChannelInfo *aeci = (struct AHIEffChannelInfo *)REG_A1;
	D(kprintf("channeleffect\n"));
	sample_position	= aeci->ahieci_Offset[0];
	return 0;
}

static struct EmulLibEntry EffectFunc_Gate =
{
	TRAP_LIB, 0, (void (*)(void))EffectFunc
};

/*******************************************************************/

/* Plugin support */

// XXX: just a quick test (data is wrong and it shouldn't be instanciated here, anyway)

extern void reverse_fft(float X[], int N);

static LONG plugcpt = 0;
static ULONG plugmask = 0;
static struct MsgPort *plugport = NULL;
static struct Process *plugproc = NULL;
static UWORD          *SpecRawL = NULL;
static UWORD          *SpecRawR = NULL;
static WORD           *SampleRaw = NULL;
static float L[512+1];
static float R[512+1];

ULONG OutEffectFunc(void)
{
	// A0 - (struct Hook *) A2 - (struct AHIAudioCtrl *) A1 - (struct AHIEffOutputBuffer *)
	ULONG i;
	struct Hook *hook = (struct Hook*)REG_A0;
	struct AHIEffOutputBuffer *outbuf = (struct AHIEffOutputBuffer *)REG_A1;

	if(outbuf && outbuf->ahieob_Buffer)
	{
		memcpy(SampleRaw, outbuf->ahieob_Buffer, 1024*2);

		for(i = 0; i < 512; i++)
		{
			L[i] = (float)  ( ((WORD*)(outbuf->ahieob_Buffer))[i*2] );
			R[i] = (float)  ( ((WORD*)(outbuf->ahieob_Buffer))[i*2+1] );
		}

		reverse_fft(L, 512);
		reverse_fft(R, 512);

		for(i = 0; i < 512; i++)
		{
			//SpecRawL[i] = abs( (WORD)( L[i+1] ) ) * 2;
			//SpecRawR[i] = abs( (WORD)( R[i+1] ) ) * 2;
			SpecRawL[i] = 0;
			SpecRawR[i] = 0;
		}
		
		Signal(&(plugproc->pr_Task), plugmask);
	}

	return(TRUE);
}

struct EmulLibEntry GATEOutEffectFunc =
{
	TRAP_LIB, 0, (void (*)(void))OutEffectFunc
};

struct Hook OutEffectHook;
struct AHIEffOutputBuffer OutEffect = { AHIET_OUTPUTBUFFER,&OutEffectHook,0,NULL,0 };

void handleplugin(ULONG quit)
{
	struct PluginMessage* pmsg;

	if(quit)
	{
		D(kprintf("-> quit order sent by mplayer\n"));
		D(kprintf("-> signal plugin to quit\n"));
		Forbid();
		if(plugcpt > 0)
		{
			Signal((struct Task *) plugproc, SIGBREAKF_CTRL_C);
		}
		Permit();

		Wait (1L << plugport->mp_SigBit);
	}
	
	while((pmsg = (struct PluginMessage *) GetMsg(plugport ) ))
	{
		D(kprintf("<- received plugin message\n"));

		if(pmsg->PluginTask)
		{
			if(plugcpt > 0)
			{
				D(kprintf("a plugin already registered, skipping\n"));
				// a plugin is already running
				pmsg->Accepted = FALSE;
			}
			else
			{
				D(kprintf("plugin registering\n"));

				*(pmsg->SpecRawL) = AllocVec(512*2*2+1, MEMF_CLEAR);  //16bit
				*(pmsg->SpecRawR) = AllocVec(512*2*2+1, MEMF_CLEAR);
				*(pmsg->SampleRaw) = AllocVec(1024*2, MEMF_CLEAR);  //16bit stereo

				if(*(pmsg->SpecRawL) && *(pmsg->SpecRawR) && *(pmsg->SampleRaw))
				{
					SpecRawL = *(pmsg->SpecRawL);
					SpecRawR = *(pmsg->SpecRawR);
					SampleRaw = *(pmsg->SampleRaw);
					plugproc = pmsg->PluginTask;
					plugmask = pmsg->PluginMask;
					pmsg->Accepted = TRUE;
					EffectHook.h_Data = NULL;
					EffectHook.h_Entry = (APTR)&GATEOutEffectFunc;
					OutEffect.ahie_Effect = AHIET_OUTPUTBUFFER;
					OutEffect.ahieob_Func = &EffectHook;
					AHI_SetEffect(&OutEffect, actrl);
					plugcpt = 1;
				}
				else
				{
					if(*(pmsg->SpecRawL))
						FreeVec(*(pmsg->SpecRawL));
					if(*(pmsg->SpecRawR))
						FreeVec(*(pmsg->SpecRawR));
					if(*(pmsg->SampleRaw))
						FreeVec(*(pmsg->SampleRaw));
					pmsg->Accepted = FALSE;
				}

				D(kprintf("ok\n"));
			}
		}
		else
		{
			D(kprintf("plugin unregistering\n"));

			OutEffect.ahie_Effect = AHIET_OUTPUTBUFFER | AHIET_CANCEL;
			AHI_SetEffect(&OutEffect, actrl);
			if(SpecRawL!=NULL)
				FreeVec(SpecRawL);
			if(SpecRawR!=NULL)
				FreeVec(SpecRawR);
			if(SampleRaw!=NULL)
				FreeVec(SampleRaw);

			plugcpt = 0;

			D(kprintf("ok\n"));
		}

		D(kprintf("replying message\n"));

		ReplyMsg( (struct Message*)pmsg );
	}
}

/*******************************************************************/
static void cleanup(void)
{
	DEBUG(KPRINTF("Cleanup !\n"));

	if (AHIBase)
	{
		if ( AHI_SampleLoaded )
		{
			DEBUG(KPRINTF("Unloading Sample\n"));
			AHI_UnloadSound ( 0, actrl);
			AHI_UnloadSound ( 1, actrl);
			AHI_SampleLoaded = FALSE;
		}

		if (Sample0.ahisi_Address)
		{
			DEBUG(KPRINTF("Freeing Sample buffer\n"));
			FreeVec(Sample0.ahisi_Address);
			Sample0.ahisi_Address = NULL;
		}

		if (Sample1.ahisi_Address)
		{
			FreeVec(Sample1.ahisi_Address);
			Sample1.ahisi_Address = NULL;
		}

		if (actrl)
		{
			DEBUG(KPRINTF("Freeing Audio Hardware\n"));
	    	AHI_FreeAudio (actrl);
			actrl = NULL;
		}
		AHIBase = NULL;
	}

	if (! AHIDevice )
	{
		DEBUG(KPRINTF("Closing device\n"));
		CloseDevice ( (struct IORequest *) AHIio);
		AHIDevice = 1;       // 1 -> Not opened !
	}

	if (AHIio)
	{
		DEBUG(KPRINTF("Deleting IORequest\n"));
		DeleteIORequest ( (struct IORequest *) AHIio);	
		AHIio = NULL;
	}

	if (AHIMsgPort)
	{
		DEBUG(KPRINTF("Deleting AHIMsgPort\n"));
		DeleteMsgPort(AHIMsgPort);
		AHIMsgPort = NULL;
	}
	
	if (buffer)
	{
		DEBUG(KPRINTF("Freeing Buffer\n"));
		FreeVec(buffer );
		buffer = NULL;
	}

	if (sig_taskend != -1)
	{
		DEBUG(KPRINTF("Freeing signal taskend\n"));
	    FreeSignal ( sig_taskend );   
	    sig_taskend=-1; 
	}

	buffer_get=0;
	buffer_put=0;
	written_byte=0;

	DEBUG(KPRINTF("The end\n"));
  
}

#if 0
static int buf_free(void) {
  int free = buffer_get - buffer_put - ChunkLen;
  if (free < 0) free += TotalLen;
  return free;
}

/**
 * \brief return number of buffered bytes
 *    may only be called by playback thread
 * \return minimum number of buffered bytes, value may change between
 *    two immediately following calls, and the real number of buffered bytes
 *    might actually be larger!
 */
static int buf_used(void) {
  int used = buffer_put - buffer_get;
  if (used < 0) used += TotalLen;
  return used;
}
#endif

/******************** PLAYER TASK ********************/
static void PlayerTask (void)
{
	if ( ( sigsound = AllocSignal ( -1 ) )  != -1 )
	{
		ULONG sigdata = 1L << sigsound;
		ULONG sigplug = 0;
		ULONG sigmask = sigdata;

/*
		if(!plugport)
		{
			plugport = (struct MsgPort *) CreatePort( "AmigaAMP plugin port", 0 );
		}
*/
		if(plugport)
		{
			sigplug = 1L << plugport->mp_SigBit;
			sigmask |= sigplug;
		}

		DEBUG(KPRINTF("TASK: Starting main loop\n"));

		while(running)
		{
			ULONG signals = 0;

			signals = Wait (sigmask);

			if (!running) break;

			if(signals & sigdata)
			{
				//ObtainSemaphore(&LockSemaphore);
				DEBUG(KPRINTF("TASK: Semaphore obtained\n"));

				if ( ! written_byte )
				{
					if ( DBflag ) memset( Sample1.ahisi_Address, 0, ChunkLen);
					else memset( Sample0.ahisi_Address, 0, ChunkLen);
					DEBUG(KPRINTF("TASK: Semaphore released (Buffer EMPTY)\n"));
					//ReleaseSemaphore(&LockSemaphore);
					continue;
				}

				// Copy data in the good sample buffer
				// if ( DBflag ) // Sound 0 has been launched -> filling buffer 1
				// else ...
				if ( DBflag ) memcpy( Sample1.ahisi_Address, buffer + buffer_get, ChunkLen);
				else memcpy( Sample0.ahisi_Address, buffer + buffer_get, ChunkLen);
				   
				// Update some stuff
				written_byte   -= ChunkLen;
				buffer_get     += ChunkLen;
				buffer_get     %= TotalLen;

				DEBUG(KPRINTF("TASK: Semaphore released (next line)\n"));
				//ReleaseSemaphore(&LockSemaphore);
			}

			if(signals & sigplug)
			{
				D(kprintf("received plugin signal\n"));
				handleplugin(FALSE);
			}
		}

		if(plugport)
		{
			if(plugcpt > 0)
			{
				handleplugin(TRUE);
			}

			DeletePort(plugport);
			plugport = NULL;
		}

		DEBUG(KPRINTF("Freeing signal sigsound\n"));
		FreeSignal (sigsound );
		sigsound =-1;
	}

	Signal( (struct Task *) MainTask , 1L<< sig_taskend );
	DEBUG(KPRINTF("TASK: sig_taskend -> break\n"));
}

/******************** SOUND FUNC ********************/

static void SoundFunc (void) {
	//struct Hook *hook = (struct Hook *)REG_A0;
	//struct AHIAudioCtrl * actrl = (struct AHIAudioCtrl *)REG_A2;
	//struct AHISoundMessage * smsg = (struct AHISoundMessage *)REG_A1;

	if( (DBflag = !DBflag) ) // Flip and test
	{
		AHI_SetSound(0,1,0,0,actrl,NULL);
	}
	else
	{
		AHI_SetSound(0,0,0,0,actrl,NULL);
	}

	Signal( (struct Task *) PlayerTask_Task, 1L << sigsound );
	DEBUG(KPRINTF("Hook : sigsound sent to playtask\n"));
}

struct EmulLibEntry GATE_SoundFunc =
{
   TRAP_LIB, 0, (void (*)(void))SoundFunc
};

struct Hook SoundFunc_Hook =
{
   {NULL, NULL},
   (APTR) &GATE_SoundFunc,
   NULL,
   NULL,
};

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
			DEBUG(KPRINTF("vol->left=%f\n", vol->left));
			return CONTROL_OK;
		}

		case AOCONTROL_SET_VOLUME:
		{
			float diff;
			ao_control_vol_t* vol = (ao_control_vol_t*)arg;
			diff = (vol->left+vol->right) / 2;
			AHI_Volume =  ( (int) (diff * 2.56) ) << 8 ;
			if(AHIBase) AHI_SetVol(0, AHI_Volume, 0x8000L, actrl, AHISF_IMM);
			DEBUG(KPRINTF("AHI_VOLUME=%x diff=%f\n", AHI_Volume, diff));
			return CONTROL_OK;
		}
	}
	return CONTROL_UNKNOWN;
}

/***************************** INIT **********************************/
// open & setup audio device
// return: 1=success 0=fail
static int init(int rate,int channels,int format,int flags)
{
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

	AHIBase= NULL;

   	DEBUG(KPRINTF("Init AUDIO\n"));

	switch ( format )
	{
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
		    convop  = CO_NONE;
		    AHIType = channels > 1 ? AHIST_S16S : AHIST_M16S;
		    break;
	    case AF_FORMAT_U16_LE:
		    convop  = CO_16_LE2BE_U2S;
		    AHIType = channels > 1 ? AHIST_S16S : AHIST_M16S;
		    break;
	    case AF_FORMAT_S16_LE:
		    convop  = CO_16_LE2BE;
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
		    convop  = CO_NONE;
		    AHIType = channels > 1 ? AHIST_S32S : AHIST_M32S;
		    break;
	    case AF_FORMAT_S32_LE:
		    convop  = CO_32_LE2BE;
		    AHIType = channels > 1 ? AHIST_S32S : AHIST_M32S;
		    break;
	    case AF_FORMAT_AC3:
		    convop  = CO_NONE;
		    AHIType = AHIST_S16S;
		    rate = 48000;
		    break;

		case AF_FORMAT_FLOAT_BE:
			convop  = CO_FLOAT_S32BE;
			AHIType = channels > 1 ? AHIST_S32S : AHIST_M32S;
			break;
		case AF_FORMAT_FLOAT_LE:
			convop  = CO_FLOAT_S32LE;
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
			mp_msg(MSGT_AO, MSGL_WARN, "AHI: Sound format %d not supported by this driver.\n", format);
		    return 0; // return fail !
	  }


		DEBUG(KPRINTF("Init AHI\n"));

   	// Init AHI

	if( ( AHIMsgPort=CreateMsgPort() ))
	{
		if ( (AHIio=(struct AHIRequest *)CreateIORequest(AHIMsgPort,sizeof(struct AHIRequest))))
		{
		 	AHIio->ahir_Version = 4;
		 	AHIDevice=OpenDevice(AHINAME, AHI_NO_UNIT,(struct IORequest *)AHIio,NULL);
	  	}
   	}
	
	if(AHIDevice)
	{
	  	DEBUG(KPRINTF("Unable to open %s/0 version 4\n",AHINAME));
	  	cleanup();
	  	return 0;
   	}

   	AHIBase = (struct Library *) AHIio->ahir_Std.io_Device;
 

   memset(&LockSemaphore, 0, sizeof(LockSemaphore));
   //InitSemaphore(&LockSemaphore);

   // Calculate some size, etc ...
   {
	  ULONG playsamples;
	  ULONG AudioID;
	  if (format == AF_FORMAT_AC3)
	  {
		 AudioID = AHI_BestAudioID(
								AHIDB_Stereo,     TRUE,
								AHIDB_Bits,       16,
								AHIDB_HiFi,       FALSE,
								AHIDB_Volume,     TRUE,
								AHIDB_Panning,    TRUE,
						        AHIDB_MinMixFreq, rate,
						TAG_DONE);
	  } else {
		 AudioID = AHI_BestAudioID(
								AHIDB_Stereo,     (AHIType & 2) ? TRUE : FALSE,
								AHIDB_Bits,       (AHIType == AHIST_S8S) ? 8 : 16,
								AHIDB_HiFi,       TRUE,
								AHIDB_Volume,     TRUE,
								AHIDB_Panning,    TRUE,
						        AHIDB_MinMixFreq, rate,
						TAG_DONE);
	  }
	  if ( AudioID == AHI_INVALID_ID ) {
		 cleanup();
		 mp_msg(MSGT_AO, MSGL_ERR, "No good AudioID found.\n");
			return 0;
	  }

	  // mp_msg(MSGT_AO, MSGL_INFO, "AHI: Using 0x%x AudioID.\n", AudioID);

	  DEBUG(KPRINTF("AllocAudio\n"));
	  if(!( actrl=AHI_AllocAudio(
				   AHIA_AudioID,    AHI_DEFAULT_ID/*(AHI_AudioID ) ? AHI_AudioID : AudioID*/,
				   AHIA_Channels,   1,
				   AHIA_Sounds,     2,
				   AHIA_MixFreq,    rate,
				   AHIA_SoundFunc,  (ULONG) &SoundFunc_Hook,
				   TAG_DONE) ) ) {
		 // Problem
		 cleanup();
		 mp_msg(MSGT_AO, MSGL_ERR, "Unable to allocate audio hardware.\n");
		 return 0;
	  }

	  DEBUG(KPRINTF("GetAudioAttrs\n"));
	  AHI_GetAudioAttrs(AHI_INVALID_ID,
			actrl,
			AHIDB_MaxPlaySamples, (ULONG) &playsamples,
			TAG_DONE);

	  // Here, ChunkLen is in samples because ahisi_Length from
	  // Samplex want samples
	  ChunkLen = playsamples * 3;

	  Sample0.ahisi_Type= AHIType;
	  Sample1.ahisi_Type= AHIType;

	  Sample0.ahisi_Length= ChunkLen;
	  Sample1.ahisi_Length= ChunkLen;

	  // Now just converts ChunkLen is byte
	  ChunkLen *= AHI_SampleFrameSize( AHIType );

	  //Total buffer size
	  TotalLen = ChunkLen * 32;
	  mp_msg(MSGT_AO, MSGL_INFO, "AHI: Using %d bytes per chunk and %d Kb for buffer\n", ChunkLen, TotalLen/1024);

	  DEBUG(KPRINTF("Tring to Alloc some mem\n"));
	  Sample0.ahisi_Address=AllocVec(ChunkLen,MEMF_PUBLIC|MEMF_CLEAR);
	  Sample1.ahisi_Address=AllocVec(ChunkLen,MEMF_PUBLIC|MEMF_CLEAR);
   }

   if ( (!Sample0.ahisi_Address) || (!Sample1.ahisi_Address) ) {
	  cleanup();
	  DEBUG(KPRINTF("Alloc ChunkLen = %d\n", ChunkLen));
	  mp_msg(MSGT_AO, MSGL_ERR, "AHI: Not enough memory.\n");
	  return 0;
   } 

   if ( ! (buffer = AllocVec ( TotalLen  , MEMF_PUBLIC|MEMF_CLEAR) ) ) {
	  cleanup();
	  DEBUG(KPRINTF("Alloc AHI_MAXBUFFER failed %d\n", ChunkLen));
	  mp_msg(MSGT_AO, MSGL_ERR, "AHI: Not enough memory.\n");
	  return 0;
   }

   DEBUG(KPRINTF("Trying to load AHI sound\n"));
   if( (AHI_LoadSound(0, AHIST_DYNAMICSAMPLE, &Sample0, actrl) ) ||
	   (AHI_LoadSound(1, AHIST_DYNAMICSAMPLE, &Sample1, actrl) ) ) {
	  mp_msg(MSGT_AO, MSGL_ERR, "AHI: Unable to load sound.\n");
	  cleanup();
	  return 0;
   }

	AHI_SampleLoaded = TRUE;

	MainTask = FindTask(NULL);
	running = TRUE;
	
	DEBUG(KPRINTF("Init Signal\n"));
   	//Allocate our signal
	if ( ( sig_taskend = AllocSignal ( -1 ) )  == -1 )
	{
	  	cleanup();
	  	return 0;
   	}

	DEBUG(KPRINTF("Creating Task\n"));
	if ( ! (PlayerTask_Task = NewCreateTask(
					TASKTAG_CODETYPE,    CODETYPE_PPC,
					TASKTAG_PC,          (ULONG) PlayerTask,
					TASKTAG_NAME,        PLAYERTASK_NAME,
					TASKTAG_PRI,         PLAYERTASK_PRIORITY,
					TAG_DONE) ) )
	{

			DEBUG(KPRINTF("AHI: Unable to create the audio process.\n"));
			mp_msg(MSGT_AO, MSGL_WARN, "AHI: Unable to create the audio process.\n");
			cleanup();
			return 0;
	}
  
	if ( AHI_ControlAudio(actrl,
						  AHIC_Play,TRUE,
						  TAG_DONE) )
	{
	    mp_msg(MSGT_AO, MSGL_WARN, "AHI: Unable to start playing.\n");
	    cleanup();
	    return 0;
	}

   AHI_Play(actrl,
					AHIP_BeginChannel, 0,
					AHIP_Freq,         rate,
					AHIP_Vol,          AHI_Volume,
					AHIP_Pan,          0x8000L,
					AHIP_Sound,        0,
					//AHIP_Offset,       0,
					//AHIP_Length,       0,
					AHIP_EndChannel,   NULL,
					TAG_DONE);

	/*
	aci.aeci.ahie_Effect = AHIET_CHANNELINFO;
	aci.aeci.ahieci_Func = &EffectHook;
	aci.aeci.ahieci_Channels = 1;

	EffectHook.h_Entry = (void *)&EffectFunc_Gate;
	EffectHook.h_Data = NULL;

	AHI_SetEffect(&aci, actrl);
	*/

   ao_data.outburst = ChunkLen;
   ao_data.buffersize = TotalLen;

   return 1;                                // Everything is ready to go !
}

/***************************** UNINIT ********************************/
// close audio device
static void uninit(int immed)
{
	running = FALSE;
	//ObtainSemaphore(&LockSemaphore);
	DEBUG(KPRINTF("Uninit\n"));
	if (sig_taskend != -1 && PlayerTask_Task)
	{
		DEBUG(KPRINTF("Waiting for the PlayerTask \n"));
		Wait(1L << sig_taskend ); // Wait for the PlayerTask to be finished before leaving !
	}

	if (actrl)
	{
		//aci.aeci.ahie_Effect = AHIET_CHANNELINFO|AHIET_CANCEL;
		//AHI_SetEffect(&aci.aeci, actrl);
		AHI_ControlAudio(actrl,
						 AHIC_Play,FALSE,
						 TAG_DONE);
	}

	cleanup();
	//ReleaseSemaphore(&LockSemaphore);
	DEBUG(KPRINTF("Uninit END\n"));
}

/***************************** RESET **********************************/
// stop playing and empty buffers (for seeking/pause)
static void reset()
{
	memset( Sample0.ahisi_Address, 0, ChunkLen);
	memset( Sample1.ahisi_Address, 0, ChunkLen);
	buffer_get=0;
	buffer_put=0;
	written_byte=0;
}

/***************************** PAUSE **********************************/
// stop playing, keep buffers (for pause)
static void audio_pause()
{
	AHI_ControlAudio(actrl,
					 AHIC_Play, FALSE,
					 TAG_DONE);
}

/***************************** RESUME **********************************/
// resume playing, after audio_pause()
static void audio_resume()
{
	AHI_ControlAudio(actrl,
					 AHIC_Play, TRUE,
					 TAG_DONE);
}

/***************************** GET_SPACE *******************************/
// return: how many bytes can be played without blocking
static int get_space()
{
	DEBUG(KPRINTF("AHI_MAXBUFFER - written_byte = %d\n", TotalLen - written_byte));
	return TotalLen - written_byte; // aka total - used
}

/***************************** PLAY ***********************************/
// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(void* data,int len,int flags)
{
	// This function just fill a buffer which is played by another task
	int data_to_put, data_put;   
	int i, imax;

	if (!(flags & AOPLAY_FINAL_CHUNK))
	len = (len/ao_data.outburst)*ao_data.outburst;

	DEBUG(KPRINTF("play\n"));
	   
	//ObtainSemaphore(&LockSemaphore); // Semaphore to protect our variable as we have 2 tasks
	DEBUG(KPRINTF("Semaphore Obtained\n"));

	if ( written_byte == TotalLen )
	{
		DEBUG(KPRINTF("Semaphore Released (next line)\n"));
		//ReleaseSemaphore(&LockSemaphore);
		return 0;
	}
	   
	// Buffer not full -> we put data inside
	data_to_put = buffer_get - buffer_put;
	if ( (data_to_put < 0) || ( (!data_to_put) && ( written_byte != TotalLen ) ) ) data_to_put = TotalLen - buffer_put;
	if ( data_to_put > len) data_to_put = len ;
	data_put = data_to_put;

	DEBUG(KPRINTF("Filling the buffer: data_to_put:%d\n", data_to_put));
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
			imax = data_to_put / 4;
			data_to_put /= 2;
		    for (i = 0; i < imax; i++)
		    {
				((WORD *) (buffer + buffer_put))[i] = lrintf(32767.0 * (((float *)data)[i]));
		    }
		    break;

		case CO_FLOAT_S16LE:
			imax = data_to_put / 4;
			data_to_put /= 2;
		    for (i = 0; i < imax; i++)
		    {
				((WORD *) (buffer + buffer_put))[i] = SWAPWORD(lrintf(32767.0 * (((float *)data)[i])));
		    }
		    break;
   }
   
   // update buffer_put
   buffer_put += data_to_put;
   buffer_put %= TotalLen;

   written_byte += data_to_put;

   tv_write = GetTimerMS();

   DEBUG(KPRINTF("Semaphore Released (next line);\n"));
   //ReleaseSemaphore(&LockSemaphore); // We have finished to use variables which need protection
  
   return data_put;
}

/***************************** GET_DELAY **********************************/
// return: delay in seconds between first and last sample in buffer
static float get_delay()
{
	float t = tv_write ? (float)(GetTimerMS() - tv_write) / 1000.0f : 0.0f;
	return ((float)(written_byte)/(float)audio_bps) - t;
}
