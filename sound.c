/*
 * $Id: sound.c,v 1.3 1997/09/27 23:27:22 mdz Exp mdz $
 * WADDLE - sound.c
 *
 * History: $Log: sound.c,v $
 * History: Revision 1.3  1997/09/27 23:27:22  mdz
 * History: *** empty log message ***
 * History:
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

#ifdef WIN32_MCI
#include <mmsystem.h>
#endif

#include <assert.h>

#include "waddle.h"

#ifdef LINUX
#include <linux/soundcard.h>
#endif

/* Remember this for channel separation */
static int sample_size_global;

#ifdef WIN32
static LPDIRECTSOUND lpDirectSound;
static LPDIRECTSOUNDBUFFER lpDsb;
#endif

#ifdef WIN32_MCI
/* For WIN32 sound */
/* WAVE header (16-byte format spec) */
static char wave_header[128] = "RIFF\0x19\0xa2\0x00\0x00WAVEfmt \0x10\0x00\0x00\0x00";
/*static char wave_header[128] = "WAVEfmt \0x10\0x00\0x00\0x00";*/
/*static char wave_header[128] = "WAVEfmt \0x00\0x00\0x00\0x00";*/
#define WAVE_MAGIC 20 /* start of format data */
static char *lpData = wave_header;
static long fileSize = 128;
static UINT wave_device_id;
#endif

/*
 * Sound setup
 *
 */

