#	$FabBSD$
#	$OpenBSD: Makefile,v 1.4 2008/03/08 15:36:12 espie Exp $

# NOTE: The mkhybrid distrib also makes an "mkisofs" which is the same binary
# linked. When we know mkhybrid makes distrib CDs ok we should modify this
# to make mkisofs as a link to this, rather than the (earlier) version in
# the tree.

PROG=	mkhybrid
MAN=	mkhybrid.8
BINDIR=	/usr/sbin
.PATH:	${.CURDIR}/../src ${.CURDIR}/../src/libhfs_iso ${.CURDIR}/../src/libfile

SRCS=	data.c block.c low.c lfile.c btree.c node.c record.c lvolume.c \
	hfs.c file.c apprentice.c softmagic.c mkisofs.c tree.c write.c \
	hash.c rock.c multi.c joliet.c match.c name.c eltorito.c \
	getopt.c getopt1.c apple.c volume.c desktop.c mac_label.c		

CFLAGS+=-DSYSTEM_ID_DEFAULT=\"FabBSD\" -DAPPLE_HYB -DVANILLA_AUTOCONF \
	-I${.CURDIR}/../src -I${.CURDIR}/../src/include \
	-I${.CURDIR}/../src/libhfs_iso \
	-I${.CURDIR}/../src/libfile

# XXX: alpha gcc bug
write.o: write.c
	${CC} ${CFLAGS} -O0 -c ${.CURDIR}/../src/write.c

.include <bsd.prog.mk>
