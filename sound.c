/*
 * $Id: sound.c,v 1.13 1997/10/11 17:20:16 mdz Exp $
 *
 * History: $Log: sound.c,v $
 * History: Revision 1.13  1997/10/11 17:20:16  mdz
 * History: Cleaned up
 * History:
 * History: Revision 1.12  1997/10/10 21:54:18  mdz
 * History: fixed stereo problem in linux driver
 * History:
 * History: Revision 1.11  1997/10/10 17:01:31  mdz
 * History: Whipped things into shape
 * History:
 * History: Revision 1.10  1997/10/06 02:46:55  mdz
 * History: Took out sequence numbers
 * History:
 * History: Revision 1.9  1997/10/02 16:00:45  mdz
 * History: Added sequence numbers
 * History:
 * History: Revision 1.8  1997/10/01 19:48:31  mdz
 * History: Cleaned up
 * History:
 * History: Revision 1.7  1997/10/01 19:16:55  mdz
 * History: Finally got the DirectSound driver working
 * History:
 * History: Revision 1.3  1997/09/27 23:27:22  mdz
 * History: *** empty log message ***
 * History:
 *
 */

/* 
 * WADDLE - sound.c
 *
 * Interfaces to system sound drivers
 *
 * Currently supports:
 *
 * Linux (VOXware)
 * Win95/WinNT (DirectSound/DirectX)
 *
 * Matt "Doc" Zimmerman <mdz@csh.rit.edu> 1997
 *
 */

#include <stdio.h>

#ifndef WIN32
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

#ifdef WIN32
#include <io.h>
#endif

#include <assert.h>

#ifdef LINUX
#include <linux/soundcard.h>
#endif

#include "waddle.h"

/* Remember this for channel separation */
static int sample_size_global;
static int dev_global;

#ifdef WIN32

static LPDIRECTSOUND lpDirectSound;
static LPDIRECTSOUNDBUFFER lpDsb;

static int playing = 0; /* Has the buffer started playing? */
static DWORD real_writecursor = 0; /* where to write in the buffer */

#endif /* WIN32 */

/*
 * Sound interfaces
 *
 */

/* For all interfaces - called by sound_setup */
void generic_sound_setup(void)
{
  /* Nothing anymore */
}

/* Called by top level */
void generic_play_sound(char *buf,unsigned int len)
{
  play_sound(buf,len);
}

#ifdef LINUX

/* The Linux (VOXware) sound interface */
/* Requires kernel sound support (incl. /dev/dsp) */

int sound_setup(int dev,int sampling_rate,int sample_size,int channels)
{
  int format = 0, arg = 0, tmp;

  /* Note: always set sample format (size), number of channels, sample rate
     in that order */

  sample_size_global = sample_size;
  dev_global = dev;

  generic_sound_setup();

  if (ioctl(dev,SNDCTL_DSP_GETFMTS,&arg) < 0)
    {
      return(-1); /* Not a sound device? */
    }

  /* Set sample size */
  assert( (sample_size == 1) || (sample_size == 2));
  if (sample_size == 1)
    format = arg = AFMT_U8;
  else if (sample_size == 2)
    format = arg = AFMT_S16_LE; /* Little-endian */

  if (ioctl(dev,SNDCTL_DSP_SETFMT,&arg) < 0)
    {
      perror("ioctl: SNDCTL_DSP_SETFMT");
      exit(1);
    }

  if (arg != format) /* Our request was denied */
    {
      fprintf(stderr,"Sample size (%d) unsupported\n",sample_size);
      exit(1);
    }

  /* Set number of channels */
  tmp = channels - 1; /* 0 == mono, 1 == stereo */
  arg = tmp;
  if (ioctl(dev,SNDCTL_DSP_STEREO,&arg) < 0)
    {
      perror("ioctl: SNDCTL_DSP_STEREO");
      exit(1);
    }
  if (arg != tmp) /* Our request was denied */
    {
      fprintf(stderr,"Number of channels (%d) unsupported\n",channels);
      exit(1);
    }

  /* Set sampling rate */
  format = arg = sampling_rate;

  if (ioctl(dev,SNDCTL_DSP_SPEED,&arg) < 0)
    {
      perror("ioctl: SNDCTL_DSP_SPEED");
      exit(1);
    }
  if (arg != format) /* Our request was denied */
    {
      fprintf(stderr,"Sampling rate (%d) unsupported\n",sampling_rate);
      exit(1);
    }

  return(0);
}

