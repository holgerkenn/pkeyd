/**********************************************************************/
/* Debug Definitions							      */
/**********************************************************************/
/* #define SERVER_MODE */
/* #define NS_QUERY */

/**********************************************************************/
/* Includes							      */
/**********************************************************************/
#include <autoconf.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <gdbm.h>
#include <netdb.h>
#include <TCPlib.h>
#if defined(__svr4__)
#include <fcntl.h>
#include <sys/termios.h>
#endif

#include <defs.h>
#include <local.h>

/**********************************************************************/
/* Definitions							      */
/**********************************************************************/

#define VERSIONSTRING "$Id: pkeyd.c,v 1.2 1997/05/19 14:10:40 kenn Exp kenn $\n"
#define	QLEN		   5	/* maximum connection queue length	 */
#define	BUFSIZE		1024
#define MAXLINE		256	/* Maximale Zeichen in einer Zeile */
#define MAXLINES	256	/* Maximale Anzahl der Routing-Eintraege */
#define CMD_HELP	1
#define CMD_ERROR	0
#define CMD_GET		2
#define CMD_DIR		3
#define CMD_QUIT	4
#define CMD_SGET	5
#define MAXCOUNT	5 /* Maximum Number of hops */
#define ROUTE_DYNAMIC	1
#define ROUTE_STATIC	0
#define ROUTE_NOFORCE	-1
#define FATHER_DOMAIN   "*"
#define RET_OK		0	/* Antwort OK und enth"alt Daten */
#define RET_ROUTE	1	/* Antwort OK unt enth"alt Route */
#define RET_NO_DOMAIN	-1	/* Endg"ultige Ablehnung */
#define RET_ROUTE_ERR	-2	/* Routing fehler ? */
#define RET_NO_USER	-3	/* User unbekannt */
#define RET_ACK_ERR	-4	/* Zustaendiger Server, aber Fehler */
#define RET_NACK_ERR	-5	/* Nicht zust"andig, aber Feherl */
#define RET_INT_ERR	-6	/* Fehler vor Bestimmung der Zust. */
#define RET_SYN_ERR	-7	/* Syntax Error in ExtractQuery */

/**********************************************************************/
/* Global Arrays						      */
/**********************************************************************/

char            keydb[MAXLINE];
char            mydomain[MAXLINE];
char            myadress[MAXLINE];
char            routefilename[MAXLINE];
char            tmproutefilename[MAXLINE];
char           *service = SERVICE;	/* service name or port number	 */

char * Chop(char *,char *);

extern int      errno;
//extern char    *sys_errlist[];
extern FILE    *tcplogfile;
extern int      dlevel;
FILE           *configfile;
int             lfile;

/* Routing-Table */
typedef struct route_entry {
	char           *server;
	char           *domain;
	int             flag;
}               r_entry;


r_entry         route[MAXLINES];
int             rmax = 0;	/* Anzahl der Eintraege in der Routetable */

int             HandleRequest(int);
int             reaper();
int             cleanup();
int             server_exit;
int             msock;		/* master server socket		 */




void DoServer()
{
#if defined(SERVER_MODE)	/* server-specific behaviour */
	int i,fd;
	i = fork();
	if (i < 0) {
		fprintf(stderr, "error in fork: %s\n", sys_errlist[errno]);
		exit(1);
	}
	if (i) {
		printf("pkeyd PID: %d\n", i);
		exit(0);	/* OK */
	}
	/* forking away, no & is needed */
	for (i = getdtablesize() - 1; i >= 0; --i)
		(void) close(i);
	/* Now stdin, stdout and stderr are closed */
	fd = open("/dev/tty", O_RDWR);
	(void) ioctl(fd, TIOCNOTTY, 0);
	(void) close(fd);
	/* No TTY any more */
#endif

}

/**********************************************************************/
/* Main 							      */
/**********************************************************************/

