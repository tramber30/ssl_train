#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define HTTP_PORT 80

int process_http_request (int sock)
{
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



