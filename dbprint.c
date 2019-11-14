#include <autoconf.h>
#include <stdlib.h>
#include <fcntl.h>
#include <gdbm.h>
#include <stdio.h>
#include <string.h>
#define MAXKEY 100
#define MAXCON 1000
main()
{
GDBM_FILE mydb;
datum key,content,getback;
char keybuf[MAXKEY];
char contbuf[MAXCON];
int ret;
FILE *fh;
int tocopy;
mydb=gdbm_open("./keydb",0,GDBM_READER,0600,0); 
	/* Oeffne Datenbank, Lesen  */

if (mydb==NULL) exit(-1);
			
key=gdbm_firstkey(mydb);
do {
getback=gdbm_fetch(mydb,key);
printf("<%s>\n%s\n",key.dptr,getback.dptr);
free(getback.dptr);
free(key.dptr);
key=gdbm_nextkey(mydb,key);
} while (key.dptr!=NULL);
}