int 
main(int argc, char *argv[])
{
	struct sockaddr_in fsin;/* the address of a client	 */
	int             alen;	/* length of client's address	 */
	int             ssock;	/* slave server socket		 */
	int             i;
	int             fd;
	char            pbuf[10];	/* Buffer for PID */
	char		outbuf[BUFSIZE];
	char		line[MAXLINE];
	char		*ptr;
	printf(VERSIONSTRING);
	switch (argc) {
	case 1:
		break;
	case 2:
		service = argv[1];
		break;
	default:
		fprintf(stderr, "usage: %s [port]\n",argv[0]);
		exit(1);
	}


#if defined(DEBUG)
	dlevel = 5;
#else
	dlevel = 3;
#endif


	DoServer();
	(void) chdir(SECUREDIR);
	/* secure directory */

	(void) umask(027);
	/* secure umask */


#if defined(SERVER_MODE)
#if defined(__svr4__) || defined(SYSTYPE_SVR4) || defined(__linux__)
	(void) setpgrp();
#else
	(void) setpgrp(0, getpid());
#endif
	/* private process group */
	fd = open("/dev/null", O_RDWR);	/* new STDIN */
	(void) dup(fd);		/* stdout */
	(void) dup(fd);		/* stderr */
#endif

	lfile = open(LOCKFILE, O_RDWR | O_CREAT, 0640);
	if (lfile < 0)
		exit(1);	/* Error creating lock */

#if defined(__svr4__) || defined(SYSTYPE_SVR4)
	if (lockf(lfile, (F_TLOCK), 0))
#else
	if (flock(lfile, (LOCK_EX | LOCK_NB)))
#endif
		exit(0);	/* Lock not exclusive, server already running */


	(void) sprintf(pbuf, "%6d\n", getpid());
	(void) write(lfile, pbuf, strlen(pbuf));


	/* Lockfile OK, now doing Logfile */
#if defined(SERVER_MODE)
	tcplogfile = fopen(LOGFILE, "a");
	if (tcplogfile == NULL) {
		exit(1);
	}
	errmsg(1, "Logfile open");
	errmsg(2, "Logfile is %s", &LOGFILE);
#else

	tcplogfile = stdout;
	errmsg(2, "Logfile to STDOUT");
#endif
	errmsg(0, "Debuglevel is %d", dlevel);
	errmsg(2, "Lockfile is %s", &LOCKFILE);

	/* Defaults schreiben */
	strcpy(routefilename, ROUTEFILE);
	strcpy(mydomain, MYDOMAIN);
	strcpy(myadress, MYADRESS);
	strcpy(keydb, KEYDB);
	/* Configfile ueberschreibt defaults */
	configfile = fopen(CONFIGFILE, "r");
	if (configfile == NULL) {
		errmsg(1, "No config file %s, using defaults", &CONFIGFILE);
	} else {
		errmsg(1, "Reading config file %s", &CONFIGFILE);
		ReadConfigFile(configfile);
		/* read config file */
	}

	strcpy(tmproutefilename, routefilename);
	strcat(tmproutefilename, ".tmp");

	PrintConfig();
	ReadRoute(routefilename);
	PrintRoute();
	SortRoute();
	PrintRoute();
	WriteRoute(tmproutefilename);

	server_exit = 1;

	msock = passiveTCP(service, QLEN);
	if (msock < 0)
		errexit("passiveTCP failed, exit %d\n", msock);

	(void) signal(SIGCHLD, reaper);
	(void) signal(SIGINT, cleanup);
	(void) signal(SIGQUIT, cleanup);
	(void) signal(SIGTERM, cleanup);

	errmsg(1, "%s ready for connections on port %s\n", argv[0],service);

/**********************************************************************/
/* MAIN LOOP							      */
/**********************************************************************/


	while (server_exit) {
		alen = sizeof(fsin);
		ssock = accept(msock, (struct sockaddr *) & fsin, &alen);
		if (ssock < 0) {
			if (errno == EINTR)
				continue;
			errexit("accept: %s\n", strerror(errno));
		}
		switch (fork()) {
		case 0:	/* child */
			(void) close(msock);
			exit(HandleRequest(ssock));
		default:	/* parent */
			(void) close(ssock);
			break;
		case -1:
			errexit("fork: %s\n", strerror(errno));
		}
	}
}

/**********************************************************************/
/* Identity							      */
/**********************************************************************/



int 
WhoIsIt(int fd)
{
	struct sockaddr_in peer;
	struct hostent *peername;
	int             peerlen = sizeof(struct sockaddr_in);
	peer.sin_family = AF_INET;
	peer.sin_port = 5000;
	errmsg(1, "incomming connection ");
	if (!getpeername(fd, &peer, &peerlen)) {
		/*
		 * errmsg(2,"from %x:%d", peer.sin_addr.S_un.S_addr,
		 * peer.sin_port);
		 *//* Hex addr */
		/*
		 * errmsg(2,"from %d.%d.%d.%d:%d",
		 * peer.sin_addr.S_un.S_un_b.s_b1,
		 * peer.sin_addr.S_un.S_un_b.s_b2,
		 * peer.sin_addr.S_un.S_un_b.s_b3,
		 * peer.sin_addr.S_un.S_un_b.s_b4, peer.sin_port);
		 */
#ifdef NS_QUERY
		peername = gethostbyaddr(&peer.sin_addr, 4, AF_INET);
		if ((peername) && (peername->h_name)) {
			errmsg(1, "from %s:%d", peername->h_name, peer.sin_port);
		}
#endif

		/* hier kommt der identd-query hin */

		return (0);
	} else {
		errmsg(2, " error finding peer: %s", strerror(errno));
		return (1);
	}

}				/* WhoIsIt */


