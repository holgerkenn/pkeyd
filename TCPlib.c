/* passivesock.c - passivesock */
#include <autoconf.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "TCPlib.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif
#if !defined(__linux__)
int socket(int,int,int); /* Selfmade Socket Prototype ;-) */
int bind(int,struct sockaddr *,int); /* Selfmade bind Prototype  */
int listen(int,int); /* Selfmade listen Prototype */
int connect(int,struct sockaddr *,int); /* Selfmade connect Prototype */
int inet_addr(char *);
#endif

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif
extern int	errno;
//extern char	*sys_errlist[];
FILE	*tcplogfile;
int	dlevel;

char timestring[40]; /* For now() */

/*u_short	htons(), ntohs();*/

#ifndef	INADDR_NONE
#define	INADDR_NONE	0xffffffff
#endif	/* INADDR_NONE */

/*u_long	inet_addr(char * host); */


u_short	portbase = 0;		/* port base, for non-root servers	*/

/*------------------------------------------------------------------------
 * passivesock - allocate & bind a server socket using TCP or UDP
 *------------------------------------------------------------------------
 */
int passivesock(char *service,char *protocol,int qlen )
	/* service associated with the desired port	*/
	/* name of protocol to use ("tcp" or "udp")	*/
	/* maximum length of the server request queue	*/
{
	struct servent	*pse;	/* pointer to service information entry	*/
	struct protoent *ppe;	/* pointer to protocol information entry*/
	struct sockaddr_in sin;	/* an Internet endpoint address		*/
	int	s, type;	/* socket descriptor and socket type	*/

	memset((char *)&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;

    /* Map service name to port number */
	if ( pse = getservbyname(service, protocol) )
		sin.sin_port = htons(ntohs((u_short)pse->s_port)
			+ portbase);
	else if ( (sin.sin_port = htons((u_short)atoi(service))) == 0 )
		{
		errmsg(2,"can't get \"%s\" service entry\n", service);
		return ERR_NO_SERVICE;
		}

    /* Map protocol name to protocol number */
	if ( (ppe = getprotobyname(protocol)) == 0)
		{
		errmsg(2,"can't get \"%s\" protocol entry\n", protocol);
		return ERR_NO_PROTOCOL;
		}

    /* Use protocol to choose a socket type */
	if (strcmp(protocol, "udp") == 0)
		type = SOCK_DGRAM;
	else
		type = SOCK_STREAM;

    /* Allocate a socket */
	s = socket(PF_INET, type, ppe->p_proto);
	if (s < 0)
		{
		errmsg(2,"can't create socket: %s\n", strerror(errno));
		return ERR_NO_SOCKET;
		}

    /* Bind the socket */
	if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0){
		errmsg(1,"Waiting for free socket...");
		while (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0);
		errmsg (1,"OK\n");
	}
	if (type == SOCK_STREAM && listen(s, qlen) < 0)
		{
		errmsg(2,"can't listen on %s port: %s\n", service,
			strerror(errno));
		return ERR_CANT_LISTEN;
		}
	return s;
}
/* connectsock.c - connectsock */


/*------------------------------------------------------------------------
 * connectsock - allocate & connect a socket using TCP or UDP
 *------------------------------------------------------------------------
 */
