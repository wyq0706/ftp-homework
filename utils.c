#include <stdio.h>
#include <netinet/in.h>   /* for sockaddr_in */
// #include <sys/types.h>     
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include<sys/ioctl.h>
#include<netdb.h>
#include<net/if.h>
/* for random */
#include<stdlib.h> 

//for select
#include <sys/time.h>

// #include <strings.h>   //for bzero
#include <string.h>
#include <ctype.h>
#include<time.h>
#include"globals.h"

/* Usage: To combine code and response to send back to the client.
Return: null. Response is written in the buff. */
void merge_response(char* buf, int code, char* response) {
	memset(buf, 0, strlen(buf));
	if (code == 257) {
		sprintf(buf, "%d \"%s\"\r\n", code, response);
	}
	else {
		sprintf(buf, "%d %s\r\n", code, response);
	}

}

/* Usage: To get the host ip address *
Return: String of IP; Error string if failed */
char *ip_search(void) {

	int sfd, intr;
	struct ifreq _buf[16];
	struct ifconf ifc;
	sfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sfd < 0)
		return ERRORIP;
	ifc.ifc_len = sizeof(_buf);
	ifc.ifc_buf = (caddr_t)_buf;
	if (ioctl(sfd, SIOCGIFCONF, (char *)&ifc))
		return ERRORIP;
	intr = ifc.ifc_len / sizeof(struct ifreq);
	while (intr-- > 0 && ioctl(sfd, SIOCGIFADDR, (char *)&_buf[intr]));
	close(sfd);
	return inet_ntoa(((struct sockaddr_in*)(&_buf[intr].ifr_addr))->sin_addr);
}


/* Usage: To combine ip and port as a string response to the PASV mode.
Return: null.  String written in buf that contains PASVMODE_INFO ,ip and port */
void merge_response_PASV(char*buf, int code, char* ip, int port_num) {
	memset(buf, 0, strlen(buf));
	char new_ip[30];
	memset(new_ip, 0, 30);
	for (int i = 0; ip[i] != '\0'; ++i) {
		if (ip[i] == '.') {
			new_ip[i] = ',';
		}
		else {
			new_ip[i] = ip[i];
		}
	}
	sprintf(buf, "%d %s(%s,%d,%d)\r\n", code, PASVMODE_INFO, new_ip, port_num / 256, port_num % 256);
}


/* Usage: To get a random port number between 20000 and 65535 for PASV mode.
Return: a port number */
int get_random_port_number() {

	srand((unsigned)time(NULL));
	return rand() % 45535 + 20000;
}

void initialize_all_clientinfo() {
	for (int i = 0; i < CLI_NUM; ++i) {
		client_set[i].state = INITIAL;
		client_set[i].port = 0;
		client_set[i].connfd_server = -1;
		client_set[i].connfd_client = -1;
		client_set[i].isTransfering = 0;
		//memset(client_set[i].thread_id, 0, sizeof(client_set[i].thread_id));
		memset(&client_set[i].conn_addr, 0, sizeof(client_set[i].conn_addr));
		memset(&client_set[i].connfd_client_addr, 0, sizeof(client_set[i].connfd_client_addr));
		memset(client_set[i].file_addr, 0, 100);
		/* set defualt file directory on server for client */
		strcpy(client_set[i].file_dir, file_dir);
		client_set[i].ifrest = 0;
	}
}
/* Usage: To create a socket for connection.
Read ip and port in client_info.
Return: -1 if error; else 0. */
int create_connection_socket(int client_id) {
	client_set[client_id].conn_addr.sin_family = AF_INET;    //IPV4	
	if (client_set[client_id].state == PORT) {
		/* use listen port number +1 as default connection port*/
		client_set[client_id].conn_addr.sin_port = htons(get_random_port_number());
	}
	else if (client_set[client_id].state == PASV) {
		/* already set*/
		// client_set[client_id].conn_addr.sin_port = client_set[client_id].port;
	}
	client_set[client_id].conn_addr.sin_addr.s_addr = INADDR_ANY;  //listen "0.0.0.0"
	printf("newcreate: %d\n", client_set[client_id].conn_addr.sin_port);
	if ((client_set[client_id].connfd_server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		printf("Error socket() when creating connection socket.\n");
		// ? strerror will com out with (hexinyizhuanchu)
		return -1;

	}
	if (client_set[client_id].state == PASV) {

		/* bind ip and port with the socket */
		/* if erro bind, change another random port */
		while (bind(client_set[client_id].connfd_server, (struct sockaddr *)&client_set[client_id].conn_addr, sizeof(client_set[client_id].conn_addr)) == -1) {
			printf("%d %d qaq Error bind()\n", client_set[client_id].state, client_set[client_id].conn_addr.sin_port);
			client_set[client_id].conn_addr.sin_port = client_set[client_id].conn_addr.sin_port - 1;
			//return 1; 
		}
	}
	//printf("a new socket:%d\n", client_set[client_id].conn_addr.sin_port);
	return 0;
}


/* Usage: To read ip and port out from client's POST cmd.
Return: 0 for ok, -1 for error.  Write ip and port into client_info. */
int parse_ip_and_port(char* msg, int client_id) {
	int h1, h2, h3, h4, p1, p2,e;
	int num=sscanf(msg, "%*s%d,%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2,&e);
	printf("num: %d", num);
	if (num != 6) {
		return -1;
	}
	printf("oook1\n");
	memset(&client_set[client_id].connfd_client_addr, 0, sizeof(client_set[client_id].connfd_client_addr));
	client_set[client_id].connfd_client_addr.sin_family = AF_INET;
	client_set[client_id].connfd_client_addr.sin_port = htons(p1 * 256 + p2);
	char tmp_ip[20];
	printf("oook2\n");
	sprintf(tmp_ip, "%d.%d.%d.%d", h1, h2, h3, h4);
	client_set[client_id].connfd_client_addr.sin_addr.s_addr = inet_addr(tmp_ip);
	return 0;

}