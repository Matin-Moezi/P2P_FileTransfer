/******************************* P2P File Transfer      *****************************************/
/******************************* Computer Network Course Project ********************************/
/******************************* Matin Moezi #9512058 *******************************************/
/******************************* Fall 2019 ******************************************************/

/******************************* Main Program *************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAXBUFFLEN 32			/* Maximum request message size */
#define MAXDGRAMLEN 128			/* Maximum datagram packet size */
#define MAXRESLEN 256			/* Maximum response message size */

#define SERVEPORT 55555			/* Serving mode port            */

// global variables
char serve_filename[32];		/* serving filename				*/
char path[64];					/* serving file path			*/

// thread function argument type
struct client_conn_t {
	int fd;								/* socket file descriptor */
	char buffer[MAXBUFFLEN];			/* request message        */
	struct sockaddr_in addr;			/* receiver address       */
};
typedef struct client_conn_t client_conn_t;

// create udp server socket
int init_serve_sock()
{
	int sock_fd;
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SERVEPORT);
	addr.sin_addr.s_addr = INADDR_ANY;

	if((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	{
		perror("create socket");
		return -1;
	}
	if(bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		perror("bind socket");
		return -1;
	}
	return sock_fd;
}

// get socket hostname(dot-decimal) and port
int getsockaddr(int fd, char *hostname, int *port)
{
	struct sockaddr_in addr;
	int len = sizeof(addr);
	if(getsockname(fd, (struct sockaddr *)&addr, &len) == -1)
	{
		perror("get socket address");
		return -1;
	}
	if(hostname != NULL)
	{
		char host[INET_ADDRSTRLEN];
		if(inet_ntop(AF_INET, &(addr.sin_addr), host, INET_ADDRSTRLEN) == NULL)
		{
			perror("network byte address to dot-decimal address");
			return -1;
		}
		strcpy(hostname, host);
	}
	*port = ntohs(addr.sin_port);
	return 0;
}


// create udp broadcast socket
int init_broadcast_sock()
{
	int sock_fd;
	if((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	{
		perror("create socket");
		return -1;
	}

	// set broadcast option to socket
	int broadcast = 1;
	if(setsockopt(sock_fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(int)) == -1)
	{
		perror("set broadcast enable");
		return -1;
	}
	struct timeval tv;
	tv.tv_sec = 3;
	tv.tv_usec = 0;
	setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
	return sock_fd;
}

// create file request message
void create_request_msg(const char *filename, char *msg)
{
	bzero(msg, MAXBUFFLEN);
	sprintf(msg, "File Request:\n\n%s\n", filename);
}

int req_parser(char *request, char *filename)
{
	if(sscanf(request, "File Request:\n\n%s\n", filename) != 1)
	{
		fprintf(stderr, "request message format is invalid.");
		return -1;
	}
	return 0;
}
void create_response(int offset, char *data, char *response)
{
	char tmp[MAXRESLEN] = {'\0'};
	bzero(response, MAXRESLEN);
	sprintf(tmp, "File Response:\noffset[%d]\n\n", offset);
	strcat(tmp, data);
	strcpy(response, tmp);
}
int res_parser(char *response, char *data, int *offset)
{
	int index;
	if(sscanf(response, "File Response:\noffset[%d]\n\n%n", offset, &index) != 1)
	{
		fprintf(stderr, "request message format is invalid.");
		return -1;
	}
	strcpy(data, response + index + 1);
	return 0;
}
void *connection_handler(void *arg)
{
	client_conn_t *client_conn = (client_conn_t *)arg;
	int client_port = ntohs(client_conn->addr.sin_port);
	char client_host[INET_ADDRSTRLEN], filename[32];

	// convert network byte address to dot-decimal hostname
	if(inet_ntop(AF_INET, &(client_conn->addr.sin_addr), client_host, INET_ADDRSTRLEN) == NULL)
	{
		perror("connection handler");
		pthread_exit(NULL);
	}

	if(req_parser(client_conn->buffer, filename) == -1)
		pthread_exit(NULL);
	printf("client %s %d requested \"%s\" file.\n", client_host, client_port, filename);

	if(strcmp(serve_filename, filename) == 0)
	{
		printf("Sending file to client...\n");
		FILE *file = fopen(path, "r");
		char c, data[MAXDGRAMLEN], response[MAXRESLEN];
		if(file == NULL)
		{
			perror("open file");
			pthread_exit(NULL);
		}

		int k = 0, i = 1;
		bzero(data, MAXDGRAMLEN);
		data[0] = k + '0';
		while((c = fgetc(file)) != EOF)
		{
			if(i == MAXDGRAMLEN)
			{
				create_response(k++, data, response);
				if(sendto(client_conn->fd, response, MAXRESLEN, 0, (struct sockaddr *)&client_conn->addr, sizeof(client_conn->addr)) == -1)
				{
					perror("sending file");
					pthread_exit(NULL);
				}
				i = 1;
				bzero(data, MAXDGRAMLEN);
				data[0] = k + '0';
			}
			data[i++] = c;
		}
		create_response(k++, data, response);
		if(sendto(client_conn->fd, response, MAXRESLEN, 0, (struct sockaddr *)&client_conn->addr, sizeof(client_conn->addr)) == -1)
		{
			perror("sending file");
			pthread_exit(NULL);
		}
		printf("Sendig file successful.\n");
		pthread_exit(NULL);
	}
	else
	{
		printf("The file is not exist.\n");
		shutdown(client_conn->fd, 2);
		pthread_exit(NULL);
	}
}
int main(int argc, char *argv[])
{
	// Receive mode
	if(argc == 3 && strcmp("-receive", argv[1]) == 0)
	{
		strcpy(serve_filename, argv[2]);

		int sockfd = init_broadcast_sock();
		if(sockfd == -1)
			return EXIT_FAILURE;

		// initialization destination address
		struct sockaddr_in dest_addr;
		dest_addr.sin_family = AF_INET;
		dest_addr.sin_port = htons(SERVEPORT);
		dest_addr.sin_addr.s_addr = INADDR_BROADCAST;

		char request_msg[MAXBUFFLEN];
		create_request_msg(serve_filename, request_msg);

		printf("broadcasting file request...\n");

		if(sendto(sockfd, request_msg, strlen(request_msg), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) == -1)
		{
			perror("broadcasting file request");
			return EXIT_FAILURE;
		}

		char response[MAXRESLEN];
		int len = sizeof(dest_addr);
		char recv_filename[10];
		sprintf(recv_filename, "received_%s", serve_filename);
		FILE *recv_file = fopen(recv_filename, "a");
		while(recvfrom(sockfd, response, MAXRESLEN, 0, (struct sockaddr *)&dest_addr, &len) > 0)
		{
			int offset;
			char data[MAXDGRAMLEN] = {'\0'};
			res_parser(response, data, &offset);	
			fprintf(recv_file, "%s", data);
		}
		fclose(recv_file);
		printf("Receiving file successful.\n");
		return 0;
	}

	// Listen mode
	if(argc == 6 && strcmp("-serve", argv[1]) == 0 && strcmp("-name", argv[2]) == 0 && strcmp("-path", argv[4]) == 0)
	{
		strcpy(serve_filename, argv[3]);
		strcpy(path, argv[5]);

		int client_sockfd, sockfd = init_serve_sock();
		if(sockfd == -1)
			return EXIT_FAILURE;

		int len;
		struct sockaddr_in client_addr;
		len = sizeof(client_addr);
		printf("Serving on port %d ...\n", SERVEPORT);

		// waiting for incomming request
		char buffer[MAXBUFFLEN];
		while(recvfrom(sockfd, buffer, MAXBUFFLEN, 0, (struct sockaddr *)&client_addr, &len) > 0)
		{
			pthread_t conn;

			// handler argument initialization
			client_conn_t handler_arg;
			handler_arg.addr = client_addr;
			handler_arg.fd = sockfd;
			strcpy(handler_arg.buffer, buffer);

			// create a thread for each client socket
			pthread_create(&conn, NULL, connection_handler, (void *)&handler_arg);
			pthread_detach(conn);
		}
	}
	else
		printf("usage:\n  %s -receive \"filename\": send broadcast request for the filename\n  %s -serve -name \"filename\" -path \"filepath\": listen for the file request\n", argv[0], argv[0]);
	return EXIT_FAILURE;
}
