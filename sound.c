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
  PCMWAVEFORMAT pcmwf;
  DSBUFFERDESC dsbdesc;
  HRESULT hr;
  HWND hwnd;

  if(DirectSoundCreate(NULL, &lpDirectSound,NULL) != DS_OK)
  {
    fprintf(stderr,"Couldn\'t initialize DirectSound -- barf!\n");
	exit(1);
  }

  /* Get a window handle for this console */
  SetConsoleTitle("WADDLE");
  Sleep(40);
  hwnd = FindWindow(NULL,"WADDLE");
  
  
  if ( lpDirectSound->lpVtbl->SetCooperativeLevel(lpDirectSound, hwnd, DSSCL_NORMAL) != DS_OK)
  {
    fprintf(stderr,"Couldn\'t set cooperative level -- barf!\n");
	exit(1);
  }
  /* We can't query capabilities for some reason...testing */
  /*if ( (hr = lpDirectSound->lpVtbl->GetCaps(lpDirectSound,&dscaps)) != DS_OK)
  {
    fprintf(stderr,"Couldn\'t get DirectSound capabilities: code %d\n",hr);
	fprintf(stderr,"Possible: %d %d %d %d\n",DSERR_ALLOCATED,
DSERR_INVALIDPARAM ,
DSERR_UNINITIALIZED ,
DSERR_UNSUPPORTED );
	exit(1);
  }

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
  /*if (sampling_rate == 11025)
	if (sample_size == 1)
	  if (channels == 1)
		pcmwf.wf.wFormatTag = WAVE_FORMAT_1M08;
	  else /* channels == 1 */
	    /*pcmwf.wf.wFormatTag = WAVE_FORMAT_1S08;
    else /* sample_size == 1 */
	  /*if (channels == 1)
		pcmwf.wf.wFormatTag = WAVE_FORMAT_1M16;
	  else /* channels == 1 */
	    /*pcmwf.wf.wFormatTag = WAVE_FORMAT_1S16;
  else if (sampling_rate == 22050)
	if (sample_size == 1)
	  if (channels == 1)
		pcmwf.wf.wFormatTag = WAVE_FORMAT_2M08;
	  else /* channels == 1 */
	    /*pcmwf.wf.wFormatTag = WAVE_FORMAT_2S08;
    else /* sample_size == 1 */
	  /*if (channels == 1)
		pcmwf.wf.wFormatTag = WAVE_FORMAT_2M16;
	  else /* channels == 1 */
	    /*pcmwf.wf.wFormatTag = WAVE_FORMAT_2S16;
  /*else /* sampling rate must be 44100 */
	/*if (sample_size == 1)
	  if (channels == 1)
		pcmwf.wf.wFormatTag = WAVE_FORMAT_4M08;
	  else /* channels == 1 */
	    /*pcmwf.wf.wFormatTag = WAVE_FORMAT_4S08;
    else /* sample_size == 1 */
	  /*if (channels == 1)
		pcmwf.wf.wFormatTag = WAVE_FORMAT_4M16;
	  else /* channels == 1 */
	    /*pcmwf.wf.wFormatTag = WAVE_FORMAT_4S16;*/
  
  pcmwf.wf.wFormatTag = WAVE_FORMAT_PCM;
  pcmwf.wf.nChannels = channels;
  pcmwf.wf.nSamplesPerSec = sampling_rate;
  pcmwf.wf.nBlockAlign = 1;
  pcmwf.wf.nAvgBytesPerSec = pcmwf.wf.nSamplesPerSec * sample_size;
  pcmwf.wBitsPerSample = sample_size * 8;

  memset(&dsbdesc, 0, sizeof(dsbdesc));
  dsbdesc.dwSize = sizeof(DSBUFFERDESC);

  dsbdesc.dwFlags = DSBCAPS_CTRLDEFAULT;
  dsbdesc.dwBufferBytes = 3 * pcmwf.wf.nAvgBytesPerSec; /* 3-second buffer */
  dsbdesc.lpwfxFormat = (LPWAVEFORMATEX)&pcmwf;

  if ( (hr = lpDirectSound->lpVtbl->CreateSoundBuffer(lpDirectSound,
       &dsbdesc, &lpDsb, NULL)) != DS_OK)
  {
    fprintf(stderr,"Couldn't create sound buffer -- waah! (%d)\n",hr);
	exit(1);
  }

  /*if ( lpDsb->lpVtbl->SetVolume(lpDsb,0) != DS_OK)
  {
    fprintf(stderr,"Couldn\'t set volume\n");
	exit(1);
  }*/
  
  return(0);
}

void play_sound(char *buf,int len)
{
	LPVOID lpvPtr1;
	DWORD dwBytes1;
	DWORD status = 0;
	DWORD playcursor, writecursor;

	if (lpDsb->lpVtbl->Lock(lpDsb, 0, len, &lpvPtr1, 
        &dwBytes1, NULL, NULL, 0) != DS_OK)
	{
	  fprintf(stderr,"Couldn\'t lock sound buffer -- waah!\n");
	  exit(1);
	}
	fprintf(stderr,"Locked %d of %d bytes of buffer\n",dwBytes1,len);

	memcpy(lpvPtr1,buf,len);
	/*write(1,buf,len);*/

	if (lpDsb->lpVtbl->Unlock(lpDsb,lpvPtr1, dwBytes1, NULL, 0) != DS_OK)
	{
	  fprintf(stderr,"Couldn\'t UN-lock sound buffer -- waah!\n");
	  exit(1);
	}

	if (lpDsb->lpVtbl->Play(lpDsb,0,0,0) != DS_OK)
	{
	  fprintf(stderr,"Couldn\'t play sound buffer -- waah!\n");
	  exit(1);
	}

	lpDsb->lpVtbl->GetStatus(lpDsb,&status);
	fprintf(stderr,"Status: %d\n",status);
	
	if (lpDsb->lpVtbl->GetCurrentPosition(lpDsb,&playcursor,&writecursor) != DS_OK)
	{
	  fprintf(stderr,"GetCurrentPosition failed\n");
	  exit(1);
	}
	Sleep(30);
	fprintf(stderr,"Cursors: %d %d\n",playcursor,writecursor);
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
