/* $Id: waddle.h,v 1.8 1997/10/14 22:42:37 mdz Exp $
 *
 * $Log: waddle.h,v $
 * Revision 1.8  1997/10/14 22:42:37  mdz
 * Added interval timers for prerecorded sound
 *
 * Revision 1.7  1997/10/11 17:20:25  mdz
 * Cleaned up
 *
 *
 */

/* WADDLE - waddle.h
 *
 * Declarations
 *
 */


#ifndef WADDLE_H
#define WADDLE_H

/**** Configurable options ****/

#define DEFAULT_PORT 10001

/* Using an integer power of 2 makes the sound driver happy */
#define REAL_BUFFERLEN 4096

/**** End of configurable options ****/

#define BUFFERLEN REAL_BUFFERLEN /* This would include any header */

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

#ifdef LINUX
#define HAVE_ITIMER
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

enum {LEFT_CHANNEL,RIGHT_CHANNEL};

void sender(unsigned int delay,
	    int sock_left,struct sockaddr_in *sin_left,
	    int sock_right,struct sockaddr_in *sin_right);
void receiver(int sock);
void usage(void);
inline void send_datagram(int sock,struct sockaddr_in *sin,char *buf,unsigned int len);
inline int receive_datagram(int sock,char *buf,int len);
int sound_setup(int dev,int sampling_rate,int sample_size,int channels);
void get_channel(const char *src,char *dst,int len,int which);

void generic_play_sound(char *buf,unsigned int len);
void play_sound(char *buf,unsigned int len);
#ifdef WIN32
void write_buffer(char *buf,unsigned int len);
void DS_Error(const char *context,HRESULT hr);
#endif

#endif /* WADDLE_H */
