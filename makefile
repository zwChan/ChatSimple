CC=gcc
CFLAG=-g3 -Wall -ansi -pedantic -O0 

OBJ = 	chatSimple.o \
		user.o \
		msg.o

OBJ_CLIENT = client.o

.PHONY: all
all: chatSimple client

chatSimple: ${OBJ}
	$(CC) $(CFLAG) $^ -o $@   -levent

client: ${OBJ_CLIENT}
	$(CC) $(CFLAG) $^ -o $@   -levent

%.o : %.c
	$(CC) $(CFLAG) -c $<

.PHONY: clean
clean:
	rm -f chatSimple
	rm -f client
	rm -f *.o
	rm -f *~
	rm -f core

