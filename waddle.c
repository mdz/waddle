#include <stdio.h>
#include <sys/types.h>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#endif

#include <errno.h>

#ifndef WIN32
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#endif

#ifdef WIN32
#include <io.h>
#include "getopt.h"
#endif

#ifdef LINUX
#include <endian.h>
#endif
#ifdef NETBSD
#include <machine/endian.h>
#endif

#include "waddle.h"

/****************************************
 *
 * $Id: waddle.c,v 1.1 1997/09/28 06:35:04 mdz Exp mdz $
 *
 * WADDLE - Wide Area Digital Distribution of Live Entertainment
 *
 * Sends and receives live audio streams over UDP (unicast and multicast)
 *
 * Author: Doctor Z <mdz@csh.rit.edu>
 *
 * History: $Log: waddle.c,v $
 * History: Revision 1.1  1997/09/28 06:35:04  mdz
 * History: Initial revision
 * History:
 * History: Revision 1.4  1997/09/28 06:30:04  mdz
 * History: Wrote most of WIN32 MCI interface.  Abandoned.
 * History: Wrote most of WIN32 DirectSound interface.  Untested.
 * History:
 *
 ****************************************/

/*********** Main **********/
int main(int argc, char *argv[])
{
  int sock1, sock2;
  int on = 1;
  int err;
  struct in_addr addr;
#ifdef WIN32
  WORD wVersionRequested;
  WSADATA WSAData;
#endif
  
  struct sockaddr_in sin1, sin2;
#ifdef HAVE_MULTICAST
  struct ip_mreq mr;
#endif
  int opt;

  /* Command line option handling */
  /* Defaults: 8kHz, 8bit, mono, receiver */
  int sampling_rate = 8000, sample_size = 1, channels = 1, mux = 0;
  int am_sender = 0;
  extern char *optarg;
  extern int optind;
  
  while ( (opt = getopt(argc,argv, "r:qsmhb")) != EOF )
    {
      switch (opt)
	{
	case 'r': /* Sampling rate */
	  sampling_rate = atoi(optarg);
	  break;
	case 'q': /* High-quality mode (16-bit samples) */
	  sample_size = 2;
	  break;
	case 's': /* Stereo mode */
	  channels = 2;
	  break;
	case 'b': /* Broadcast mode */
#ifdef WIN32
      fprintf(stderr,"Broadcast mode doesn't work under WIN32.  Get a real OS.\n");
	  exit(1);
#endif
	  am_sender = 1;
	  break;
	case 'h': /* Help */
	  usage(); /* does not return */
	default:
	  usage(); /* does not return */
	}
    }

  if ( (argc - optind) < 1 )
    {
      usage();
    }

  if ( (argc - optind) == 2 )
    {
      /* Two more arguments (IP addresses) */
      mux = 1;
    }

  if ( mux && ( channels == 1 ) )
    {
      fprintf(stderr,"Multiplexing mode may only be used with stereo mode\n");
      exit(1);
    }

  sin1.sin_family = AF_INET;
  sin1.sin_port = htons(PORT);

  /* First non-option argument */
  sin1.sin_addr.s_addr = inet_addr(argv[optind]);
  
#ifdef WIN32
  wVersionRequested = MAKEWORD(1,1);
  if ( (err = WSAStartup(wVersionRequested,&WSAData)) != 0)
    {
      fprintf(stderr,"Couldn't find a suitable winsock -- barf!\n");
      exit(1);
    }
#endif WIN32

  sock1 = socket(AF_INET,SOCK_DGRAM,0);
  if (sock1 < 0)
    {
      SOCKERR("socket");
      exit(1);
    }

  if (setsockopt(sock1, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0
      )
    {
      SOCKERR("setsockopt: SO_REUSEADDR");
      exit(1);
    }

  if (mux) /* Initialize second socket for multiplex mode */
    {
      sin2.sin_family = AF_INET;
      sin2.sin_port = htons(PORT);
	 
      sin2.sin_addr.s_addr = inet_addr(argv[optind]);
	 
      sock2 = socket(AF_INET,SOCK_DGRAM,0);
      if (sock2 < 0)
	{
	  SOCKERR("socket");
	  exit(1);
	}
    }

  if (am_sender)
    {
      fprintf(stderr,"Sender...\n");
  
      /* stdin */
    if (sound_setup(0,sampling_rate,sample_size,channels) < 0)
	{
	  fprintf(stderr,"I don't see a sound device\nHope you know what you're doing...\n");
	}

      if (mux)
	{
	  sender(sock1,&sin1,sock2,&sin2);
	}
      else
	{
	  sender(sock1,&sin1,-1,NULL);
	}
    }
  else
    {
      fprintf(stderr,"Receiver...\n");

	 
      addr = sin1.sin_addr; 
      sin1.sin_addr.s_addr = 0;
      if (bind(sock1, (struct sockaddr *)&sin1, sizeof(sin1)) < 0)
	{
	  SOCKERR("bind");
	  exit(1);
	}
    
      sin1.sin_addr = addr; /* The original address to listen to */
#ifdef HAVE_MULTICAST
      if (IS_MULTICAST(sin1.sin_addr.s_addr))
	{
	  fprintf(stderr,"Multicast mode!\n");
	  
	  mr.imr_multiaddr.s_addr = sin1.sin_addr.s_addr;
	  mr.imr_interface.s_addr = INADDR_ANY;
	  
	  if (setsockopt(sock1,IPPROTO_IP,IP_ADD_MEMBERSHIP,
			 (char *)&mr, sizeof(mr)) < 0)
	    {
	      SOCKERR("setsockopt: IP_ADD_MEMBERSHIP");
	      exit(1);
	    }
	}
#endif
    
	#ifdef WIN32
	sound_setup(0,sampling_rate,sample_size,(mux)?(1):(channels));
	#else  
    if (!isatty(1)) /* Allow for manual test procedures */
	{
	  /* Use mono output mode if we're multiplexing */
	  sound_setup(1,sampling_rate,sample_size,(mux)?(1):(channels)); /* stdout */
	}
	#endif

      receiver(sock1);
    }

  return(0);
}

/*
 * Command-line usage
 *
 */
void usage(void)
{
  fprintf(stderr,"Usage:\n");
  fprintf(stderr,"waddle [options] <left ip address> [right ip address]\n\n");
  fprintf(stderr,"Options:\n");
  fprintf(stderr,"\ts\t- Enables stereo mode (default: disabled)\n");
  fprintf(stderr,"\tr <arg>\t- Sets sampling rate (default: 8kHz)\n");
  fprintf(stderr,"\tq\t- Enables high-quality mode with 16-bit samples (default: 8-bit)\n");
  fprintf(stderr,"\tb\t- Enables broadcast (sender) mode\n");
  fprintf(stderr,"\nSpecify two IP addresses on sender for left-right multiplexing mode\n");
#ifndef WIN32
  fprintf(stderr,"Use sound device as stdin/stdout as appropriate\n");
#endif

  exit(-1);
}

/*
 * The sender 
 *
 */
void sender(int sock_left,struct sockaddr_in *sin_left,
	    int sock_right,struct sockaddr_in *sin_right)
{
  char buf[BUFFERLEN];
  char buf_chan[BUFFERLEN / 2];
  int count;

  for (;;) /* For-ev-er */
    {
      if ( (count = read(0,buf,sizeof(buf))) < 0)
	{
	  SOCKERR("read");
	  exit(1);
	}
      else if (count == 0) /* EOF */
	{
	  exit(0);
	}
      else
	{
	  if (sin_right != NULL) /* Multiplexing mode */
	    {
	      fprintf(stderr,"Multiplexing is untested -- watch out\n");
	      
	      get_channel(buf,buf_chan,count,LEFT_CHANNEL);
	      printf("Sending %d bytes\n",count);
	      if (sendto(sock_left,buf_chan,count,0,
			 (struct sockaddr *)sin_left,sizeof(*sin_left)) < 0)
		{
		  SOCKERR("sendto");
		  exit(1);
		}
	      get_channel(buf,buf_chan,count,RIGHT_CHANNEL);
	      printf("Sending %d bytes\n",count);
	      if (sendto(sock_right,buf_chan,count,0,
			 (struct sockaddr *)sin_right,sizeof(*sin_right)) < 0)
		{
		  SOCKERR("sendto");
		  exit(1);
		}
	    }
	  else /* Non-multiplexing mode */
	    {
	      printf("Sending %d bytes\n",count);
	      if (sendto(sock_left,buf,count,0,
			 (struct sockaddr *)sin_left,sizeof(*sin_left)) < 0)
		{
		  SOCKERR("sendto");
		  exit(1);
		}
	    }
	}
    }
}

/*
 * The receiver
 *
 */
void receiver(int sock)
{
  char buf[BUFFERLEN];
  int count;
  
  for(;;) /* For-ev-er */
    {
      if ( (count = recvfrom(sock,buf,sizeof(buf),0,			    
			     NULL,NULL)) < 0)
	{
	  SOCKERR("recvfrom");
	  exit(1);
	}
    else
	/* broken - This should account for endian differences */
	#ifdef WIN32
	  play_sound(buf,count);
	#else
	  write(1,buf,count);
	#endif
    }
}
