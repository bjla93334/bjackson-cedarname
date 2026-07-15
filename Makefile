# gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0

CFLAGS = -g
OBJS = cedarname.o pfs.o vermap.o

cedarname: $(OBJS)

.PHONY: clean
clean:
	$(RM) cedarname $(OBJS)

## quick test :
# setenv XeroxCedar ~/Desktop/CSL-93-16/Cedar
# ./cedarname /r/Rope.mesa

# eof
