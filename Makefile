CC = gcc
CFLAGS = -Wall -g
SRC_CLIENT_FILES = http_client.c
SRC_SERVER_FILES = http_server.c

all: http_client http_server

http_client: $(SRC_CLIENT_FILES)
	$(CC) -o $@ $(CFLAGS) $^ 

http_server: $(SRC_SERVER_FILES)
	$(CC) -o $@ $(CFLAGS) $^ 

clean: 
	rm -f http_client http_server