/**********************************************************************/
/* Routing							      */
/**********************************************************************/

int 
InsertRoute(char *d, char *s, int flag)
{
	int             lend, lens;
	char           *da, *sa;/* d alloc und s alloc */
	int i;
	/* haben wir schon eine Route fuer diese Domain ? */

	errmsg(4,"InsertRoute called for Domain %s, Server %s,Flag %d",d,s,flag);

	for (i = 0; i < rmax; i++) {
		if (!strcmp(d, route[i].domain)) {
			errmsg(3, "Route entry for %s already present",d);
			errmsg(3, "Routeserver is %s", route[i].server);
			errmsg(3, "Flag is %d", route[i].flag);
			if ((route[i].flag==ROUTE_DYNAMIC) && (flag==ROUTE_DYNAMIC)) { 
				errmsg(3,"Removing old dynamic route entry");
				RemoveRoute(d);
			}
		}
	}
	lend = strlen(d)+1;
	lens = strlen(s)+1;
	da = (char *) malloc(lend);
	if ((rmax >= MAXLINES) || (da == NULL)) {
		errmsg(2, "Memory: first alloc or MAXLINES");
		if (da != NULL)
			free(da);
		return (-1);	/* zuviel oder alloc fuer d ging schief */
	} else {
		sa = (char *) malloc(lens);
		if (sa == NULL) {
			errmsg(2, "Memory: second alloc");
			free(da);	/* ging schief, also wieder freigeben */
			return (-1);
		} else {
			strcpy(da, d);
			strcpy(sa, s);
			route[rmax].domain = da;
			route[rmax].server = sa;
			route[rmax].flag = flag;
			rmax++;
		}
	}
	return (0);
}
int 
FreeRoute()
{
	int             i;
	errmsg(4, "Freeing Routefile");
	for (i = 0; i < rmax; i++) {
		free(route[i].domain);
		free(route[i].server);
	}
	errmsg(4, "OK\n\n");
	rmax=0;
}
int 
comparefunc(r_entry * eins, r_entry * zwei)
{
	int	result;
	int	cmpout;

	if ((!eins->domain) || (!zwei->domain)) {
		errmsg(3,"Error in Sort ???");
		return(0);
	}
	errmsg(4, "Comparefunc %s,%s", eins->domain, zwei->domain);
	if (strcmp(eins->domain,FATHER_DOMAIN)) 
		/* Father Domain wird immer nach hinten sortiert */
		result = (1);
	if (strcmp(zwei->domain,FATHER_DOMAIN))
		result = (-1);
	if (strlen(eins->domain) < strlen(zwei->domain))
		result = (1);
	if (strlen(eins->domain) == strlen(zwei->domain)) {
		/* hier muss jetzt der dynamische Eintrag vor dem 
		   statischen stehen, falls die Domainstrings gleich sind */
		cmpout=strcmp(eins->domain,zwei->domain);
		if (cmpout==0) {
			if ( (eins->flag==ROUTE_DYNAMIC) 
			    || (zwei->flag==ROUTE_DYNAMIC) ) {
				if (eins->flag==ROUTE_DYNAMIC) {
					result=-1;
					/* Der dynamisce Eintrag steht immer an erster
					Stelle */
					} else {
					result=1;
					}
			} else {
				errmsg(3,"Two static routes for same domain ???");
				result=0;
			}
		} else { 
			result = cmpout; /* Nicht gleich -> lexikographisch */
		}
	}
	if (strlen(eins->domain) > strlen(zwei->domain))
		result = (-1);
	errmsg(4, "Result is %d", result);
	return (result);
}

int 
SortRoute()
{
	errmsg(4, "Sorting routefile");
	qsort(&route, rmax, sizeof(r_entry), comparefunc);

	/* nix */
	return (0);
}


char           *
GetRoute(char *qdomain)
{
	int             ret;
	char           *domain;
	char           *server;

	if (!GetFullRoute(qdomain, &domain, &server)) {
		return (server);
	} else {
		return ("");
	}
}

