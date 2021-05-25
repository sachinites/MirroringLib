rm ConnMgmt/*.o
rm ConnMgmt/*exe
gcc -g -c ConnMgmt/conn_mgmt.c -o ConnMgmt/conn_mgmt.o
gcc -g ConnMgmt/conn_mgmt.o libtimer/WheelTimer.o  libtimer/timerlib.o libtimer/gluethread/glthread.o -o ConnMgmt/conn_mgmt.exe -lpthread -lrt
