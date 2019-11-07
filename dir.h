#include<stdio.h>
#include <sys/stat.h>

/* functions that manipulate directories on server */

/* default file directory on server */


int get_pathname(char* resv_message, int client_id);
int rename_pathname(char* resv_message, int client_id);
void get_cur_dir(char* input_message, int client_id);
void change_filedir(char* msg,int client_id);
int make_dir(char* msg,int i);
int remove_dir(char* msg, int i);
int get_list(int sd,int i,char* msg);
void getperms(struct stat* sbuf, char* perms);
void wrtie_file_info(char* buf, struct stat* sbuf, char* tmppath);