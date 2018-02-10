CC=/usr/bin/gcc
CC_OPTS=-g3
CC_LIBS=
CC_DEFINES=
CC_INCLUDES=
CC_ARGS=${CC_OPTS} ${CC_LIBS} ${CC_DEFINES} ${CC_INCLUDES}

# clean is not a file
.PHONY=clean

all: client server

client: client.c
	@${CC} ${CC_ARGS} -o http_client client.c

server: server.c
	@${CC} ${CC_ARGS} -o http_server server.c

clean:
	@rm -f http_server http_client *.o