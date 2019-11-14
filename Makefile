# Generated automatically from Makefile.in by configure.

all:	dbprint dbconv pkeyd

prefix=.
CFLAGS=-g -O
CPPFLAGS=-I. -I../gdbm
LDFLAGS=-L. -L../gdbm
LIBS=-lnsl -lgdbm  

defs.h:
	echo "#define ETCDIR \"${prefix}/etc\"" >defs.h
	echo "#define VARDIR \"${prefix}/var\"" >> defs.h

TCPlib.o:	TCPlib.c
	gcc -g -O -I. -I../gdbm -DHAVE_CONFIG_H -I. -c TCPlib.c

pkeyd:	pkeyd.o TCPlib.o 
	gcc -L. -L../gdbm -o pkeyd pkeyd.o TCPlib.o -lnsl -lgdbm  

pkeyd.o:	pkeyd.c defs.h
	gcc -g -O -I. -I../gdbm -DHAVE_CONFIG_H -I. -c pkeyd.c

dbprint:	dbprint.o
	gcc -L. -L../gdbm -o dbprint dbprint.o -lnsl -lgdbm  

dbprint.o:	dbprint.c
	gcc -g -O -I. -I../gdbm -DHAVE_CONFIG_H -I. -c dbprint.c 

dbconv:	dbconv.o
	gcc -L. -L../gdbm -o dbconv dbconv.o -lnsl -lgdbm  

dbconv.o:	dbconv.c
	gcc -g -O -I. -I../gdbm -DHAVE_CONFIG_H -I. -c dbconv.c

clean:
	rm *.o dbconv dbprint pkeyd defs.h

install:
	@INSTALL@ dbprint dbconf pkeyd ${exec_prefix}/bin
	@INSTALL@ pkeyd.route pkeyd.conf ${prefix}/etc

