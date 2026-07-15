CFLAGS=-g

cedarname: cedarname.o pfs.o vermap.o
	$(CC) $(CFLAGS) -o cedarname cedarname.o pfs.o vermap.o


