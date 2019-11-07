#include <sys/socket.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include <pthread.h>
#include <signal.h> 

#include"globals.h"
#include"file_op.h"
#include"dir.h"



void merge_response(char* buf, int code, char* response);
void merge_response_PASV(char*buf, int code, char* ip, int port_num);
int get_random_port_number();
char *ip_search(void);
int create_connection_socket(int client_id);
int parse_ip_and_port(char* msg, int client_id);

extern char file_dir[100];


void do_USER(char* msg,int i) {
	char send_msg[100];
	if (client_set[i].state == INITIAL || client_set[i].state == USER ) {
		merge_response(send_msg, 331, LOGINOK_INFO);		
		client_set[i].state = USER;
	}
	else {
		merge_response(send_msg, 230, "already logged in");
		
	}
	send(client_fds[i], send_msg, strlen(send_msg), 0);
	client_set[i].rnfr = 0;
}

void do_PASS(char* msg, int i) {
	char send_msg[100];
	if (client_set[i].state == USER) {
		merge_response(send_msg, 230, WELCOME_INFO); // what about multi-line		
		client_set[i].state = PASS;
	}
	else if (client_set[i].state == INITIAL) {
		merge_response(send_msg, 503, "Login with USER first."); 
	}
	else{
		merge_response(send_msg, 202, "Already logged-in.");
		
	}
	send(client_fds[i], send_msg, strlen(send_msg), 0);
	client_set[i].rnfr = 0;
}

void do_QUIT(char* msg,int i) {
	char send_msg[100];
	merge_response(send_msg, 221, "Thank you for using the FTP service on ftp.ssast.org.");
	send(client_fds[i], send_msg, strlen(send_msg), 0);
	printf("client[%d] exit!\n", i);
	FD_CLR(client_fds[i], &ser_fdset);
	client_fds[i] = 0;
	client_set[i].state = INITIAL;
}

void do_PASV(char* msg, int i) {
	char send_msg[100];
	/* if not logged in */
	if (client_set[i].state == USER || client_set[i].state==INITIAL) {
		merge_response(send_msg, 503, "Login First."); // what about multi-line		
		client_set[i].state = INITIAL;
		send(client_fds[i], send_msg, strlen(send_msg),0);
		client_set[i].rnfr = 0;
		return;
	}

	/* to build a new socket and drop any connection already make */
	if (client_set[i].state == PORT || client_set[i].state == PASV) {
		close(client_set[i].connfd_server);
	}
	/* set new port */
	client_set[i].port = get_random_port_number();
	client_set[i].conn_addr.sin_port = htons(client_set[i].port);
	client_set[i].state = PASV;

	if (create_connection_socket(i) != -1) {
		/* if success in creating */
		/* start listening */
		if (listen(client_set[i].connfd_server, 10) < 0) {
			printf("Error listen()\n");
			merge_response(send_msg, 425, "Cannot open passive conenction.");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
			client_set[i].state = PASS;
		}
		else {
			merge_response_PASV(send_msg, 227, ip_search(), ntohs(client_set[i].conn_addr.sin_port));
			send(client_fds[i], send_msg, strlen(send_msg), 0);
			printf("%s\n", send_msg);
		}
	}
	else {
		/* failure to create */
		merge_response(send_msg, 425, "Cannot open passive conenction.");
		send(client_fds[i], send_msg, strlen(send_msg), 0);
		client_set[i].state = PASS;
	}
	client_set[i].rnfr = 0;
}

