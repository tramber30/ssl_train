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

#define DEBUG_LOG 1

#ifdef DEBUG_LOG
#define LOG(str, ...) fprintf(stdout, "[%s:%d] " str "\n", __func__, __LINE__, ##__VA_ARGS__)
#else
#define LOG(str, ...)
#endif
/*
 * Accept a well formed URL and return pointers on the host part
 * and the path part. It modify the uri and return 0 on success, 
 * -1 if the URL is malformed
 */
int parse_url(char *uri, char **host, char **path)
{
	char *pos;

	LOG("start, url: (%s)", uri);

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

	LOG("out");
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

	LOG("start");

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
int http_get(int connection, const char *path, const char *host,
		const char *proxy_host,
		const char *proxy_user,
		const char *proxy_password)
{
	int result;
	static char get_command[MAX_GET_COMMAND];

	LOG("start");
	
	if (proxy_host)
		sprintf(get_command, "GET http://%s/%s HTTP/1.1\r\n", host, path);
	else
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

int proxy_parser(char *proxy_string, int *proxy_port, char *proxy_host, char *proxy_user, char *proxy_password)
{
	char *login_sep, *colon_sep, *trailer_sep;

	LOG("start");

	/* Forgive if user doesn't prefix proxy string with "http://" */
	if (!strncmp("http://", proxy_string, 7))
		proxy_string += 7;
	
	login_sep = strchr(proxy_string, '@');
	if (login_sep) {
		colon_sep = strchr(proxy_string, ':');
		if (!colon_sep || (colon_sep > login_sep)){
			//TODO: handle no password case?
			fprintf(stderr, "Expected password in (%s)\n", proxy_string);
			return -1;
		}
		*colon_sep = '\0';
		strcpy(proxy_host, proxy_string);
		*login_sep = '\0';
		strcpy(proxy_user, proxy_string);
		strcpy(proxy_password, colon_sep + 1);
		strcpy(proxy_string, login_sep + 1);
	}
	
	/* Remove ending '/' if necessary */
	trailer_sep = strchr(proxy_string, '/');
	if (trailer_sep)
		*trailer_sep = '\0';

	colon_sep = strchr(proxy_string, ':');
	if (colon_sep) {
		/* Non standard proxy port */
		*colon_sep = '\0';
		strcpy(proxy_host, proxy_string);
		*proxy_port = atoi(colon_sep + 1);
		/* Authorized range of port */
		if (*proxy_port < 1 || *proxy_port > 65535) {
			fprintf(stderr, "Wrong port number (%d)", *proxy_port);
			return -2;
		}
	} else {
		*proxy_port = HTTP_PORT;
		strcpy(proxy_host, proxy_string);
	}
	return 0;
}

/*
 * Print http_client command line usage
 */
void usage(char *cmd_name)
{
	fprintf(stderr, "Usage: %s: [-p http://[username:password@]proxy-host:proxy-port] \
			<URL>\n", cmd_name);
}

/*
 * Simple command line HTTP client
 */
int main(int argc, char *argv[])
{
	int client_connection, result;
	int proxy_port, ind, it;
	char proxy_host[MAX_BUFFER_SIZE] = {};
	char proxy_user[MAX_BUFFER_SIZE] = {};
	char proxy_password[MAX_BUFFER_SIZE] = {};
	char *host, *path;
	struct hostent *host_name;
	struct sockaddr_in host_address;
	
	extern char *optarg;
	extern int optind, optopt;

	LOG("start");

	if (argc < 2) {
		usage(argv[0]);
		return -1;
	}

	while ((ind = getopt(argc, argv, "p:")) != -1) {
		switch(ind) {
		case 'p':
			proxy_parser(optarg, &proxy_port, proxy_host, proxy_user, proxy_password);
			LOG("Use proxy settings\n");
			LOG("proxy_port: %d, proxy_host: (%s), proxy_user: (%s), proxy_password: (%s)", proxy_port, proxy_host, proxy_user, proxy_password);
			break;
		case ':':
			LOG("-%c without filename\n", optopt);
			break;
		case '?':
			LOG("Unknown arg -%c\n", optopt);
			usage(argv[0]);
			break;
		default:
			LOG("default?? (%c)", ind);
			break;
		}
		for (it = optind; it < argc; it++)
			LOG("Non-option arguments %s", argv[it]);
	}

	result = parse_url(argv[optind], &host, &path);
	if (result < 0) {
		fprintf(stderr, "Error: malformed URL (%s)\n", argv[1]);
		return -1;
	}

	printf("Connection to host (%s)\n", host);

	/* Open socket connection on http_port with the destination host */
	client_connection = socket(PF_INET, SOCK_STREAM, 0);
	if (!client_connection) {
		perror("Unable to create local socket");
		return -2;
	}

	LOG("after socket creation");

	/* Resolve host name */
	host_name = gethostbyname(host);
	if (!host_name) {
		perror("Error in name resolution");
		return -3;
	}

	LOG("resolv name");

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

	http_get(client_connection, path, host, proxy_host, proxy_user, proxy_password);

	LOG("document retrieved");
	display_result(client_connection);

	printf("Shutting down\n");

	result = close(client_connection);
	if (result < 0) {
		perror("Error closing client socket");
		return -5;
	}

	return 0;
}




