/* Simplicity itself */
void play_sound(char *buf,unsigned int len)
{
  if (write(dev_global,buf,len) < 0)
    {
      perror("write");
      exit(1);
    }
}

#elif defined(WIN32) /* ^ Linux v WIN32 */

/* The WIN32 DirectSound interface */
/* Requires a working version of DirectX (probably 5.0 or greater) */

/* ignore dev in this interface */
int sound_setup(int dev,int sampling_rate,int sample_size,int channels)
{
  DSCAPS dscaps;
  WAVEFORMATEX pcmwf;
  DSBUFFERDESC dsbdesc;
  HRESULT hr;
  HWND hwnd;

  generic_sound_setup();

  hr = DirectSoundCreate(NULL, &lpDirectSound,NULL);
  if (hr != DS_OK)
    DS_Error("DirectSoundCreate",hr);

  /* Get a window handle for this console */
  SetConsoleTitle("WADDLE");
  Sleep(40);
  hwnd = FindWindow(NULL,"WADDLE");
  
  
  hr = lpDirectSound->lpVtbl->SetCooperativeLevel(lpDirectSound, hwnd, DSSCL_EXCLUSIVE);
  if (hr!= DS_OK)
    DS_Error("SetCooperativeLevel",hr);
	
  /* We can't query capabilities yet for some reason...testing */
  /*memset(&dscaps,0,sizeof(dscaps));
    hr = lpDirectSound->lpVtbl->GetCaps(lpDirectSound,&dscaps);
    if (hr != DS_OK)
    DS_Error("GetCaps",hr);

    if ( (channels == 2) && !(dscaps.dwFlags & DSCAPS_PRIMARYSTEREO) )
    {
    fprintf(stderr,"Stereo not available -- waaah\n");
    exit(1);
    }

    if ( (sample_size == 2) && !(dscaps.dwFlags & DSCAPS_PRIMARY16BIT) )
    {
    fprintf(stderr,"16-bit samples not available -- waah\n");
    exit(1);
    }*/

  /* XXX - Add sample rate range checking here */
  
  memset(&pcmwf, 0, sizeof(pcmwf));
    
  /* This is a braindead structure */
  /* BlockAlign and AvgBytesPerSec should be calculated for us; in fact,
     the call fails if we don't calculate them this way */
  pcmwf.wFormatTag = WAVE_FORMAT_PCM;
  pcmwf.nChannels = channels;
  pcmwf.nSamplesPerSec = sampling_rate;
  pcmwf.nBlockAlign = sample_size * channels;
  pcmwf.nAvgBytesPerSec = pcmwf.nSamplesPerSec * sample_size * channels;
  pcmwf.wBitsPerSample = sample_size * 8;

  memset(&dsbdesc, 0, sizeof(dsbdesc));
  dsbdesc.dwSize = sizeof(DSBUFFERDESC);

  /* Request default controls */
  dsbdesc.dwFlags = DSBCAPS_CTRLDEFAULT;
  /* Five seconds of buffer is more than enough since we're streaming */
  dsbdesc.dwBufferBytes = 5 * pcmwf.nSamplesPerSec;
  /* The format structure */
  dsbdesc.lpwfxFormat = &pcmwf;

  hr = lpDirectSound->lpVtbl->CreateSoundBuffer(lpDirectSound,
						&dsbdesc, &lpDsb, NULL);
  if (hr != DS_OK)
    DS_Error("CreateSoundBuffer",hr);
  
  return(0);
}