void do_PORT(char* msg, int i) {
	char send_msg[100];
	/* if not logged in */
	if (client_set[i].state == USER || client_set[i].state == INITIAL) {
		merge_response(send_msg, 503, "Login First."); // what about multi-line		
		client_set[i].state = INITIAL;
		send(client_fds[i], send_msg, strlen(send_msg), 0);
		client_set[i].rnfr = 0;
		return;
	}

	if (client_set[i].state == PORT || client_set[i].state == PASV) {
		/* close old socket and drop connection*/
		close(client_set[i].connfd_server);
	}
	client_set[i].state = PORT;

	if (create_connection_socket(i) != -1) {
		/* write client ip and port into client info */
		if (parse_ip_and_port(msg, i) != -1) {
			printf("ook3\n");
			merge_response(send_msg, 200, PORTMODE_INFO);
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else {
			merge_response(send_msg, 425, "Fail to parse ip and port out. Check if using <h1,h2,h3,h4,p1,p2> .");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
	}
	else {
		merge_response(send_msg, 425, "Fail to enter PORT mode.");
		send(client_fds[i], send_msg, strlen(send_msg), 0);
		client_set[i].state = PASS;
	}
	client_set[i].rnfr = 0;
}

void do_RETR(char* msg, int i) {
	char send_msg[200];
	/* if not logged in */
	if (client_set[i].state == USER || client_set[i].state == INITIAL) {
		merge_response(send_msg, 503, "Login First."); // what about multi-line		
		client_set[i].state = INITIAL;
		send(client_fds[i], send_msg, strlen(send_msg), 0);
		client_set[i].rnfr = 0;
		return;
	}
	if (client_set[i].state == PASV || client_set[i].state == PORT) {
		get_file_addr(msg, i);
		if (establish_connection(i) == -1) {
			return;
		}
		else {
			printf("senddatastartok\n");
			FILE* infile;
			infile = fopen(client_set[i].file_addr, "rb");
			if (infile == NULL) {
				printf("file not exist");
				merge_response(msg, 550, "No such file.");
				send(client_fds[i], msg, strlen(msg), 0);
				client_set[i].rest_pos = 0;
				client_set[i].state = PASS;
				client_set[i].rnfr = 0;
				return;
			}
			fseek(infile, 0L, SEEK_END);
			int size = ftell(infile);
			fclose(infile);
			memset(send_msg, 0, strlen(send_msg));
			sprintf(send_msg, "%d Opening BINARY mode data connection for %s (%d bytes).\r\n", 150, client_set[i].file_addr, size);
			send(client_fds[i], send_msg, strlen(send_msg), 0);

			/* using multithread to tranfer file so as not to block the server */
			//pthread_t id;
			
			thread_arg* argp = (thread_arg*)malloc(sizeof(thread_arg));
			
			/* get file: using different socket */
			if (client_set[i].state == PASV) {
				argp->socket = client_set[i].connfd_client;
				argp->id = i;
				pthread_create(&client_set[i].thread_id, 0, read_file_to_send, (void*)argp);
				pthread_detach(client_set[i].thread_id);
			}
			else if (client_set[i].state == PORT) {
				argp->socket = client_set[i].connfd_server;
				argp->id = i;
				pthread_create(&client_set[i].thread_id, 0, read_file_to_send, (void*)argp);
				pthread_detach(client_set[i].thread_id);
			}					
		}			
	}
	else {
		merge_response(send_msg, 425, "No Tcp connection was established");
		send(client_fds[i], send_msg, strlen(send_msg), 0);
	}
	client_set[i].rnfr = 0;
	client_set[i].state = PASS;
}

void do_STOR(char* msg,int i) {
	char send_msg[100];
	/* if not logged in */
	if (client_set[i].state == USER || client_set[i].state == INITIAL) {
		merge_response(send_msg, 503, "Login First."); // what about multi-line		
		client_set[i].state = INITIAL;
		send(client_fds[i], send_msg, strlen(send_msg), 0);
		client_set[i].rnfr = 0;
		return;
	}

	if (client_set[i].state == PASV || client_set[i].state == PORT) {
		parse_filename(msg, i);
		if (establish_connection(i) == -1) {
			return;
		}
		else {

			memset(send_msg, 0, strlen(send_msg));
			sprintf(send_msg, "%d Opening BINARY mode data connection.\r\n", 150);
			send(client_fds[i], send_msg, strlen(send_msg), 0);
			//pthread_t id;
			thread_arg* argp = (thread_arg*)malloc(sizeof(thread_arg));
			/* get file: using different socket */
			if (client_set[i].state == PASV) {
				argp->socket = client_set[i].connfd_client;
			}
			else if (client_set[i].state == PORT) {
				argp->socket = client_set[i].connfd_server;
			}
			argp->id = i;
			pthread_create(&client_set[i].thread_id, 0, get_and_write_file, (void*)argp);
			pthread_detach(client_set[i].thread_id);
			/* transfer complete: telling the client ro close socket */
			/*merge_response(send_msg, 226, "Transfer_complete.");
			send(client_fds[i], send_msg, strlen(send_msg), 0);*/
			client_set[i].state = PASS;
		}
	}
	else {
		merge_response(send_msg, 425, "No Tcp connection was established");
		send(client_fds[i], send_msg, strlen(send_msg), 0);
	}
	client_set[i].rnfr = 0;
}

void do_SYST(char* msg, int i) {
	char send_msg[30];
	/* if not logged in */
	if (client_set[i].state == USER || client_set[i].state == INITIAL) {
		merge_response(send_msg, 503, "Login First."); // what about multi-line		
		client_set[i].state = INITIAL;
		send(client_fds[i], send_msg, strlen(send_msg), 0);
		client_set[i].rnfr = 0;
		return;
	}

	merge_response(send_msg, 215, "UNIX Type: L8");
	send(client_fds[i], send_msg, strlen(send_msg), 0);
	client_set[i].rnfr = 0;
}

void do_TYPE(char* msg, int i) {
	char send_msg[30];
	/* if not logged in */
	if (client_set[i].state == USER || client_set[i].state == INITIAL) {
		merge_response(send_msg, 503, "Login First."); // what about multi-line		
		client_set[i].state = INITIAL;
		send(client_fds[i], send_msg, strlen(send_msg), 0);
		client_set[i].rnfr = 0;
		return;
	}

	if (strncmp(msg, "TYPE I", 6) == 0) {
		printf("oktype\n");
		merge_response(send_msg, 200, "Type set to I.");
		send(client_fds[i],send_msg, strlen(send_msg), 0);
	}
	else {
		merge_response(send_msg, 425, "Error Type input");
		send(client_fds[i], send_msg, strlen(send_msg), 0);
	}
	client_set[i].rnfr = 0;
}

void do_RNFR(char* msg, int i) {
	char send_msg[60];
	/* if not logged in */
	if (client_set[i].state == USER || client_set[i].state == INITIAL) {
		merge_response(send_msg, 503, "Login First."); // what about multi-line		
		client_set[i].state = INITIAL;
		send(client_fds[i], send_msg, strlen(send_msg), 0);
		client_set[i].rnfr = 0;
		return;
	}

	if (get_pathname(msg, i) != -1) {
		merge_response(send_msg, 350, "Requested file action pending further information.");
		send(client_fds[i], send_msg, strlen(send_msg), 0);
	}
	else {
		merge_response(send_msg, 550, "File unavailable.");
		send(client_fds[i], send_msg, strlen(send_msg), 0);
	}
}

void do_MKD(char* msg, int i) {
	char send_msg[30];
	/* if not logged in */
	if (client_set[i].state == USER || client_set[i].state == INITIAL) {
		merge_response(send_msg, 503, "Login First."); // what about multi-line		
		client_set[i].state = INITIAL;
		send(client_fds[i], send_msg, strlen(send_msg), 0);
		client_set[i].rnfr = 0;
		return;
	}

	if (make_dir(msg, i) != -1) {
		merge_response(send_msg, 257, "Make dir over.");
		send(client_fds[i], send_msg, strlen(send_msg), 0);
	}
	else {
		merge_response(send_msg, 550, "Make dir failure.");
		send(client_fds[i], send_msg, strlen(send_msg), 0);
	}

	client_set[i].rnfr = 0;
}

void do_RMD(char* msg, int i) {
	char send_msg[30];
	/* if not logged in */
	if (client_set[i].state == USER || client_set[i].state == INITIAL) {
		merge_response(send_msg, 503, "Login First."); // what about multi-line		
		client_set[i].state = INITIAL;
		send(client_fds[i], send_msg, strlen(send_msg), 0);
		client_set[i].rnfr = 0;
		return;
	}

	if (remove_dir(msg, i) != -1) {
		merge_response(send_msg, 257, "Remove dir over.");
		send(client_fds[i], send_msg, strlen(send_msg), 0);
	}
	else {
		merge_response(send_msg, 500, "Remove dir failure.");
		send(client_fds[i], send_msg, strlen(send_msg), 0);
	}

	client_set[i].rnfr = 0;
}

void do_RNTO(char* msg, int i) {
	char send_msg[60];
	/* if not logged in */
	if (client_set[i].state == USER || client_set[i].state == INITIAL) {
		merge_response(send_msg, 503, "Login First."); // what about multi-line		
		client_set[i].state = INITIAL;
		send(client_fds[i], send_msg, strlen(send_msg), 0);
		client_set[i].rnfr = 0;
		return;
	}

	if (client_set[i].rnfr == 1) {
		if (rename_pathname(msg, i) != -1) {
			printf("kkk\n");
			merge_response(send_msg, 250, "Requested file action okay, file renamed.");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else {
			merge_response(send_msg, 553, "Cannot rename file.");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
	}
	else {
		merge_response(send_msg, 503, "Cannot find the file which has to be renamed.");
		send(client_fds[i], send_msg, strlen(send_msg), 0);
	}
	client_set[i].rnfr = 0;
}

void do_PWD(char* msg, int i) {
	char send_msg[100];
	/* if not logged in */
	if (client_set[i].state == USER || client_set[i].state == INITIAL) {
		merge_response(send_msg, 503, "Login First."); // what about multi-line		
		client_set[i].state = INITIAL;
		send(client_fds[i], send_msg, strlen(send_msg), 0);
		client_set[i].rnfr = 0;
		return;
	}

	get_cur_dir(send_msg, i);
	// merge_response(input_message, 200, client_set[i].file_dir);
	printf("%s", send_msg);
	send(client_fds[i], send_msg, strlen(send_msg), 0);
	client_set[i].rnfr = 0;
}

void do_CWD(char* msg, int i) {
	char send_msg[30];
	/* if not logged in */
	if (client_set[i].state == USER || client_set[i].state == INITIAL) {
		merge_response(send_msg, 503, "Login First."); // what about multi-line		
		client_set[i].state = INITIAL;
		send(client_fds[i], send_msg, strlen(send_msg), 0);
		client_set[i].rnfr = 0;
		return;
	}

	change_filedir(msg, i);
	merge_response(send_msg, 250, "Change path over.");
	send(client_fds[i], send_msg, strlen(send_msg), 0);
	client_set[i].rnfr = 0;
}

void do_LIST(char* msg, int i) {
	char send_msg[30];
	/* if not logged in */
	if (client_set[i].state == USER || client_set[i].state == INITIAL) {
		merge_response(send_msg, 503, "Login First."); // what about multi-line		
		client_set[i].state = INITIAL;
		send(client_fds[i], send_msg, strlen(send_msg), 0);
		client_set[i].rnfr = 0;
		return;
	}

	if (client_set[i].state == PASV || client_set[i].state == PORT) {
		printf("kkkkkkkuuuuu\n");
		if (establish_connection(i) == -1) {
			client_set[i].state = PASS;
			client_set[i].rnfr = 0;
			return;
		}
		merge_response(send_msg, 150, "Here comes the directory listing.");
		send(client_fds[i], send_msg, strlen(send_msg), 0);
		int flag = 0;
		/* get list: using different socket */
		if (client_set[i].state == PASV) {
			printf("qqqqqyyyy\n");

			if (get_list(client_set[i].connfd_client, i, msg) == -1) {
				flag = 1;
			}
			close(client_set[i].connfd_client);
		}
		else if (client_set[i].state == PORT) {
			if (get_list(client_set[i].connfd_server, i, msg) == -1) {
				flag = 1;
			}
			close(client_set[i].connfd_server);
		}
		printf("overlisttttttttttttttt\n");
		sleep(0.5);
		if (flag == 0) {
			/* transfer complete: telling the client ro close socket */
			merge_response(send_msg, 226, "Directory send OK.");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
			client_set[i].state = PASS;
		}
		else {
			merge_response(send_msg, 426, "Get list failure.");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
			client_set[i].state = PASS;
		}

	}
	else {
		merge_response(send_msg, 425, "No Tcp connection was established");
		send(client_fds[i], send_msg, strlen(send_msg), 0);
	}
	client_set[i].rnfr = 0;
}

void do_REST(char* msg, int i) {
	char send_msg[100];
	/* if not logged in */
	if (client_set[i].state == USER || client_set[i].state == INITIAL) {
		merge_response(send_msg, 503, "Login First."); // what about multi-line		
		client_set[i].state = INITIAL;
		send(client_fds[i], send_msg, strlen(send_msg), 0);
		client_set[i].rnfr = 0;
		return;
	}

	int num=sscanf(msg, "%*s %ld", &client_set[i].rest_pos);
	if (num == 1) {
		sprintf(send_msg, "%d %s%ld%s\r\n", 350, "Restarting at ", client_set[i].rest_pos, " . Send STORE or RETRIEVE to initiate transfer.");
		send(client_fds[i], send_msg, strlen(send_msg), 0);
	}
	else {
		merge_response(send_msg, 501, "Not a valid number.");
		send(client_fds[i], send_msg, strlen(send_msg), 0);
	}
	client_set[i].rnfr = 0;
}

void do_ABOR(char* msg, int i) {
	char send_msg[100];
	/* if not logged in */
	if (client_set[i].state == USER || client_set[i].state == INITIAL) {
		merge_response(send_msg, 503, "Login First."); // what about multi-line		
		client_set[i].state = INITIAL;
		send(client_fds[i], send_msg, strlen(send_msg), 0);
		client_set[i].rnfr = 0;
		return;
	}
	/* check if thread is alive */
	int kill_ret = pthread_kill(client_set[i].thread_id, 0);
	if (kill_ret == ESRCH) {
		merge_response(send_msg, 226, "ABOR command successful. Nothing happens."); // what about multi-line		
		send(client_fds[i], send_msg, strlen(send_msg), 0);
	}
	else if (kill_ret == EINVAL) {
		printf("调用传递一个无用的信号\n");
		merge_response(send_msg, 226, "Nothing happens...."); // what about multi-line		
		send(client_fds[i], send_msg, strlen(send_msg), 0);
	}
	else {
		printf("线程存在\n");
		if (pthread_cancel(client_set[i].thread_id) == 0) {
			/* cancel success */
			//merge_response(send_msg, 226, "Transfer aborted."); // what about multi-line		
			//send(client_fds[i], send_msg, strlen(send_msg), 0);
			//sleep(1);
			close(client_set[i].connfd_server);
			printf("yesaborted!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
			memset(send_msg, 0, strlen(send_msg));
			if (client_set[i].isTransfering == 2) {
				sprintf(send_msg, "%d ABOR command successful.Transfer aborted at %ld.\r\n", 226, client_set[i].lastPos - 4096); // what about multi-line
			}
			else {
				FILE* infile;
				infile = fopen(client_set[i].file_addr, "rb");
				fseek(infile, 0L, SEEK_END);
				long size = ftell(infile);
				fclose(infile);
				sprintf(send_msg, "%d ABOR command successful.Transfer has been aborted at %ld.\r\n", 226,size);
			}
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else {
			merge_response(send_msg, 226, "Transfer aborted."); // what about multi-line		
			send(client_fds[i], send_msg, strlen(send_msg), 0);
			//sleep(1);
			merge_response(send_msg, 226, "ABOR Error happens."); // what about multi-line		
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		client_set[i].isTransfering = 0;
	}
	client_set[i].rnfr = 0;
}

/* Parse command and check for syntax here. 
   Do the command job in further functions. */
void parse_command_and_do(char* command, int i) {
	char verb[5];
	char param1[50];
	char param2[50];
	char send_msg[50];
	int num = sscanf(command, "%s %s %s", verb, param1, param2);
	if (num >= 3) {
		merge_response(send_msg, 501, "Two many arguments. ");
		send(client_fds[i], send_msg, strlen(send_msg), 0);
		return;
	}
	if (strcmp(verb, "USER") == 0) {
		if (num >= 3) {
			merge_response(send_msg, 501, "Two many arguments. ");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else{
			do_USER(command, i);
		}
	}
	else if (strcmp(verb, "PASS") == 0) {
		if (num >= 3) {
			merge_response(send_msg, 501, "Two many arguments. ");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else {
			do_PASS(command, i);
		}
	}
	else if (strcmp(verb, "QUIT") == 0) {
		if (num >= 2) {
			merge_response(send_msg, 501, "Two many arguments. ");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else {
			do_QUIT(command, i);
		}
	}
	else if (strcmp(verb, "PASV") == 0) {
		if (num >= 2) {
			merge_response(send_msg, 501, "Two many arguments. ");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else {
			do_PASV(command, i);
		}
	}
	else if (strcmp(verb, "PORT") == 0) {
		if (num >2) {
			merge_response(send_msg, 501, "Two many arguments. ");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else if (num == 1) {
			merge_response(send_msg, 501, "Two little arguments. ");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else{
			do_PORT(command, i);
		}
	}
	else if (strcmp(verb, "RETR") == 0) {
		if (num >2) {
			merge_response(send_msg, 501, "Two many arguments. ");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else if (num == 1) {
			merge_response(send_msg, 501, "Two little arguments. ");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else {
			do_RETR(command, i);
		}
	}
	else if (strcmp(verb, "STOR") == 0) {
		if (num >2) {
			merge_response(send_msg, 501, "Two many arguments. ");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else if (num == 1) {
			merge_response(send_msg, 501, "Two little arguments. ");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else {
			do_STOR(command, i);
		}
	}
	else if (strcmp(verb, "SYST") == 0) {
		if (num >1) {
			merge_response(send_msg, 501, "Two many arguments. ");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else {
			do_SYST(command, i);
		}
	}
	else if (strcmp(verb, "TYPE") == 0) {
		if (num >2) {
			merge_response(send_msg, 501, "Two many arguments. ");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else if (num == 1) {
			merge_response(send_msg, 501, "Two little arguments. ");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else {
			do_TYPE(command, i);
		}
	}
	else if (strcmp(verb, "RNFR") == 0) {
		if (num >2) {
			merge_response(send_msg, 501, "Two many arguments. ");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else if (num == 1) {
			merge_response(send_msg, 501, "Two little arguments. ");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else {
			do_RNFR(command, i);
		}
	}
	else if (strcmp(verb, "RNTO") == 0) {
		if (num >2) {
			merge_response(send_msg, 501, "Two many arguments. ");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else if (num == 1) {
			merge_response(send_msg, 501, "Two little arguments. ");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else {
			do_RNTO(command, i);
		}
	}
	else if (strcmp(verb, "MKD") == 0) {
		if (num >2) {
			merge_response(send_msg, 501, "Two many arguments. ");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else if (num == 1) {
			merge_response(send_msg, 501, "Two little arguments. ");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else {
			do_MKD(command, i);
		}
	}
	else if (strcmp(verb, "RMD") == 0) {
		if (num >2) {
			merge_response(send_msg, 501, "Two many arguments. ");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else if (num == 1) {
			merge_response(send_msg, 501, "Two little arguments. ");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else {
			do_RMD(command, i);
		}
	}
	else if (strcmp(verb, "PWD") == 0) {
		if (num >21) {
			merge_response(send_msg, 501, "Two many arguments. ");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else {
			do_PWD(command, i);
		}
	}
	else if (strcmp(verb, "CWD") == 0) {
		if (num >2) {
			merge_response(send_msg, 501, "Two many arguments. ");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else if (num == 1) {
			merge_response(send_msg, 501, "Two little arguments. ");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else {
			do_CWD(command, i);
		}
	}
	else if (strcmp(verb, "LIST") == 0) {
		if (num >2) {
			merge_response(send_msg, 501, "Two many arguments. ");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else {
			do_LIST(command, i);
		}
	}
	else if (strcmp(verb, "REST") == 0) {
		if (num >2) {
			merge_response(send_msg, 501, "Two many arguments. ");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else if (num == 1) {
			merge_response(send_msg, 501, "Two little arguments. ");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else {
			do_REST(command, i);
		}
	}
	else if (strcmp(verb, "ABOR") == 0) {
		if (num >=2) {
			merge_response(send_msg, 501, "Two many arguments. ");
			send(client_fds[i], send_msg, strlen(send_msg), 0);
		}
		else {
			do_ABOR(command, i);
		}
	}
	else {
		merge_response(send_msg, 501, "TInvalid command. ");
		send(client_fds[i], send_msg, strlen(send_msg), 0);
	}

}