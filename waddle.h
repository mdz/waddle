#ifndef WADDLE_H
#define WADDLE_H

/*#define LINUX*/

/**** Configurable options ****/

#define PORT 10001

/* Enough to buffer .5 seconds of audio */
/*#define BUFFERLEN ((SAMPLING_RATE * CHANNELS * SAMPLE_SIZE) / 2)*/

/* Perhaps we should use an integer power of 2 instead - makes the sound */
/* driver happy */
#define BUFFERLEN 4096

/**** End of configurable options ****/

#ifdef LINUX
#include <linux/soundcard.h>
#endif

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <dsound.h> /* If we include this before winsock, it blows up.  $#@@# Microsoft */
#endif

#ifdef IP_ADD_MEMBERSHIP
#define HAVE_MULTICAST /* Can receive multicasts */
#else
#pragma warning No multicast support
#endif

#ifdef HAVE_MULTICAST

/* 224.0.0.0 .. 239.255.255.255 */
#define IS_MULTICAST(addr) ((htonl(addr) & 0xf0000000) == 0xe0000000)

#endif

#ifdef WIN32
#define SOCKERR(str) fprintf(stderr,"%s: winsock error %d\n",str,WSAGetLastError())
#else
#define SOCKERR(str) perror(str)
#endif

#define LEFT_CHANNEL 0
#define RIGHT_CHANNEL 1

void sender(int sock_left,struct sockaddr_in *sin_left,
	    int sock_right,struct sockaddr_in *sin_right);
void receiver(int sock);
void usage(void);

int sound_setup(int dev,int sampling_rate,int sample_size,int channels);
void get_channel(const char *src,char *dst,int len,int which);

#ifdef WIN32
void play_sound(char *buf,int len);
#endif

#endif /* WADDLE_H */