void play_sound(char *buf,unsigned int len)
{
  LPVOID lpvPtr1;
  DWORD dwBytes1;
  LPVOID lpvPtr2;
  DWORD dwBytes2;
  DWORD status = 0;
  DWORD playcursor, writecursor;
  HRESULT hr;

  hr = lpDsb->lpVtbl->Lock(lpDsb, real_writecursor, len, &lpvPtr1, 
			   &dwBytes1, &lpvPtr2, &dwBytes2, 0);
  if (hr != DS_OK)
    DS_Error("Lock",hr);
	
  fprintf(stderr,"Locked %d of %d bytes of buffer\n",dwBytes1,len);

  if (dwBytes1 < len) /* Wrap around end of buffer */
    {
      CopyMemory(lpvPtr1,buf,dwBytes1);
      if (dwBytes2 != 0)
	{
	  CopyMemory(lpvPtr2,buf + dwBytes1,dwBytes2);
	}
      real_writecursor = dwBytes2;
    }
  else /* Straight copy */
    {
      CopyMemory(lpvPtr1,buf,dwBytes1);
      real_writecursor += dwBytes1;
    }

  hr = lpDsb->lpVtbl->Unlock(lpDsb,lpvPtr1, dwBytes1, lpvPtr2, dwBytes2);
  if (hr != DS_OK)
    DS_Error("Unlock",hr);

  if (!playing)
    {
      playing = 1;
      hr = lpDsb->lpVtbl->Play(lpDsb,0,0,DSBPLAY_LOOPING);
      if (hr != DS_OK)
	DS_Error("Play",hr);
    }

  lpDsb->lpVtbl->GetStatus(lpDsb,&status);
  fprintf(stderr,"Status: %d\n",status);
	
  hr = lpDsb->lpVtbl->GetCurrentPosition(lpDsb,&playcursor,&writecursor);
  if (hr != DS_OK)
    DS_Error("GetCurrentPosition",hr);

  fprintf(stderr,"Cursors: %d %d\n",playcursor,writecursor);
}


/* Error codes for DirectSound
   (why doesn't MS give me a strerror() equivalent?) */
/* Print an error message and bail */
void DS_Error(const char *context,HRESULT hr)
{
  char *msg;
  switch (hr)
    {
    case DS_OK:
      msg = "No error";
      break;
    case DSERR_ALLOCATED:
      msg = "The request failed because resources, such as a priority level, were already in use by another caller.";
      break;
    case DSERR_ALREADYINITIALIZED:
      msg = "The object is already initialized.";
      break;
    case DSERR_BADFORMAT:
      msg = "The specified wave format is not supported.";
      break;
    case DSERR_BUFFERLOST:
      msg = "The buffer memory has been lost and must be restored.";
      break;
    case DSERR_CONTROLUNAVAIL:
      msg = "The control (volume, pan, and so forth) requested by the caller is not available.";
      break;
    case DSERR_GENERIC:
      msg = "An undetermined error occurred inside the DirectSound subsystem.";
      break;
    case DSERR_INVALIDCALL:
      msg = "This function is not valid for the current state of this object.";
      break;
    case DSERR_INVALIDPARAM:
      msg = "An invalid parameter was passed to the returning function.";
      break;
    case DSERR_NOAGGREGATION:
      msg = "The object does not support aggregation.";
      break;
    case DSERR_NODRIVER:
      msg = "No sound driver is available for use.";
      break;
    case DSERR_OTHERAPPHASPRIO:
      msg = "This value is obsolete and is not used.";
      break;
    case DSERR_OUTOFMEMORY:
      msg = "The DirectSound subsystem could not allocate sufficient memory to complete the caller's request.";
      break;
    case DSERR_PRIOLEVELNEEDED:
      msg = "The caller does not have the priority level required for the function to succeed.";
      break;
    case DSERR_UNINITIALIZED:
      msg = "The IDirectSound::Initialize method has not been called or has not been called successfully before other methods were called.";
      break;
    case DSERR_UNSUPPORTED:
      msg = "The function called is not supported at this time.";
      break;
    default:
      msg = "Unknown error.";
    }
  fprintf(stderr,"%s: DirectSound error %d: %s\n",context,hr,msg);
  exit(1);
}

#else /* ^ WIN32 v unsupported*/

#error Unsupported sound driver interface

#endif /* architecture */

void get_channel(const char *src,char *dst,int len,int which)
{
  const char *src_ptr;
  char *dst_ptr;
  /*const short int *src_ptr_big;
    short int *dst_ptr_big;*/

  assert(which == LEFT_CHANNEL || which == RIGHT_CHANNEL);
  if (which == LEFT_CHANNEL)
    src_ptr = src;
  else /* RIGHT_CHANNEL */
    src_ptr = src + sample_size_global;

#define CHANNELS 2 /* More someday... */

  if (sample_size_global == 1) /* 8-bit samples */
    for(dst_ptr = dst; len > 0;
	len -= CHANNELS,
	  src_ptr += CHANNELS,
	  dst_ptr++)
      *dst_ptr = *src_ptr;
  else /* 16-bit samples */
    {
      fprintf(stderr,"Channel separation for 16-bit samples is incomplete\n");
      exit(-1);
    }
}
