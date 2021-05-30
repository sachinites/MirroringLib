rm ConnMgmt/*.o
rm ConnMgmt/*exe
rm libtimer/*.o
rm CommandParser/*.o
gcc -g -c ConnMgmt/conn_mgmt.c -o ConnMgmt/conn_mgmt.o
gcc -g -c ConnMgmt/conn_mgmt_ui.c -o ConnMgmt/conn_mgmt_ui.o
cd CommandParser
make
cd ..
cd libtimer
sh compile.sh
cd ..
echo Building conn_mgmt.exe
gcc -g ConnMgmt/conn_mgmt.o ConnMgmt/conn_mgmt_ui.o libtimer/WheelTimer.o  libtimer/timerlib.o libtimer/gluethread/glthread.o -o ConnMgmt/conn_mgmt.exe -lpthread -lrt -L CommandParser -lcli
