# $Id: Makefile,v 1.7 2013/09/13 14:56:38 akadams Exp $

VERSION = 1.1.0

PREFIX = /usr/local
BIN_DIR = /bin
PKG_CONFIG_PATH=/usr/local/lib/pkgconfig

CXX = g++

CXXFLAGS = -g -O3 -Wall -pedantic -Wno-variadic-macros -D_THREAD_SAFE -DUSE_LOGGER
#CXXFLAGS = -g -O3 -Wall -pedantic -Wno-variadic-macros -D_THREAD_SAFE

INCLUDES = 
LDFLAGS = 
LIBS = 

CXXOPTIM =
CXXPATH =

TAR_PATH = tar
TAR_SRC_DIR = ip-utils-${VERSION}
TAR_SRC_NAME = ip-utils-${VERSION}.tar
GZIP_PATH = gzip

OBJS = ErrorHandler.o Descriptor.o File.o Logger.o IPComm.o TCPConn.o SSLConn.o URL.o MIMEFraming.o HTTPFraming.o MsgHdr.o TCPSession.o SSLContext.o SSLSession.o

all: libip-utils.a

libip-utils.a: ${OBJS} RFC822MsgHdr.h BasicFraming.h MsgInfo.h
	rm -f $@ ; ar -q $@ ${OBJS} ; ranlib $@

%.o: %.cc
	${CXX} -c ${CXXFLAGS} ${INCLUDES} ${CXXOPTIM} ${CXXPATH} $?

# Build c files as if they were c++ files.
%.o: %.c
	${CXX} -c ${CXXFLAGS} ${INCLUDES} ${CXXOPTIM} ${CXXPATH} $?

# Argh, who uses a "C" extension for c++?
%.o: %.C
	${CXX} -c ${CXXFLAGS} ${INCLUDES} ${CXXOPTIM} ${CXXPATH} $?

# And apparently, some people still use cpp extensions ...
%.o: %.cpp
	${CXX} -c ${CXXFLAGS} ${INCLUDES} ${CXXOPTIM} ${CXXPATH} $?

tar_src:
	cd /tmp; /bin/rm -rf ${TAR_SRC_DIR}; \
	cvs -d repository.psc.edu:/afs/psc.edu/projects/midas/cvsgaia \
		co -d ${TAR_SRC_DIR} GAIA/src/ip-utils; \
	/bin/rm -f ${TAR_SRC_NAME}.gz; \
	${TAR_PATH} -vcpf ${TAR_SRC_NAME} ${TAR_SRC_DIR}; \
	${GZIP_PATH} -f ${TAR_SRC_NAME}
	# No longer in /tmp.
	/bin/rm -f ${TAR_SRC_NAME}.gz
	cp /tmp/${TAR_SRC_NAME}.gz .

clean:	
	rm -rf libip-utils.a *.o
