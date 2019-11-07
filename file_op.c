#include <sys/socket.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<pthread.h>

#include"globals.h"

void merge_response(char* buf, int code, char* response);

/* Usage: get and write file; capable of handling large copies of files && support REST cmd
Notion: reading buf from socket defined in parameters
Return: */
void* get_and_write_file(void* argp) {	
	thread_arg* arg = (thread_arg*)argp;
	int sd_socket = arg->socket;
	int client_id = arg->id;
	client_set[client_id].isTransfering = 1;
	FILE* outfile;
	client_set[client_id].lastPos = 0;
	if (client_set[client_id].rest_pos == 0) {
		outfile = fopen(client_set[client_id].file_addr, "wb");
	}
	else {
		outfile = fopen(client_set[client_id].file_addr, "ab");
		//fseek(outfile, 0L, SEEK_END);
		printf("Yesinrestttttt: %ld", client_set[client_id].rest_pos);
	}
	int byte_num = 0;
	printf("startRead\n");
	char msg[TRANSFER_LEN];
	memset(msg, 0, TRANSFER_LEN);
	long pos = 0;
	//long rest = client_set[client_id].rest_pos;
	
	while (1)				//判断文件是否结束
	{
		pthread_testcancel();
		byte_num = recv(sd_socket, msg, TRANSFER_LEN, 0);
		pthread_testcancel();
		/* if using read, erro happens?? dont know why */
		if (byte_num == 0) {
			break;
		}
		if (byte_num < 0) {
			printf("error reading file from socket.\n");
			return (void*)-1;
		}
		//if (pos<rest&&pos + byte_num>rest) {
		//	fwrite(&msg[rest-pos], 1, byte_num-rest+pos, outfile);
		//}
		//else if (pos >= rest) {
		//	fwrite(msg, 1, byte_num, outfile);
		//}
		fwrite(msg, 1, byte_num, outfile);
		client_set[client_id].lastPos += byte_num;
		pos += byte_num;
		// printf("%s\n", resv_message);
		memset(msg, 0, TRANSFER_LEN);
	}
	fclose(outfile);
	printf("over?\n");
	char send_msg[100];
	merge_response(send_msg, 226, "Transfer_complete.");
	send(client_fds[client_id], send_msg, strlen(send_msg), 0);
	client_set[client_id].rest_pos = 0;
	client_set[client_id].isTransfering = 0;
	close(sd_socket);
	pthread_exit(0);
}

/* Usage: read file and send to socket && support REST cmd
Return: 0 for ok; -1 for error */
void * read_file_to_send(void* argp) {	
	thread_arg* arg = (thread_arg*)argp;
	int sd_socket = arg->socket;
	int i = arg->id;
	client_set[i].isTransfering = 2;
	FILE* infile;
	infile = fopen(client_set[i].file_addr, "rb");
	fseek(infile, client_set[i].rest_pos, 0); 
	int byte_num = 0;
	printf("mydebug\n");
	client_set[i].lastPos=0;
	while (!feof(infile)) {				//判断文件是否结束
		memset(client_set[i].file_buf, 0, TRANSFER_LEN);
		byte_num = fread(client_set[i].file_buf, 1, TRANSFER_LEN, infile);
		printf("mudebugqaq%d\n", byte_num);
		send(sd_socket, client_set[i].file_buf, byte_num, 0);
		client_set[i].lastPos += byte_num;
	}
	printf("???debug\n");
	fclose(infile);
	char send_msg[100];
	sleep(0.5);
	merge_response(send_msg, 226, "Transfer_complete.");
	send(client_fds[i], send_msg, strlen(send_msg),0);
	client_set[i].rest_pos = 0;
	close(sd_socket);
	client_set[i].isTransfering = 0;
	pthread_exit(0);
	//return;
}

/* Usage: get file address from RETR cmd
Return: null */
void get_file_addr(char* msg, int client_id) {
	char verb[5];
	char tmp_addr[100];
	memset(client_set[client_id].file_addr, 0, 100);
	sscanf(msg, "%s%s", verb, client_set[client_id].file_addr);
	if (client_set[client_id].file_addr[0] == '/') {
		/* absolute path */
		printf("absolute path: %s\n", client_set[client_id].file_addr);
	}
	else {
		strcpy(tmp_addr, client_set[client_id].file_dir);
		strcat(tmp_addr, client_set[client_id].file_addr);
		strcpy(client_set[client_id].file_addr, tmp_addr);
		printf("relative path: %s\n", client_set[client_id].file_addr);
	}

}

/* accept or connect in RETR, STOR, LIST cmd 
   0 for ok; -1 for error */
int establish_connection(int i) {
	char msg[200];
	memset(msg, 0, 200);
	if (client_set[i].state == PASV) {
		if ((client_set[i].connfd_client = accept(client_set[i].connfd_server, NULL, NULL)) == -1) {
			printf("Error accept(): %s(%d)\n", strerror(errno), errno);
			merge_response(msg, 425, "Cannot establish the data conenction.");
			client_set[i].state = PASS;
			send(client_fds[i], msg, strlen(msg), 0);
			return -1;
		}
	}
	else if (client_set[i].state == PORT) {
		if (connect(client_set[i].connfd_server, (struct sockaddr*)&client_set[i].connfd_client_addr, sizeof(client_set[i].connfd_client_addr)) < 0) {
			printf("Error connect(): %s(%d)\n", strerror(errno), errno);
			merge_response(msg, 425, "Cannot establish the data conenction.");
			client_set[i].state = PASS;
			send(client_fds[i], msg, strlen(msg), 0);
			return -1;
		}
	}
	/* sending start transfering msg to client */



	return 0;
}

/* parse file name out of the STOR cmd; combine it with default file directory (Used in STOR cmd)
write it into client_info */
void parse_filename(char* msg, int client_id) {
	int sym = 0;
	int i = 0;
	printf("debug444\n");
	for (i = 0; msg[i] != '\0'&&msg[i] != '\r'&&msg[i] != '\n'; ++i) {
		if (msg[i] == '/' || msg[i] == ' ') {
			sym = i;
		}
	}
	printf("debug11\n");
	memset(client_set[client_id].file_addr, 0, 100);
	char name[30];
	memset(name, 0, 30);
	for (i = sym + 1; msg[i] != '\0'&&msg[i] != '\r'&&msg[i] != '\n'; ++i) {
		name[i - sym - 1] = msg[i];
	}
	printf("debug22\n");
	int dir_len = strlen(client_set[client_id].file_dir);
	sprintf(client_set[client_id].file_addr, "%.*s%s", dir_len, client_set[client_id].file_dir, name);
	//strcpy(client_set[client_id].file_addr, client_set[client_id].file_dir);
	//strcat(client_set[client_id].file_addr, name);
	printf("%s\n", client_set[client_id].file_addr);
}


/* REST cmd */
void rest_cmd(char* resv_message,int i) {
	sscanf(resv_message, "%*s %ld", &client_set[i].rest_pos);
	char msg[100];
	sprintf(msg, "%d %s%ld%s\r\n", 350, "Restarting at ", client_set[i].rest_pos, " . Send STORE or RETRIEVE to initiate transfer.");
	send(client_fds[i], msg, strlen(msg), 0);
}