int connectsock(char *host,char *service,char *protocol )
	/* name of host to which connection is desired	*/
	/* service associated with the desired port	*/
	/* name of protocol to use ("tcp" or "udp")	*/
{
	struct hostent	*phe;	/* pointer to host information entry	*/
	struct servent	*pse;	/* pointer to service information entry	*/
	struct protoent *ppe;	/* pointer to protocol information entry*/
	struct sockaddr_in sin;	/* an Internet endpoint address		*/
	int	s, type;	/* socket descriptor and socket type	*/


	memset((char *)&sin,0, sizeof(sin));
	sin.sin_family = AF_INET;

    /* Map service name to port number */
	if ( pse = getservbyname(service, protocol) )
		sin.sin_port = pse->s_port;
	else if ( (sin.sin_port = htons((u_short)atoi(service))) == 0 )
		{
		errmsg(2,"can't get \"%s\" service entry\n", service);
		return ERR_NO_SERVICE;
		}

    /* Map host name to IP address, allowing for dotted decimal */
	if ( phe = gethostbyname(host) )
		memcpy((char *)&sin.sin_addr,phe->h_addr, phe->h_length);
	else if ( (sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE )
		{
		errmsg(2,"can't get \"%s\" host \n", host);
		return ERR_NO_HOST;
		}


    /* Map protocol name to protocol number */
	if ( (ppe = getprotobyname(protocol)) == 0)
		{
		errmsg(2,"can't get \"%s\" protocol entry\n", protocol);
		return ERR_NO_PROTOCOL;
		}


    /* Use protocol to choose a socket type */
	if (strcmp(protocol, "udp") == 0)
		type = SOCK_DGRAM;
	else
		type = SOCK_STREAM;

    /* Allocate a socket */
	s = socket(PF_INET, type, ppe->p_proto);
	if (s < 0)
		{
		errmsg(2,"can't create socket: %s\n", strerror(errno));
		return ERR_NO_SOCKET;
		}


    /* Connect the socket */
	if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		{
		errmsg(2,"can't connect to %s.%s: %s\n", host, service,
			strerror(errno));
		return ERR_CANT_CONNECT;
		}
	return s;
}
/* errexit.c - errexit */

/**********************************************************************/
/* Time Function                                                      */
/**********************************************************************/

char *now()
{
time_t ti;
struct tm *tm;
ti = time(NULL); tm = localtime(&ti);
sprintf(timestring,     "%02d/%02d %02d:%02d:%02d ",
                                tm->tm_mon+1,  tm->tm_mday,
                                tm->tm_hour, tm->tm_min, tm->tm_sec);
return timestring;
}

/*------------------------------------------------------------------------
 * errexit - print an error message and exit
 *------------------------------------------------------------------------
 */
/*VARARGS1*/
int errexit(char *format,...)
{
	va_list	args;
	fprintf(tcplogfile,	"\n%s",now());
	va_start(args,format);

#if defined(__linux__)
	vfprintf(tcplogfile,format,args);
#else
	_doprnt(format, args,tcplogfile);
#endif
	va_end(args);

	fclose(tcplogfile);
	exit(1);
}
/*VARARGS1*/
int errmsg(int d,char *format,...)
{
	va_list	args;
	if (d>=dlevel) return(0);
	fprintf(tcplogfile,	"\n%s",now());
	va_start(args,format);
#if defined(__linux__)
	vfprintf(tcplogfile,format,args);
#else
	_doprnt(format, args,tcplogfile);
#endif
	va_end(args);

	fflush(tcplogfile);
}
/* connectTCP.c - connectTCP */

/*------------------------------------------------------------------------
 * connectTCP - connect to a specified TCP service on a specified host
 *------------------------------------------------------------------------
 */
int connectTCP(char *host,char *service )
	/* name of host to which connection is desired	*/
	/* service associated with the desired port	*/
{
	return connectsock( host, service, "tcp");
}
/* connectUDP.c - connectUDP */

/*------------------------------------------------------------------------
 * connectUDP - connect to a specified UDP service on a specified host
 *------------------------------------------------------------------------
 */
int connectUDP(char *host,char *service )
	/* name of host to which connection is desired	*/
	/* service associated with the desired port	*/
{
	return connectsock(host, service, "udp");
}
/* passiveTCP.c - passiveTCP */

/*------------------------------------------------------------------------
 * passiveTCP - create a passive socket for use in a TCP server
 *------------------------------------------------------------------------
 */
int passiveTCP(char *service,int qlen )
	/* service associated with the desired port	*/
	/* maximum server request queue length		*/
{
	return passivesock(service, "tcp", qlen);
}
/* passiveUDP.c - passiveUDP */

/*------------------------------------------------------------------------
 * passiveUDP - create a passive socket for use in a UDP server
 *------------------------------------------------------------------------
 */
int passiveUDP(char *service )
	/* service associated with the desired port	*/
{
	return passivesock(service, "udp", 0);
}