/**** Linux sound (voxware) ****/
#ifdef LINUX
int sound_setup(int dev,int sampling_rate,int sample_size,int channels)
{
  int format = 0, arg = 0;

  /* Note: always set sample format (size), number of channels, sample rate *
   * in that order */

  sample_size_global = sample_size;

  if (ioctl(dev,SNDCTL_DSP_GETFMTS,&arg) < 0)
    {
      return(-1); /* Not a sound device? */
    }

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

  arg = channels;
  if (ioctl(dev,SNDCTL_DSP_STEREO,&arg) < 0)
    {
      perror("ioctl: SNDCTL_DSP_STEREO");
      exit(1);
    }
  if (arg != channels) /* Our request was denied */
    {
      fprintf(stderr,"Number of channels (%d) unsupported\n",channels);
      exit(1);
    }

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

/**** WIN32 sound (DirectSound) ****/
#elif defined(WIN32)

/* ignore dev in this driver */
int sound_setup(int dev,int sampling_rate,int sample_size,int channels)
{
  DSCAPS dscaps;
  WAVEFORMATEX pcmwf;
  DSBUFFERDESC dsbdesc;
  HRESULT hr;
  HWND hwnd;

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
	
  /* We can't query capabilities for some reason...testing */
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
    
  pcmwf.wFormatTag = WAVE_FORMAT_PCM;
  pcmwf.nChannels = channels;
  pcmwf.nSamplesPerSec = sampling_rate * 2; /* This is insane */
  pcmwf.nBlockAlign = 1; /* This pukes in strange ways if changed */
  pcmwf.nAvgBytesPerSec = pcmwf.nSamplesPerSec * sample_size;
  pcmwf.wBitsPerSample = sample_size * 8;

  memset(&dsbdesc, 0, sizeof(dsbdesc));
  dsbdesc.dwSize = sizeof(DSBUFFERDESC);

  dsbdesc.dwFlags = DSBCAPS_CTRLDEFAULT;
  dsbdesc.dwBufferBytes = 5 * pcmwf.nSamplesPerSec;
  dsbdesc.lpwfxFormat = &pcmwf;

  hr = lpDirectSound->lpVtbl->CreateSoundBuffer(lpDirectSound,
       &dsbdesc, &lpDsb, NULL);
  if (hr != DS_OK)
    DS_Error("CreateSoundBuffer",hr);
  
  /* Set format for the buffer */
  /*lpDsb->lpVtbl->SetFormat(lpDsb,&pcmwf);
  if (hr != DS_OK)
	DS_Error("SetFormat",hr);*/
  
  return(0);
}

static int playing = 0;
static DWORD real_writecursor = 0;

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
	else
	{
		CopyMemory((unsigned char *)lpvPtr1,buf,dwBytes1);
		real_writecursor += dwBytes1;
	}
	/*memcpy(lpvPtr1,buf,len);
	write(1,buf,len);*/

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

/* This is the old MCI driver that I gave up on...it's just too watered down */
#ifdef WIN32_MCI

LRESULT CALLBACK IOProc(LPMMIOINFO lpMMIOInfo, UINT uMessage, LPARAM lParam1, LPARAM lParam2)
{ 
  static BOOL alreadyOpened = FALSE;

  fprintf(stderr,"Inside MCI IOProc...\n");
  fprintf(stderr,"uMessage: %d, lParam1: %d, lParam2: %d\n",uMessage,lParam1,lParam2);

   switch (uMessage) {
      case MMIOM_OPEN:
         if (alreadyOpened)
            return 0;
    alreadyOpened = TRUE;

    lpMMIOInfo->lDiskOffset = 0;
    return 0;

      case MMIOM_CLOSE:
         return 0;

      case MMIOM_READ:
    memcpy((void *)lParam1, lpData+lpMMIOInfo->lDiskOffset,lParam2); 
         lpMMIOInfo->lDiskOffset += lParam2;

         return (lParam2);

      case MMIOM_SEEK:
         switch (lParam2) {
            case SEEK_SET:
               lpMMIOInfo->lDiskOffset = lParam1;
               break;

            case SEEK_CUR:
               lpMMIOInfo->lDiskOffset += lParam1;

            case SEEK_END:
               lpMMIOInfo->lDiskOffset = fileSize - 1 - lParam1;
               break;
         }
         return lpMMIOInfo->lDiskOffset;

      default:
        return(-1); /* Unexpected msgs.  For instance, we do not
                             process MMIOM_WRITE in this sample */
  }
}

int sound_setup(int dev,int sampling_rate,int sample_size,int channels)
{
  /*MCI_WAVE_SET_PARMS set_parms;*/
  MCI_OPEN_PARMS open_parms;
  DWORD dwReturn;
  WAVEFORMAT wave_format;
  char errbuf[128];

  fprintf(stderr,"WIN32 sound support is incomplete -- watch out\n");

  /* Windows is dumb like dat */
  if ( (sampling_rate != 11025) && (sampling_rate != 22050) && (sampling_rate != 44100) )
  {
	fprintf(stderr,"Sampling rate (%d) unsupported\n",sampling_rate);
	exit(-1);
  }

  /* Fake a WAVE header */
  /* I hate this, I really do --mdz */
  if (sampling_rate == 11025)
	if (sample_size == 1)
	  if (channels == 1)
		wave_format.wFormatTag = WAVE_FORMAT_1M08;
	  else /* channels == 1 */
	    wave_format.wFormatTag = WAVE_FORMAT_1S08;
    else /* sample_size == 1 */
	  if (channels == 1)
		wave_format.wFormatTag = WAVE_FORMAT_1M16;
	  else /* channels == 1 */
	    wave_format.wFormatTag = WAVE_FORMAT_1S16;
  else if (sampling_rate == 22050)
	if (sample_size == 1)
	  if (channels == 1)
		wave_format.wFormatTag = WAVE_FORMAT_2M08;
	  else /* channels == 1 */
	    wave_format.wFormatTag = WAVE_FORMAT_2S08;
    else /* sample_size == 1 */
	  if (channels == 1)
		wave_format.wFormatTag = WAVE_FORMAT_2M16;
	  else /* channels == 1 */
	    wave_format.wFormatTag = WAVE_FORMAT_2S16;
  else /* sampling rate must be 44100 */
	if (sample_size == 1)
	  if (channels == 1)
		wave_format.wFormatTag = WAVE_FORMAT_4M08;
	  else /* channels == 1 */
	    wave_format.wFormatTag = WAVE_FORMAT_4S08;
    else /* sample_size == 1 */
	  if (channels == 1)
		wave_format.wFormatTag = WAVE_FORMAT_4M16;
	  else /* channels == 1 */
	    wave_format.wFormatTag = WAVE_FORMAT_4S16;

  wave_format.nChannels = channels;
  wave_format.nSamplesPerSec = sampling_rate;
  wave_format.nAvgBytesPerSec = (sampling_rate * sample_size);
  wave_format.nBlockAlign = 1;

  memcpy(&wave_header[WAVE_MAGIC],&wave_format,sizeof(wave_format));
  memcpy(&wave_header[WAVE_MAGIC + sizeof(wave_format)],"data",4);

  /* Install a custom I/O routine */
  mmioInstallIOProc(mmioFOURCC('W', 'A', 'D', ' '), (LPMMIOPROC)IOProc,
        MMIO_INSTALLPROC | MMIO_GLOBALPROC);

  /* Open the wave audio device. */
  open_parms.lpstrDeviceType = "waveaudio";
  open_parms.lpstrElementName = "test.WAD+";
  
  if (dwReturn = mciSendCommand( 0, MCI_OPEN, MCI_OPEN_TYPE |
         MCI_OPEN_ELEMENT, (DWORD) (LPVOID) &open_parms))
  {
    mciGetErrorString(dwReturn, errbuf, sizeof (errbuf));
	fprintf(stderr,"MCI error: %s",errbuf);
	exit(1);
  }

  wave_device_id = open_parms.wDeviceID;
  fprintf(stderr,"wave_device_id: %d\n",wave_device_id);

  return(-1);
}

void play_sound(char *buf,int len)
{
  DWORD dwReturn;
  char errbuf[128];

  lpData = buf;
  fileSize = len;
  if ( dwReturn = mciSendCommand(wave_device_id,MCI_PLAY,0,0) )
  {
    mciGetErrorString(dwReturn,errbuf,sizeof(errbuf));
	fprintf(stderr,"MCI error: %s\n",errbuf);
	exit(1);
  }
}

void sound_cleanup(void)
{
  mmioInstallIOProc(mmioFOURCC('W', 'A', 'D', ' '), NULL,MMIO_REMOVEPROC);
}

#error Gave up on the MCI driver early 9/28

#endif /* WIN32_MCI */

#else /* WIN32 */

#error Unsupported sound driver interface

#endif /* WIN32 */

void get_channel(const char *src,char *dst,int len,int which)
{
  const char *src_ptr;
  char *dst_ptr;
  /*const short int *src_ptr_big;
    short int *dst_ptr_big;*/

  if (which == LEFT_CHANNEL)
    src_ptr = src;
  else if (which == RIGHT_CHANNEL)
    src_ptr = src + sample_size_global;
  else
    {
      fprintf(stderr,"Scary shit going down.  Bailing.\n");
      exit(1);
    }

#define CHANNELS 2 /* Someday... */

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
