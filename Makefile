EXEC =		sh
OBJS = 		sh.o
CFLAGS =	$(AUXCFLAGS) -g -Wall 
CC =		/usr/bin/gcc

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) -o $@ $(CFLAGS) $(OBJS) 

clean::
	rm -f *.o $(OBJS) $(EXEC) *~ cscope.out mon.out gmon.out

