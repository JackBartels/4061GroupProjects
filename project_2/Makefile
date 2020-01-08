all: client server

client: client.c comm.h comm.o util.o
	gcc client.c -std=gnu11 -g -o $@ comm.o util.o


server: server.c comm.h comm.o util.o
	gcc server.c -std=gnu11 -g -o $@ comm.o util.o

util.o: util.c util.h
	gcc -c util.c

comm.o: comm.c comm.h
	gcc -c comm.c

clean:
	rm -f *.o client server
