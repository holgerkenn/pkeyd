/* TCPlib.h  */

#define ERR_NO_SERVICE		-1
	/* Service Entry can`t be resolved */
#define ERR_NO_PROTOCOL		-2
	/* Protocol Entry can`t be resolved */
#define ERR_NO_SOCKET		-3
	/* No Socket Available */
#define ERR_CANT_LISTEN		-4
	/* Can`t listen on Port */
#define ERR_NO_HOST		-5
	/* Hostname or IP-Adress can`t be resolved */
#define ERR_CANT_CONNECT	-6
	/* Can`t connect to remote host */


/* The logfile filehandle has to be opened in the main module.
   All errors are put into this filehandle. Should point to
   /var/tmp/logfilename or something like this*/

int passivesock(char *service,char *protocol,int qlen );

int connectsock(char *host,char *service,char *protocol );

char *now(); /* gets local time as string */ 

int errexit(char *format,...); /* These go into logfile! */

int errmsg(int d,char *format,...);

int connectTCP(char *host,char *service );

int connectUDP(char *host,char *service );

int passiveTCP(char *service,int qlen );

int passiveUDP(char* service );

