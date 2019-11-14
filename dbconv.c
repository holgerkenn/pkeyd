#include <autoconf.h>
#include <stdlib.h>
#include <fcntl.h>
#include <gdbm.h>
#include <stdio.h>
#include <string.h>
#define MAXKEY 100
#define MAXCON 1000

int ExtractQuery(char *inbuf,char *query)
        {
        char *qstart;
        char *qend;

        qstart=strchr(inbuf,'<'); /* Sucht < */
        if (qstart) {
                qstart++;
                qend=strchr(qstart,'>'); /* Sucht > */
                if (qend) {
                        strncpy(query,qstart,qend-qstart);
                        query[qend-qstart]=0; /* Ende */
                        return(0);
		}
	}
        return(1);
	} /* ExtractQuery */



main()
{
GDBM_FILE mydb;
datum key,content,getback;
char keybuf[MAXKEY];
char backbuf[MAXKEY];
char contbuf[MAXCON];
char index[MAXKEY];
int contptr;
int ret;
FILE *fh;
int tocopy;
printf(" Konvertiert ein publickeyfile in die Datenbank \n");
mydb=gdbm_open("./keydb",0,GDBM_NEWDB,0600,0); 
	/* Oeffne Datenbank, Lesen u Schreiben, 600 als Rechte */

if (mydb==NULL) exit(-1);
fh=fopen("publickeys","r");
if (fh) {
	if (!feof(fh)) fgets(keybuf,(sizeof(keybuf)),fh);

	while(!feof(fh)){
		if (strncmp(keybuf,"AU:",3)) break; /* Kein AU: break */
		ExtractQuery(keybuf,index); /* Sucht <...> */
		printf("\nindex >%s<\n",index);
		key.dptr=index;
		key.dsize=strlen(index)+1; /* Speichert 0 mit ab */
		printf("o");
		strcpy(contbuf,keybuf);
		contptr=strlen(contbuf);
		while (!feof(fh)){
			fgets(contbuf+contptr,(sizeof(contbuf)-contptr),fh);
			printf(".");

			if ((strncmp(contbuf+contptr,"AU:",3)==0)||feof(fh)) {
				/*Kopiere contbuf nach keybuf */
				tocopy=(
					(strlen(contbuf+contptr)+1)<MAXKEY ? 
					(strlen(contbuf+contptr)+1) : 
					MAXKEY);
				memcpy(backbuf,contbuf+contptr,tocopy);
				contbuf[contptr]=0; /* Neues Stringende */
				content.dptr=contbuf;
				content.dsize=strlen(contbuf)+1;

				ret=gdbm_store(mydb,key,content,GDBM_REPLACE);
				memcpy(keybuf,backbuf,tocopy);
				printf("\b");
				break;
			}
			contptr=strlen(contbuf);
		}
	}
}
printf(" \n");
fclose(fh);
gdbm_close(mydb);

}
