PREFIX?=/usr/local
BINDIR=${PREFIX}/bin
MANDIR=${PREFIX}/man/cat

PROG=	icmpnam
SRCS=	log.c icmpnam.c
MAN=

CFLAGS+= -g -ggdb -O0 -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
LDADD+= -levent -lutil
DPADD+= ${LIBEVENT} ${LIBUTIL}

.include <bsd.prog.mk>
#.include <bsd.man.mk>
