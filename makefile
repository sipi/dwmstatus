# Compiler : gcc, g++, ...
CC=gcc
# Extension
EXT=.c
# Library flags : -lgl, -lpthread, ...
LDFLAGS= -lX11 -lasound 
# Debug flags
DEBUG_FLAGS= -DDEBUG=1 -W -Wall -ansi -pedantic -g
# Other flags
FLAGS= -std=c99


#INCLUDE_DIR=include
SOURCE_DIR=src

PROG_NAME=dwmstatus
SRCS=\
dwmstatus.c

################################################################
#
################################################################

ifeq (${DEBUG},1)
	FLAGS+=${DEBUG_FLAGS}
endif
#FLAGS+= -I ${INCLUDE_DIR}
OBJS=${SRCS:${EXT}=.o}


${PROG_NAME}: ${OBJS}
	${CC} -o $@ ${OBJS} ${LDFLAGS} ${FLAGS}

%.o: ${SOURCE_DIR}/%${EXT}
	${CC} -o $@ -c $< ${FLAGS}

mrproper:
	rm -rf *.o *~ \#*\# ${PROG_NAME}

clean:
	rm -rf *.o
