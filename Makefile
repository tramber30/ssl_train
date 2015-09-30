CC = gcc
CFLAGS = -Wall
SRC_FILES = http.c

http_client: $(SRC_FILES)
	$(CC) -o $@ $(CFLAGS) $^ 

clean: 
	rm -f http_client