int 
mystrcmp(char *s1, char *s2)
{
	int             i, ret, s1l, s2l;
	ret = 0;
	if (!strcmp(s2,FATHER_DOMAIN))
		return(0);
	s1l = strlen(s1);
	s2l = strlen(s2);
	if (s1l < s2l)
		return (1);	/* Kein Praefix, zu kurz */
	while ((s1l > 0) && (s2l > 0)) {
		if (s1[s1l] != s2[s2l]) {
			ret = 1;
		}
		--s1l, --s2l;
	}
	return (ret);
}

int 
GetFullRoute(char *qdomain, char **domain, char **server, int *flag)
{
	int             i;
	errmsg(4, "in GetFullRoute, qdomain is %s",qdomain);
	FreeRoute();
	ReadRoute(tmproutefilename);
	SortRoute();
	PrintRoute();

	for (i = 0; i < rmax; i++) {
		if (!mystrcmp(qdomain, route[i].domain)) {
			errmsg(4, "Routeserver is %s, Domain is %s", route[i].server,route[i].domain);
			*domain = route[i].domain;
			*server = route[i].server;
			errmsg(4, "Flag is %d", route[i].flag);
			flag = &(route[i].flag);
			errmsg(4, "zugewiesen und zurueckgegeben");
			return (0);
		}
	}
	errmsg(3, "GetFullRoute = -1, no server found");
	return (-1);
}

int 
RemoveRoute(char *domain)
{
	int             i,j;
	for (i = 0; i < rmax; i++) {
		if (!strcmp(domain, route[i].domain)) {
			if (route[i].flag == ROUTE_DYNAMIC) {
				errmsg(4, "Removing Server %s for Domain %s"
				       ,route[i].server, route[i].domain);
				free(route[i].domain);
				free(route[i].server);
				--rmax;
				for (j=i;j<rmax;j++) { 
					route[j].domain=route[j+1].domain; 
					route[j].server=route[j+1].server; 
					route[j].flag=route[j+1].flag; 
					}

				return (0); /* Success */
			} else {
				errmsg(3, "remove called for static entry Server %s Domain %s"
				       ,route[i].server, route[i].domain);
				return(1); 
			}
		}
	}
	errmsg(3,"Remove called for nonexistant route to %s",domain);
	return (1); 
}

int 
PrintRoute()
{
	int             i;
	errmsg(4, "Printing Routing Table");
	for (i = 0; i < rmax; i++) {
		errmsg(4, "Server for Domain %s is %s, %s"
		       ,route[i].domain, route[i].server,
		 ((route[i].flag == ROUTE_DYNAMIC) ? "DYNAMIC" : "STATIC"));
	}
	return (0);
	errmsg(4, "End of Routing Table");
}
int 
WriteRoute(char *filename)
{
	int             i;
	FILE *	routefile;
	routefile = fopen(filename, "w");
	if (routefile == NULL) {
		errmsg(1, "No tmproutefile ??? HELP !");
	} else {

	errmsg(4, "Writing Routing Table %s",filename);
	fprintf(routefile,"# %s :Temp Routefile written by Process %d\n",now(),getpid());
	for (i = 0; i < rmax; i++) {
		fprintf(routefile,"%s\t\t%s %d\n",route[i].domain,route[i].server,route[i].flag);
	}

	fclose(routefile);
	}
	return (0);
}


char * Chop(char *ptr,char *line)
{
int	linelen;
	linelen=0;

while (1) {
	if ((ptr[0]==0) || (linelen==MAXLINE)) {
		line[linelen]=0; /* Stringende */
		return(NULL); /* Ende der Vorstellung */
	}
	if (ptr[0]=='\n') {
		line[linelen]=0; /* Stringende */
		return(++ptr); /* Geht noch weiter */
	}
	line[linelen++]=ptr[0];ptr++;
}

}



int
InsertTextRoute(char * line, int defaultpflag, int forcepflag)
/* defaultpflag wird benutzt, wenn kein pflag in der route-Zeile steht */
/* forcepflag ueberschreibt auf jeden Fall defaultpflag und den Eintrag */
{
	char            pdomain[MAXLINE];
	char            pserver[MAXLINE];
	int		pflag;
	int		ret;
/*	do { */
		pflag=defaultpflag; /* default */
		ret=sscanf(line, "%s %s %d", pdomain, pserver,&pflag);
		errmsg(4, "InsertTextRoute : Server for Domain %s is %s, Flag %s", pdomain, pserver,((pflag == ROUTE_DYNAMIC) ? "DYNAMIC" : "STATIC"));
		if ((strlen(pdomain) != 0) && (strlen(pserver) != 0)) {
			if (forcepflag!=ROUTE_NOFORCE) pflag=forcepflag;
			InsertRoute(pdomain, pserver, pflag);
			}
/*	}
	while (ret!=EOF); */
}

