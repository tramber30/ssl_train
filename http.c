#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>


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
	pos = strstr(*host, '/');
	if (!pos)
		*path = NULL;
	else {
		*pos = '\0';
		*path = pos + 1;
	}
	return 0;
}
