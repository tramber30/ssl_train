#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define HTTP_PORT 80
#define DEFAULT_LINE_LEN 255

static void build_error_response(int connection, int cause)
{
	char buf[DEFAULT_LINE_LEN];
	char *response_string = "HTTP/1.1 %d Error Occurred\r\n\r\n";
	sprintf(buf, response_string, cause);

	 // Technically, this should account for short writes.
	if (send( connection, buf, strlen( buf ), 0 ) < strlen( buf ))
		perror("build_error_response failed");
	else
		printf("%s: (%s)\n", __FUNCTION__, response_string);

	return;
}

static void build_success_response(int connection)
{
	char buf[DEFAULT_LINE_LEN];
	char *response_string =    "HTTP/1.1 200 Success\r\nConnection: Close\r\nContent-Type:text/html\r\n\r\n<html><head><title>Test Page</title></head><body>Nothing here</body></html>\r\n";
	sprintf(buf, response_string);

	if (send(connection, buf, strlen(buf), 0) < strlen(buf))
		perror("build_success_respond failed");
	else
		printf("%s: (%s)\n", __FUNCTION__, response_string);

	return;
}

char *read_line(int connection)
{
	static int line_len = DEFAULT_LINE_LEN;
	static char *line = NULL;
	int size, pos = 0;
	char c;

	if (!line)
		line = malloc(line_len);

	while ((size = recv(connection, &c, 1, 0)) > 0)
	{
		if ((c == '\n') && (line[pos-1] == '\r'))
		{
			line[pos -1] = '\0';
			break;
		}
		line[pos++] = c;

		if (pos > line_len)
		{
			line_len *= 2;
			line = realloc(line, line_len);
		}
	}

	return line;
}

int process_http_request (int connection)
{
	int res = 0;
	char *req_line;

	req_line = read_line(connection);
	if (strncmp(req_line, "GET", 3))
		build_error_response(connection, 501);
	else { //Skip over all header lines, don't care
		while(strcmp(read_line(connection), ""));

		build_success_response(connection);
	}

	res = shutdown(connection, 2);
	if (res)
		perror("Unable to close connection");

	return 0;
}

int main(int argc, char *argv[])
{
	int listen_sock, connect_sock;
	int options = 1;
	struct sockaddr_in local_addr;
	struct sockaddr_in client_addr;
	socklen_t client_addr_len = sizeof(client_addr);
	int res;

	listen_sock = socket(PF_INET, SOCK_STREAM, 0); 

	if (listen_sock == -1) {
		perror("Unable to create listening socket");
		return listen_sock;
	}

	res = setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &options, sizeof(options));
	if (res == -1) {
		perror("Unable to set listening socket options");
		return res;
	}
	
	local_addr.sin_family = AF_INET;
	local_addr.sin_port = htons(HTTP_PORT);
	local_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	//local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	res = bind(listen_sock, (struct sockaddr *) &local_addr, sizeof(local_addr));
	if (res == -1) {
		perror("Unable to bind to local address");
		return res;
	}

	res = listen(listen_sock, 5);
	if (res == -1) {
		perror("Unable to set socket backlog");
		return res;
	}

	while((connect_sock = accept(listen_sock, (struct sockaddr *) &client_addr, &client_addr_len)) != -1) {
		//TODO: ideally this would spawn a new thread
		process_http_request(connect_sock);
	}

	if (connect_sock == -1)
		perror("Unable to accept socket");

	return 0;
}



