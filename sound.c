#include <stdio.h>

#ifndef WIN32
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#endif

#include <assert.h>

#include "waddle.h"

#ifdef LINUX
#include <linux/soundcard.h>
#endif

static int sample_size_global;

/*
 * Sound setup
 *
 */
#ifdef LINUX
int sound_setup(int dev,int sampling_rate,int sample_size,int channels)
{
  int format = 0, arg = 0;

  /* Note: always set sample format (size), number of channels, sample rate *
  * in that order */

  /* We need this later for channel extraction */
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

#elif defined(WIN32)

int sound_setup(int dev,int sampling_rate,int sample_size,int channels)
{
  fprintf(stderr,"Win32 sound support is incomplete -- watch out\n");
  return(-1);
}

#else

#error Unsupported sound driver interface

#endif

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
