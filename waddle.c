/****************************************
 *
 * $Id: waddle.c,v 1.6 1997/10/11 17:20:35 mdz Exp mdz $
 *
 * WADDLE - Wide Area Digital Distribution of Live Entertainment
 *
 * Sends and receives live audio streams over UDP (unicast and multicast)
 *
 * Author: Doctor Z <mdz@csh.rit.edu>
 *
 * History: $Log: waddle.c,v $
 * History: Revision 1.6  1997/10/11 17:20:35  mdz
 * History: Cleaned up
 * History:
 * History: Revision 1.5  1997/10/06 02:46:55  mdz
 * History: Took out sequence numbers
 * History:
 * History: Revision 1.4  1997/10/01 19:48:31  mdz
 * History: Cleaned up
 * History:
 * History: Revision 1.3  1997/09/28 20:09:45  mdz
 * History: *** empty log message ***
 * History:
 *
 ****************************************/

/*
 * WADDLE (Wide Area Digital Distribution of Live Entertainment)
 *
 * Sends a raw audio stream over UDP to one or more recipients, possibly
 * using IP multicast
 *
 * Matt "Doc" Zimmerman <mdz@csh.rit.edu> 1997
 *
 */

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

#endif /* !WIN32 */

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
#ifdef HAVE_ITIMER
#include <sys/time.h>
#include <signal.h>
#endif /* HAVE_ITIMER */

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
#ifdef HAVE_ITIMER
  unsigned int delay = 0;
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
  int port = DEFAULT_PORT;
  extern char *optarg;
  extern int optind;
  
  while ( (opt = getopt(argc,argv, "r:qsmhbp")) != EOF )
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
	  fprintf(stderr,"Broadcast mode not implemented for WIN32 and I don't care.\n");
	  exit(1);
#endif
	  am_sender = 1;
	  break;
	case 'p': /* Port */
	  port = atoi(optarg);
	  break;
	case 'h': /* Help */
	  usage(); /* does not return */
	default: /* Unknown option */
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
  sin1.sin_port = htons(port);

  /* First non-option argument */
  sin1.sin_addr.s_addr = inet_addr(argv[optind]);
  
#ifdef WIN32
  wVersionRequested = MAKEWORD(1,1);
  if ( (err = WSAStartup(wVersionRequested,&WSAData)) != 0)
    {
      fprintf(stderr,"Couldn't find a suitable winsock -- barf!\n");
      exit(1);
    }
#endif /* WIN32 */

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
      sin2.sin_port = htons(port);
	 
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
	  fprintf(stderr,"I don't see a sound device\nAssuming prerecorded input...\n");
	  /* Number of microseconds of audio that can fit in the buffer */
	  delay = REAL_BUFFERLEN * 1000000.0 /
	    (sampling_rate * sample_size * channels);
	}

      if (mux)
	{
	  sender(delay,sock1,&sin1,sock2,&sin2);
	}
      else
	{
	  sender(delay,sock1,&sin1,-1,NULL);
	}
    }
  else /* receiver */
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
      sound_setup(-1,sampling_rate,sample_size,(mux)?(1):(channels));
#else
      if (!isatty(1)) /* Don't puke to allow for manual test procedures */
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
  fprintf(stderr,"\ts\t- Enables stereo mode (default: mono)\n");
  fprintf(stderr,"\tr <arg>\t- Sets sampling rate (default: 8kHz)\n");
  fprintf(stderr,"\tq\t- Enables high-quality mode with 16-bit samples (default: 8-bit)\n");
  fprintf(stderr,"\tb\t- Enables broadcast (sender) mode\n");
  fprintf(stderr,"\nSpecify two IP addresses on sender for stereo multiplexing mode\n");
#ifndef WIN32
  fprintf(stderr,"Use sound device as stdin/stdout as appropriate\n");
#endif

  exit(-1);
}

/*
 * The sender 
 *
 */
void sender(unsigned int delay,
	    int sock_left,struct sockaddr_in *sin_left,
	    int sock_right,struct sockaddr_in *sin_right)
{
  char buf[REAL_BUFFERLEN];
  char buf_chan[REAL_BUFFERLEN / 2];
  int count;
#ifdef HAVE_ITIMER
  struct itimerval tmval;
#endif

#ifdef HAVE_ITIMER
  signal(SIGALRM,SIG_IGN);
#endif

  fprintf(stderr,"Using delay of %d us\n",delay);

  for (;;) /* For-ev-er */
    {
#ifdef HAVE_ITIMER
      if (delay > 0)
	{
	  tmval.it_interval.tv_sec = tmval.it_interval.tv_usec = 0;
	  tmval.it_value.tv_sec = 0;
	  tmval.it_value.tv_usec = delay;
	  if (setitimer(ITIMER_REAL,&tmval,0) < 0)
	    {
	      perror("setitimer");
	      exit(1);
	    }
	}
#endif /* HAVE_ITIMER */

      if ( (count = read(0,buf,REAL_BUFFERLEN)) < 0)
	{
	  SOCKERR("read");
	  exit(1);
	}
      else if (count == 0) /* EOF */
	{
	  exit(0);
	}
      else /* We have data */
	{
#ifdef HAVE_ITIMER
	  if (delay > 0)
	    {
	      if (getitimer(ITIMER_REAL,&tmval) < 0)
		{
		  perror("getitimer");
		  exit(1);
		}
	      /* Sleep off the remainder of the timer, if there is any */
	      /*fprintf(stderr,"Sleeping %d s + %d us\n",tmval.it_value.tv_sec,
		tmval.it_value.tv_usec);*/
	      sleep(tmval.it_value.tv_sec);
	      usleep(tmval.it_value.tv_usec);
	    }
#endif /* HAVE_ITIMER */

	  if (sin_right != NULL) /* Multiplexing mode */
	    {
	      fprintf(stderr,"Multiplexing is untested -- watch out\n");
	      
	      get_channel(buf,buf_chan,count,LEFT_CHANNEL);
	      /* printf("Sending %d bytes (left)\n",count); */
	      send_datagram(sock_left,sin_left,buf_chan,count);

	      get_channel(buf,buf_chan,count,RIGHT_CHANNEL);
	      /* printf("Sending %d bytes (right)\n",count); */
	      send_datagram(sock_right,sin_right,buf_chan,count);
	    }
	  else /* Non-multiplexing mode */
	    {
	      /* printf("Sending %d bytes\n",count); */
	      send_datagram(sock_left,sin_left,buf,count);
	    }
	}
    }
}

inline void send_datagram(int sock,struct sockaddr_in *sin,char *buf,unsigned int len)
{
  if (sendto(sock,buf,len,0,(struct sockaddr *)sin,sizeof(*sin)) < 0)
    {
      SOCKERR("sendto");
      exit(1);
    }
}

/*
 * The receiver
 *
 */
void receiver(int sock)
{
  char buf[REAL_BUFFERLEN];
  int count;
  
  for(;;) /* For-ev-er */
    {
      count = receive_datagram(sock,buf,sizeof(buf));
      /* XXX - This should account for endian differences someday */
      generic_play_sound(buf,count);
    }
}

inline int receive_datagram(int sock,char *buf,int len)
{
  int count;

  if ( (count = recvfrom(sock,buf,len,0,
			 NULL,NULL)) < 0)
    {
      SOCKERR("recvfrom");
      exit(1);
    }

  return(count);
}
