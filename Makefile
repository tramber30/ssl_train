CC = gcc
CFLAGS = -Wall -g
SRC_FILES = http_client.c

http_client: $(SRC_FILES)
	$(CC) -o $@ $(CFLAGS) $^ 

clean: 
	rm -f http_client