int 
ReadRoute(char * filename)
{
	char            line[MAXLINE];
	char            pdomain[MAXLINE];
	char            pserver[MAXLINE];
	int		pflag;
	FILE * routefile;
	routefile = fopen(filename, "r");

	if (routefile == NULL) {
		errmsg(1, "No routefile, running local");
	} else {
	errmsg(2, "Reading routefile %s", routefilename);

	while (!feof(routefile))
		if (fgets(line, MAXLINE, routefile))
			if (line[0] != '#') {
				InsertTextRoute(line,ROUTE_STATIC,ROUTE_NOFORCE);
			}


	fclose(routefile);
	}

}

/**********************************************************************/
/* Config file							      */
/**********************************************************************/

int 
ReadConfigFile(FILE * configfile)
{
	char            line[MAXLINE];
	char            tag[MAXLINE];
	char            value[MAXLINE];
	char            firstword[MAXLINE];

	while (!feof(configfile))	/* File nicht leer */
		if (fgets(line, MAXLINE, configfile))	/* Zeile gelesen */
			if (line[0] != '#') {	/* Keine Kommentarzeile */
				sscanf(line, "%s = %s", tag, value);
				if (!strcasecmp(tag, "routefile")) {
					strcpy(routefilename, value);
				}
				if (!strcasecmp(tag, "mydomain")) {
					strcpy(mydomain, value);
				}
				if (!strcasecmp(tag, "myadress")) {
					strcpy(myadress, value);
				}
				if (!strcasecmp(tag, "keydb")) {
					strcpy(keydb, value);
				}
			}
}				/* ReadConfigFile */

int 
PrintConfig()
{
	errmsg(4, "routefile is %s", routefilename);
	errmsg(4, "tmproutefile is %s", tmproutefilename);
	errmsg(4, "mydomain is %s", mydomain);
	errmsg(4, "myadress is %s", myadress);
	errmsg(4, "keydb is %s", keydb);
	return (0);
}				/* PrintConfig */

/**********************************************************************/
/* Query Processing						      */
/**********************************************************************/


int 
ExtractQuery(char *inbuf, char *query)
{
	char           *qstart;
	char           *qend;

	errmsg(4, "inbuf: >%s<", inbuf);
	qstart = strchr(inbuf, '<');	/* Sucht < */
	if (qstart) {
		qstart++;
		qend = strchr(qstart, '>');	/* Sucht > */
		if (qend) {
			strncpy(query, qstart, qend - qstart);
			query[qend - qstart] = 0;	/* Ende */
			errmsg(4, "query extracted : >%s<", query);
			return (0);
		}
	}
	errmsg(3, "Couldn't extract query from >%s<", inbuf);
	return (1);
}				/* ExtractQuery */

int 
ExtractHops(char *inbuf)
{
	char           *qstart;
	char           *qend;
	int             hops;
	hops = 0;
	errmsg(4, "inbuf: >%s<", inbuf);
	qstart = strchr(inbuf, '[');	/* Sucht "[" */
	if (qstart) {
		qstart++;
		qend = strchr(qstart, ']');	/* Sucht "]" */
		if (qend) {

			sscanf(qstart, "%d", &hops);
			errmsg(4, "hops extracted : >%d<", hops);
			return (hops);
		}
	}
	errmsg(3, "Couldn't extract hops from >%s<", inbuf);
	return (0);
}				/* ExtractHops */

char           *
XtractDomain(char *query)
{
	char           *dstart;
	dstart = strchr(query, '@');
	if (dstart)
		return (dstart + 1);
	errmsg(3, "no domain");
	return (0);
}				/* XtractDomain */

int 
LocalDir(char *outbuf, char *query)
{
	datum           key, content;
	char           *backkey;
	GDBM_FILE       kdb;
	int             bufcount = 0;
	outbuf[0] = 0;		/* String loeschen */

	kdb = gdbm_open(keydb, 0, GDBM_READER, 0600, 0);
	/* Datenbank fuer Lesen oeffnen */
	if (kdb != NULL) {
		key = gdbm_firstkey(kdb);
		do {
			if (++bufcount < BUFSIZE)
				strcat(outbuf, "<");
			bufcount += strlen(key.dptr);
			if (bufcount < BUFSIZE)
				strcat(outbuf, key.dptr);
			bufcount += 2;
			if (bufcount < BUFSIZE)
				strcat(outbuf, ">\n");
			backkey = key.dptr;	/* aufheben fuer free */
			key = gdbm_nextkey(kdb, key);	/* naechster */
			free(backkey);	/* alten Key freigeben */
		} while (key.dptr != NULL);	/* Noch einer da ?= */
		gdbm_close(kdb);/* Datenbank freigeben */
		return (0);
	} else {
		errmsg(1, "Error: couldn`t open key database");
		return (-1);
	}

}				/* LocalDir */
/**********************************************************************/
/* RemoteQuery							      */
/**********************************************************************/

