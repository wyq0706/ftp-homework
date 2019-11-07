#include <stdio.h>
#include <netinet/in.h>   /* for sockaddr_in */   
#include <sys/socket.h>
#include <errno.h>
#include<sys/ioctl.h>
/* for random */
#include<stdlib.h> 
//for select
#include <sys/time.h>
#include <unistd.h>
#include <sys/select.h>
#include <string.h>
#include <ctype.h>
#include<time.h>

#include"globals.h"
#include"file_op.h"
#include"cmd_op.h"
#include"utils.h"


#define BUFF_SIZE 200
#define NULL ((void *)0)
/* defualt port */
int ser_port = 21;  // listening port number


int main(int argc, char **argv)
{

	int listenfd;
	int i;

	char resv_message[BUFF_SIZE];
	memset(resv_message, 0, BUFF_SIZE);
	char input_message[BUFF_SIZE];
	memset(input_message, 0, BUFF_SIZE);

	memset(client_fds, 0, sizeof(client_fds));
	struct sockaddr_in addr;

	//bind ip and port
	addr.sin_family = AF_INET;    //IPV4
	if (argc >= 2) {
		ser_port = atoi(argv[2]);
		printf("%d\n", ser_port);
	}
	if (argc > 3) {
		memset(file_dir, 0, 100);
		strcpy(file_dir, argv[4]);
		strcat(file_dir, "/");
	}
	else {
		memset(file_dir, 0, 100);
		strcpy(file_dir, "/tmp/");
	}
	initialize_all_clientinfo();
	addr.sin_port = htons(ser_port);
	addr.sin_addr.s_addr = INADDR_ANY;  //listen "0.0.0.0"

	//create a socket
	if ((listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		printf("Error socket()\n");
		return 1;
	}

	//bind ip and port with socket
	if (bind(listenfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		printf("Error bind()\n");
		return 1;
	}

	//listen
	if (listen(listenfd, 10) < 0) {
		printf("Error listen()\n");
		return -1;
	}


	//fd_set
	int max_fd = 1;
	struct timeval mytime; // how long we wait
	printf("Waiting for clients to connnect!\n");

	while (1) {
		mytime.tv_sec = 27;
		mytime.tv_usec = 0;

		//clean out each time polling
		FD_ZERO(&ser_fdset);
		//add server
		FD_SET(listenfd, &ser_fdset);
		// printf("listenfd:%d\n",listenfd);
		if (max_fd < listenfd)
		{
			max_fd = listenfd;
		}

		//add client
		for (i = 0; i < CLI_NUM; i++)
		{
			if (client_fds[i] != 0)
			{
				FD_SET(client_fds[i], &ser_fdset);
				if (max_fd < client_fds[i])
				{
					max_fd = client_fds[i];
				}
			}
		}
		printf("test22222222222222222222\n");
		//select
		int ret = select(max_fd + 1, &ser_fdset, NULL, NULL, &mytime);
		// int select(int maxfdp1,fd_set *readset,fd_set *writeset,fd_set *exceptset,struct timeval *timeout);
		// return number of file desciptors that have been prepared; 0 as timeout;-1 as failure;

		if (ret < 0) {
			printf("select failure\n");
			continue;   // ? why continue
		}
		else if (ret == 0) {
			// printf("time out!");  // no file to read or write
			continue;
		}
		int client_sock_fd = -1;
		if (FD_ISSET(listenfd, &ser_fdset)) {
			//struct sockaddr_in client_address;
			//socklen_t address_len;
			//client_sock_fd = accept(listenfd, (struct sockaddr *)&client_address, &address_len);
			/* if not use NULL error happens why??? */
			client_sock_fd = accept(listenfd, NULL, NULL);
			printf("test111111111111111111\n");
			printf("client_sock_fd %d", client_sock_fd);
			if (client_sock_fd > 0) { // a client comes
				int flags = -1;  // flag means which i
				printf("oooo\n");
				//  A fd is distributed; no more than 3 clients; if more, break out of the loop and flag is -1
				for (i = 0; i < CLI_NUM; i++) {
					if (client_fds[i] == 0) {
						flags = i;
						printf("llll\n");
						client_fds[i] = client_sock_fd;
						break;
					}
				}


				if (flags >= 0) {
					printf("A new user, Client[%d] has added sucessfully!\n", flags);

					// bzero(input_message,BUFF_SIZE);
					// strncpy(input_message, CONNECTOK_INFO, 100);
					merge_response(input_message, 220, CONNECTOK_INFO);
					send(client_sock_fd, input_message, strlen(input_message), 0);
					client_set[flags].state = INITIAL;
					/* renew file directory */
					memset(client_set[flags].file_dir, 0, FILEDIR_LEN);
					strcpy(client_set[flags].file_dir, file_dir);

					//client_set[flags].connection_build = 0;
				}
				else { //flags=-1
					char tmp_message[] = "The client is full! You fail to join! Please wait...\n";
					// bzero(input_message,BUFF_SIZE);
					strncpy(input_message, tmp_message, 100);
					send(client_sock_fd, input_message, strlen(input_message), 0);
					// what about the client? does it need to log out?
				}
			}
		}


		//deal with the message
		for (i = 0; i < CLI_NUM; i++) {
			if (client_fds[i] != 0) {
				if (FD_ISSET(client_fds[i], &ser_fdset)) {
					// bzero(resv_message,BUFF_SIZE);
					// clear the buf
					memset(resv_message, 0, 4096);
					// memset(input_message, 0, 4096);
					int byte_num = read(client_fds[i], resv_message, BUFF_SIZE);
					if (byte_num < 0) {
						printf("rescessed error!\n");
						continue;
					}
					// 0 means some client quits
					else if (byte_num == 0) { //cancel fdset and set fd=0
						printf("client[%d] abortly exit!\n", i);   // A client exits the program itself without sending QUIT command.
						FD_CLR(client_fds[i], &ser_fdset);
						client_fds[i] = 0;
						client_set[i].state = INITIAL;
						// printf("clien[%d] exit!\n",i);
						continue;  //if using break, the server will quit 
					}
					resv_message[byte_num] = '\n';
					resv_message[byte_num + 1] = '\0';

					printf("message form client[%d]:%s", i, resv_message);

					parse_command_and_do(resv_message, i);
				}
			}

		}
	}
	return 0;
}

