#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define HTTP_PORT 80
#define MAX_GET_COMMAND 255
#define MAX_BUFFER_SIZE 255

/*
 * Accept a well formed URL and return pointers on the host part
 * and the path part. It modify the uri and return 0 on success, 
 * -1 if the URL is malformed
 */

int parse_url(char *uri, char **host, char **path)
{
	char *pos;

	pos = strstr(uri, "//");
	if (!pos)
		return -1;

	*host = pos + 2;
	pos = strstr(*host, "/");
	if (!pos) {
		*path = NULL;
	} else {
		*pos = '\0';
		*path = pos + 1;
	}

	return 0;
}

/*
 * Receive data on a connection and display them
 * on standard output
 */
int display_result(int connection)
{
	int received = 0;
	static char recv_buf[MAX_BUFFER_SIZE + 1];

	received = recv(connection, recv_buf, MAX_BUFFER_SIZE, 0);
	while (received > 0) {
		recv_buf[received] = '\0';
		printf("%s", recv_buf);
		received = recv(connection, recv_buf, MAX_BUFFER_SIZE, 0);
	}
	printf("\n");

	return 0;
}

/*
 * Format and send an http get command. return 0 on success, else -1
 * with errno set. Caller must then retieve the response
 */
int http_get(int connection, const char *path, const char *host)
{
	int result;
	static char get_command[MAX_GET_COMMAND];

	sprintf(get_command, "GET /%s HTTP/1.1\r\n", path);
	result = send(connection, get_command, strlen(get_command), 0);
	if (result < 0)
		return -1;

	sprintf(get_command, "Host: %s\r\n", host);
	result = send(connection, get_command, strlen(get_command), 0);
	if (result < 0)
		return -2;

	sprintf(get_command, "Connection: close\r\n\r\n");
	result = send(connection, get_command, strlen(get_command), 0);
	if (result < 0)
		return -3;

	return 0;
}

/*
 * Simple command line HTTP client
 */
int main(int argc, char *argv[])
{
	int client_connection, result;
	char *host, *path;
	struct hostent *host_name;
	struct sockaddr_in host_address;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s: <URL>\n", argv[0]);
		return -1;
	}
	result = parse_url(argv[1], &host, &path);
	if (result < 0) {
		fprintf(stderr, "Error: malformed URL (%s)\n", argv[1]);
		return -1;
	}

	printf("Connection to host (%s)\n", host);

	/* Step 1: open socket connection on http_port with the destination host */
	client_connection = socket(PF_INET, SOCK_STREAM, 0);
	if (!client_connection) {
		perror("Unable to create local socket");
		return -2;
	}

	host_name = gethostbyname(host);
	if (!host_name) {
		perror("Error in name resolution");
		return -3;
	}

	host_address.sin_family = AF_INET;
	host_address.sin_port = htons(HTTP_PORT);
	memcpy(&host_address.sin_addr, host_name->h_addr_list[0],
			sizeof(struct in_addr));
	
	result = connect(client_connection, (struct sockaddr *)&host_address, 
			sizeof(host_address));
	if (result < 0) {
		perror("Unable to connect to host");
		return -4;
	}

	printf("Retrieve document: (%s)\n", path);

	http_get(client_connection, path, host);
	display_result(client_connection);

	printf("Shutting down\n");

	result = close(client_connection);
	if (result < 0) {
		perror("Error closing client socket");
		return -5;
	}

	return 0;
}




