int 
RemoteQuery(char *outbuf, char *query)
{
	char           *server;
	char            line[BUFSIZE];
	char		*ptr;
	int             s;
	int		dummy;
	int             ret;
	int             status = 0;
	FILE           *sfile;


	server = GetRoute(XtractDomain(query));
	if (!strlen(server)) {
		errmsg(2, "No appropriate server found");
		return (-5);
	}
	errmsg(2,"Connecting Server %s for query %s",server,query);
	errmsg(4, "Vor connect ");
	s = connectTCP(server, service);
	errmsg(4, "Nach connect s=%d", s);
	if (s <= 0) {
		errmsg(3, "Couldn't open Socket for Remote Query");
		if ((s = -5) || (s = -6)) {
			if (!RemoveRoute(XtractDomain(query))) {
				/* Hat funktioniert */
				WriteRoute(tmproutefilename);
				return (-5);
			}
			else
				/* Hat nicht funktioniert */
				return (-5);
		}
		/* Try to remove false temporary route entry */
		return (-5);
	}
	read(s, line, BUFSIZE);
	sfile = fdopen(s, "rw");
	line[0] = 0;
	sprintf(line, "sget <%s>\n", query);
	write(s, line, strlen(line));

	status = 0;

	while ((status != 3)) {	/* File nicht leer */
		errmsg(4, "in while status= %d", status);
		if (fgets(line, MAXLINE, sfile)) {	/* Zeile gelesen */
			if ((!strncasecmp(line, "START", 5)) && (status == 0)) {
				errmsg(4, "START:status 1");
				status = 1;
				continue;
			}
			if (!strncasecmp(line, "END", 3)) {
				errmsg(4, "END: status=2");
				status = 2;
				continue;
			}
			if (status == 1) {
				errmsg(4, "in strcat");
				strcat(outbuf, line);
/*				strcat(outbuf, "\n"); /* Noetig? */
				continue;
			}
			if (status == 2) {
				errmsg(4, "in sscanf");
				dummy=sscanf(line, "RET: %d", &ret);
				errmsg(4,"Status =2, ssccanf returned %d, ret is %d",dummy,ret);
				line[0] = 0;
				strcat(line, "quit\n");
				write(s, line, strlen(line));

				status = 3;
				continue;
			}
		}
	}
	fclose(sfile);
	shutdown(s, 2);
	close(s);

/* So, hier kennen wir ret. Und wenn ret =1, dann haben wir einen Routeeintrag bekommen */
if (ret==1) {
	errmsg(2,"Got Route Entries");
	ptr=outbuf;
	do {
		errmsg(4,"ptr is %s",ptr);
		ptr=Chop(ptr,line);
		errmsg(4,"line is %s, calling InsertTextRoute",line);
		InsertTextRoute(line,ROUTE_DYNAMIC,ROUTE_DYNAMIC); 
		/* Route Entries eintragen */
		} while (ptr!=NULL);
	SortRoute();
	WriteRoute(tmproutefilename);

} else {
	errmsg(2,"Got the answer");
}
	errmsg(4, "State= %d, RET= %d", status, ret);
	return (ret);
}

int 
CacheQuery(char *outbuf, char *query)
{
	errmsg(4, "In CacheQuery");
	return (1);
}

/**********************************************************************/
/* LocalQuery							      */
/**********************************************************************/


int 
LocalQuery(char *outbuf, char *query)
{
	datum           key, content;
	GDBM_FILE       kdb;
	int             foundit = -1;	/* Datenbank ging nicht auf */
	errmsg(2,"Searching local database for query %s",query);
	errmsg(2, "in LocalQuery");
	key.dptr = query;
	/* Anfrage an die lokale Datenbank */
	key.dsize = strlen(key.dptr) + 1;	/* 0 wurde mit abgespeichert
						 * ! */
	kdb = gdbm_open(keydb, 0, GDBM_READER, 0600, 0);
	/* Oeffne Datenbank, Lesen */
	if (kdb != NULL) {
		foundit = -3;
		content = gdbm_fetch(kdb, key);	/* Der Zugriff */
		if (content.dptr != NULL) {	/* Haben wir was gefunden ? */
			foundit = -4;	/* Gefunden, aber zu lang */
			if (key.dsize < BUFSIZE) {	/* Passt's hin */
				strcat(outbuf, content.dptr);	/* Kopieren */
				free(content.dptr);	/* Freigeben */
				foundit = 0;	/* Alles OK */
			}
		}
		gdbm_close(kdb);
	}
	if (foundit==0) {
		errmsg(2,"Success");
		} else { 
		errmsg(2,"Failed");
		}

	return (foundit);
}				/* LocalQuery */


