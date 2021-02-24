CC=gcc
CFLAGS=-g
TARGET=testapp.exe
EXTERNAL_LIBS=-lpthread

OBJS=gluethread/glthread.o \
	 ConnMgmt/conn_mgmt.o

${TARGET}:testapp.o ${OBJS}
	${CC} ${CFLAGS} testapp.o ${OBJS} -o ${TARGET} ${EXTERNAL_LIBS}

testapp.o:testapp.c
	${CC} ${CFLAGS} -c -I . testapp.c -o testapp.o
gluethread/glthread.o:gluethread/glthread.c
	${CC} ${CFLAGS} -c -I gluethread gluethread/glthread.c -o gluethread/glthread.o
ConnMgmt/conn_mgmt.o:ConnMgmt/conn_mgmt.c
	${CC} ${CFLAGS} -c -I . -I gluethread -I ConnMgmt/ ConnMgmt/conn_mgmt.c -o ConnMgmt/conn_mgmt.o
clean:
	rm -f *.o
	rm -f ${OBJS}
	rm -f *exe
