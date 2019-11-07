#ifndef _HDRNAME_H   //_HDRNAME_H按头文件的文件名取名，防止同名冲突

#define _HDRNAME_H

#include<stdio.h>
#include<stdlib.h>
#include <netinet/in.h>   /* for sockaddr_in */
// #include <sys/types.h>     
#include <sys/socket.h>

#include <sys/time.h>
#include <unistd.h>
#include<pthread.h>
#include <sys/select.h>

/* response to client command */
#define CONNECTOK_INFO "ftp.ssast.org FTP server ready."
#define LOGINOK_INFO "Guest login ok, send your complete e-mail address as password."
#define WELCOME_INFO "Verified. Welcome!"
#define PASVMODE_INFO "Entering PassiveMode"
#define PORTMODE_INFO "PORT command successful."

/* util function */
#define ERRORIP "cannot find host ip"


# define PATHNAME_LEN 100
#define FILEDIR_LEN 100
# define TRANSFER_LEN 4096
/* use state to verify for certain commands */
enum client_state {
	INITIAL,
	USER,
	PASS,
	/* PASV and PORT turn to PASS after a file transfer */
	PASV,
	PORT
};

struct client_info {
	/* save some information about the client*/
	int state;
	/* for connection socket in PORT mode */
	struct sockaddr_in connfd_client_addr;
	/* record port (USED in PASV mode) */
	int port;
	//char ip[20];
	/* save port number of server connection socket in PASV mode;
	save default port number(listen socket port+1) in PORT mode */
	struct sockaddr_in conn_addr;
	/* file-descriptors of connection socket */
	int connfd_server;
	int connfd_client;
	/* file address */
	char file_addr[100];
	/* buf for file */
	unsigned char file_buf[TRANSFER_LEN];
	/* if rnfr; 0 means not, 1 means yes */
	int rnfr; 
	/* record pathname written in RNFR cmd*/
	char pathname[PATHNAME_LEN];
	/* default file directory on server */
	char file_dir[FILEDIR_LEN];
	/* rest position */
	long rest_pos;
	/* if rest: 0 for not,1 for yes*/
	int ifrest;
	int isTransfering;
	pthread_t thread_id;
	long lastPos;
};

#define CLI_NUM 3  // maximum number of clients

/* for recording info */
struct client_info client_set[CLI_NUM];

/* for select */
int client_fds[CLI_NUM];
fd_set ser_fdset;

char file_dir[100];

typedef struct
{
	int socket;
	int id;
}thread_arg;

#endif
