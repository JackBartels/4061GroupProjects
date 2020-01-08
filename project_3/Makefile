CC = gcc
CFLAGS = -D_REENTRANT
LDFLAGS = -lpthread -pthread

web_server: server.c
	${CC} -g -o web_server -std=gnu11 server.c util.o ${LDFLAGS}

clean:
	rm web_server
	rm *.jpg
	rm *.jpg.*
	rm *.gif
	rm *.gif.*
	rm *.txt
	rm *.txt.*
	rm *.html
	rm *.html.*
