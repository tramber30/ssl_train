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

typedef struct {
	int	port;
	char	host[MAX_BUFFER_SIZE];
	char	user[MAX_BUFFER_SIZE];
	char	password[MAX_BUFFER_SIZE];
} proxy_t;
	
static char b64 [] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/*
 * Base64 encoding
 */
void base64_encode(const unsigned char *input, int len, unsigned char *output)
{
	do
	{
		*output++ = b64[(input[0] & 0xFC) >> 2];

		if (len == 1) {
			*output++ = b64[((input[0] & 0x03) << 4)];
			*output++ = '=';
			*output++ = '=';
			break;
		}

		*output++ = b64[((input[0] & 0x03) << 4) | ((input[1] & 0x0F) >> 4)];
		if (len == 2) {
			*output++ = b64[((input[1] & 0x0F) << 2)];
			*output++ = '=';
			break;
		}

		*output++ = b64[((input[1] & 0x0F) << 2) | ((input[2] & 0xC0) >> 6)];
		input += 3;
	} while (len -= 3);

	*output ='\0';
}

static int unbase64[] =
{
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63, 52,
	53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, 0, -1, -1, -1,
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
	16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, -1,
	26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
	42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1, -1
};

int base64_decode(const unsigned char *input, int len, unsigned char *output)
{
	size_t out_len = 0;
	int i;

	if (len & 0x03) {
		fprintf(stderr, "Error string is mal formed, not even a multiple of 4, len = %d\n", len);		return -1;
	}

	do {
		for (i = 0; i <= 3; i++) {
			// Check for illegal base64 characters
			if (input[i] > 128 || unbase64[input[i]] == -1) {
				fprintf(stderr, "Invalid base64 character: %c\n", input[i]);
				return -1;
			}
		}
		*output++ = unbase64[input[0]] << 2 | (unbase64[input[1]] & 0x30) >> 4;
		out_len++;
		
		if (input[2] != '=') {
			*output++ = (unbase64[input[1]] & 0x0F) << 4 | (unbase64[input[2]] & 0x3C) >> 2;
			out_len++;
		}

		if (input[3] != '=') {
			*output++ = (unbase64[input[2]] & 0x03) << 6 | unbase64[input[3]];
			out_len++;
		}

		input += 4;
	} while (len -= 4);

	return out_len;
}

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
		proxy_t proxy)
{
	int result = 0;
	static char get_command[MAX_GET_COMMAND];

	LOG("start");
	
	if (proxy.port)
		sprintf(get_command, "GET http://%s/%s HTTP/1.1\r\n", host, path);
	else
		sprintf(get_command, "GET /%s HTTP/1.1\r\n", path);

	result = send(connection, get_command, strlen(get_command), 0);
	if (result < 0)
		return result;

	sprintf(get_command, "Host: %s\r\n", host);
	result = send(connection, get_command, strlen(get_command), 0);
	if (result < 0)
		return result;

	if (proxy.user) {
		size_t cred_len = strlen(proxy.user) + strlen(proxy.password) + 1;
		char *cred = malloc(cred_len);
		char *encoded_cred = malloc(((cred_len*4)/3)+1);

		sprintf(cred, "%s:%s", proxy.user, proxy.password);
		base64_encode((unsigned char *)cred, cred_len, (unsigned char *)encoded_cred);
		sprintf(get_command, "Proxy-Authorization: BASIC %s/r/n", encoded_cred);

		result = send(connection, get_command, strlen(get_command), 0);
		free(encoded_cred);
		free(cred);

		if (result < 0)
			return result;
	}		
							

	sprintf(get_command, "Connection: close\r\n\r\n");
	result = send(connection, get_command, strlen(get_command), 0);
	return result;
}

int proxy_parser(char *proxy_string, proxy_t *proxy)
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
//		strcpy(proxy.host, proxy_string);
		*login_sep = '\0';
		strcpy(proxy->user, proxy_string);
		strcpy(proxy->password, colon_sep + 1);
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
		strcpy(proxy->host, proxy_string);
		proxy->port = atoi(colon_sep + 1);
		/* Authorized range of port */
		if (proxy->port < 1 || proxy->port > 65535) {
			fprintf(stderr, "Wrong port number (%d)", proxy->port);
			return -2;
		}
	} else {
		proxy->port = HTTP_PORT;
		strcpy(proxy->host, proxy_string);
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
	int ind, it;
	char *host, *path;
	struct hostent *host_name;
	struct sockaddr_in host_address;
	proxy_t proxy;
	extern char *optarg;
	extern int optind, optopt;

	LOG("start");

	if (argc < 2) {
		usage(argv[0]);
		return -1;
	}

	memset(&proxy, 0x00, sizeof(proxy_t));

	while ((ind = getopt(argc, argv, "p:")) != -1) {
		switch(ind) {
		case 'p':
			proxy_parser(optarg, &proxy);
			LOG("Use proxy settings\n");
			LOG("proxy_port: %d, proxy_host: (%s), proxy_user: (%s), proxy_password: (%s)", proxy.port, proxy.host, proxy.user, proxy.password);
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

	/* Open socket connection on http_port with the destination host */
	client_connection = socket(PF_INET, SOCK_STREAM, 0);
	if (!client_connection) {
		perror("Unable to create local socket");
		return -2;
	}

	if (proxy.port) {
		/* Resolve proxy host name */
		LOG("Connecting to proxy: (%s)", proxy.host);
		host_name = gethostbyname(proxy.host);
	} else {
		/* Resolve host name */
		LOG("Connecting to host: (%s)", host);
		host_name = gethostbyname(host);
	}

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

	http_get(client_connection, path, host, proxy);

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