/**********************************************************************/
/* Server Function						      */
/**********************************************************************/

int 
HandleRequest(int fd)
{
	char            inbuf[BUFSIZE];
	char            outbuf[BUFSIZE];
	char            query[BUFSIZE];
	char           *domain;
	char           *sdomain;
	char           *sserver;
	int             sflag;
	int             cc;	/* Char Counter (buffer) */
	int             command;
	int             reget = 0;
	FILE           *fh;
	int             ret;
	int		hopcount;
	WhoIsIt(fd);

	/* Hier folgt die Eingabeschleife */

	do {
		if (write(fd, "%pkeyd>", 7) < 0)
			errexit("echo write: %s\n", strerror(errno));
		/* Prompt */
		reget = 0;
		cc = read(fd, inbuf, (sizeof(inbuf) - 2));	/* Platz fuer 0 */
		if (cc < 0)
			errexit("echo read: %s\n", strerror(errno));
		inbuf[cc] = '\n';
		inbuf[cc + 1] = 0;	/* 0-terminiert */
		/* Eingabe abgeschlossen */

		command = CMD_ERROR;
		if (!strncasecmp(inbuf, "get ", 4))
			command = CMD_GET;
		if (!strncasecmp(inbuf, "sget ", 5))
			command = CMD_SGET;
		if (!strncasecmp(inbuf, "help", 4))
			command = CMD_HELP;
		if (!strncasecmp(inbuf, "dir", 3))
			command = CMD_DIR;
		if (!strncasecmp(inbuf, "quit", 4))
			command = CMD_QUIT;
		if (!strncasecmp(inbuf, "exit", 4))
			command = CMD_QUIT;
		if (!strncasecmp(inbuf, "bye", 3))
			command = CMD_QUIT;

		errmsg(4, "DEBUG: Got Command <%s>", inbuf);
		errmsg(2, "DEBUG: command is %i", command);
		/* Kommando erkannt */
		switch (command) {
		case CMD_DIR:
			if (write(fd, "START\n", 6) < 0)
				errexit("echo write: %s\n", strerror(errno));

			ret = LocalDir(outbuf, query);

			if (write(fd, outbuf, strlen(outbuf)) < 0)
				errexit("echo write: %s", strerror(errno));
			/* Abschicken */

			if (write(fd, "END\n", 4) < 0)
				errexit("echo write: %s", strerror(errno));
			outbuf[0] = 0;
			sprintf(outbuf, "RET: %d\n", ret);
			if (write(fd, outbuf, strlen(outbuf)) < 0)
				errexit("echo write: %s", strerror(errno));

			reget = 1;
			break;	/* Das war DIR */




		case CMD_GET:	/* jetzt wird's spannend */

			errmsg(2, "DEBUG: query for >%s<", inbuf + 4);

			if (write(fd, "START\n", 6) < 0)
				errexit("echo write: %s", strerror(errno));
			/* Und jetzt wird's ganz spannend */
			outbuf[0] = 0;

			if (ExtractQuery(inbuf, query)) {
				errmsg(1, "Anfrage fehlerhaft");
				ret = RET_SYN_ERR;
			} else {
				domain = XtractDomain(query);
				if (domain == NULL)
					domain = mydomain;	/* Keine Domain -> Local
								 * query */
				errmsg(4, "Domain = >%s<", domain);
				if (strcasecmp(domain, mydomain)) {	/* nicht-lokal? */
					hopcount = 0;
					do {
						outbuf[0] = 0; /* Sauber machen ! */
						ret = RemoteQuery(outbuf, query);	/* Anfrage an Remote Server */
						hopcount++;
						errmsg(4,"Hopcount = %d, outbuf = --->%s<---",hopcount,outbuf);
					} while ((ret!=0) && (hopcount<MAXCOUNT));
					if (ret==1) {
						errmsg(3,"hopcount cancelled Query %s",query);
						ret=-7;
					}
					
				} else {
					hopcount = 0;	/* local */
					ret = LocalQuery(outbuf, query);	/* Lokale Anfrage */
				}
				if (write(fd, outbuf, strlen(outbuf)) < 0)
					errexit("echo write: %s", strerror(errno));
			}
			if (write(fd, "END\n", 4) < 0)
				errexit("echo write: %s", strerror(errno));
			outbuf[0] = 0;
			sprintf(outbuf, "RET: %d HOPS: %d\n", ret, hopcount);
			if (write(fd, outbuf, strlen(outbuf)) < 0)
				errexit("echo write: %s", strerror(errno));
			reget = 1;
			break;	/* CMD_GET */

		case CMD_SGET:	/* jetzt wird's noch spannender */

			errmsg(2, "DEBUG: server query for >%s<", inbuf + 4);

			if (write(fd, "START\n", 6) < 0)
				errexit("echo write: %s", strerror(errno));
			outbuf[0] = 0;

			if (ExtractQuery(inbuf, query)) {
				errmsg(1, "Anfrage fehlerhaft");
				ret = RET_SYN_ERR;
			} else {
				domain = XtractDomain(query);
				if (domain == NULL) {
					ret = RET_SYN_ERR;
					errmsg(3, "Domain leer, Fehler");
				} else {
					errmsg(4, "Domain = >%s<", domain);
					if (strcasecmp(domain, mydomain)) {	/* nicht-lokal? */
						errmsg(4, "Nichtlokal");
						if (!GetFullRoute(domain, &sdomain, &sserver, &sflag)) {
							sprintf(outbuf, "%s %s %d\n", sdomain, sserver, sflag);
							sprintf(outbuf + strlen(outbuf), "%s %s 0\n", mydomain, myadress);
							errmsg(2, "sending %s %s %d\n", sdomain, sserver, sflag);
							ret = RET_ROUTE;
						} else {
							ret = RET_NO_DOMAIN;
						}

					} else {
						ret = LocalQuery(outbuf, query);	/* Lokale Anfrage */
					}
					if (write(fd, outbuf, strlen(outbuf)) < 0)
						errexit("echo write: %s", strerror(errno));
				}
			}
			if (write(fd, "END\n", 4) < 0)
				errexit("echo write: %s", strerror(errno));
			outbuf[0] = 0;
			sprintf(outbuf, "RET: %d\n", ret);
			if (write(fd, outbuf, strlen(outbuf)) < 0)
				errexit("echo write: %s", strerror(errno));
			reget = 0;
			break;	/* CMD_SGET */



		case CMD_QUIT:
			break;



		case CMD_HELP:
			reget = 1;	/* Help erlaubt neue Kommandoeingabe */
		case CMD_ERROR:
		default:	/* Abbruch der Verbindung, reget=0 */
			outbuf[0] = 0;
			strcat(outbuf, "PKEYD HELP:\n");
			strcat(outbuf, " GET <(username)[@domain]> transfers the public key of (username)\n");
			strcat(outbuf, " DIR [pattern] lists all known usernames (matching [pattern]) \n");
			strcat(outbuf, " HELP shows help\n");
			strcat(outbuf, " QUIT exits\n");
			strcat(outbuf, " (c) 1995/96 H. Kenn\n");
			if (write(fd, outbuf, strlen(outbuf)) < 0)
				errexit("echo write: %s\n", strerror(errno));


		}
	} while (reget);
	errmsg(1, "Connection closed");
	return 0;
}

/**********************************************************************/
/* Cleanup Functions						      */
/**********************************************************************/
#if defined(__svr4__)||defined(__linux__)

int 
reaper(int dummy)
{
	while (wait3(NULL, WNOHANG, (struct rusage *) 0) >= 0)
		 /* empty */ ;
#else
int 
reaper()
{
	union wait      status;

	while (wait3(&status, WNOHANG, (struct rusage *) 0) >= 0)
		 /* empty */ ;
#endif

}

#if defined(__svr4__)||defined(__linux__)

int 
cleanup(int dummy)
#else

int 
cleanup()
#endif
{
	errmsg(0, "Going down on signal \n");
	server_exit = 0;
	shutdown(msock, 2);	/* WICHTIG ! Ohne Schutdown wird der
				 * Master-Socket */
	/* nicht freigegeben. Und pkeyd wartet 2 Minuten */
	/* auf das Timeout beim naechsten Start */
	close(msock);
	FreeRoute();
	fclose(tcplogfile);
#if defined(__svr4__)
	if (lockf(lfile, (F_ULOCK), 0))
#endif
		close(lfile);
	unlink(LOCKFILE);

}
