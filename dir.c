#include<stdio.h>
#include<sys/io.h>
#include <unistd.h>
#include <fcntl.h>
#include<string.h>
#include<stdlib.h>
#include <sys/stat.h> 
#include <sys/socket.h>

#include"globals.h"


#include<pwd.h>     // 所有者信息
#include<grp.h>     // 所属组信息
#include<time.h>
//#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
//#include <sys/stat.h>
//#include <pwd.h>
//#include <grp.h>
//#include <unistd.h>
//#include <string.h>

#define LS_NONE 0
#define LS_L 101
#define LS_R 102
#define LS_D 103
#define LS_I 104

#define LS_A 200

#define LS_AL (LS_A+LS_L)
#define LS_AI (LS_A+LS_I)

extern char file_dir[100];


/* Usage: write filename in client_info && set rnfr 1 && and check if file exists
   Return: -1 for error ; 0 for ok */
int get_pathname(char* resv_message, int client_id) {
	memset(client_set[client_id].pathname, 0, PATHNAME_LEN);
	char cmd[10];
	char tmp_name[100];
	memset(tmp_name, 0, 100);
	sscanf(resv_message, "%s %s", cmd, tmp_name);
	if (tmp_name[0] != '/') {
		/* relative to absolute */
		strcpy(client_set[client_id].pathname, client_set[client_id].file_dir);
		sscanf(resv_message, "%s %s", cmd, tmp_name);
		strcat(client_set[client_id].pathname, tmp_name);
	}
	else {
		strcpy(client_set[client_id].pathname, tmp_name);
	}
	/* if file exists */
	if ((access(client_set[client_id].pathname, 0)) != -1) {
		client_set[client_id].rnfr = 1;
		return 0;
	}
	else {
		client_set[client_id].rnfr = 0;
		return -1;
	}
}

/* Usage: rename the file
   Return: -1 for error; 0 for ok */
int rename_pathname(char* resv_message, int client_id) {
	char cmd[10];
	char newname[50];
	sscanf(resv_message, "%s %s", cmd, newname);
	if (rename(client_set[client_id].pathname, newname) == 0) {
		printf("Have renamed %s as %s.\n", client_set[client_id].pathname, newname);
	}
	else {
		printf("fail to rename\n");
		return -1;
	}
	printf("okrename\n");
	return 0;
}


/* Usage: write current path into input message */
int get_cur_dir(char* input_message, int client_id) {
	char in_msg[100];
	memset(in_msg, 0, strlen(in_msg));
	strncpy(in_msg, client_set[client_id].file_dir, strlen(client_set[client_id].file_dir));
	in_msg[strlen(client_set[client_id].file_dir) - 1] = '\0';
	//in_msg[strlen(client_set[client_id].file_dir) - 1] = '\n';
	memset(input_message, 0, strlen(input_message));
	sprintf(input_message, "%d \"%s\" is the current directory.\r\n", 257, in_msg);
	return 0;
	//merge_response(input_message, 200, in_msg);
}

/* Usage: change default file dirctoey on server (CWD cmd) */
void change_filedir(char* msg, int client_id) {
	memset(client_set[client_id].file_dir, 0, strlen(client_set[client_id].file_dir));
	char cmd[10];
	sscanf(msg, "%s %s", cmd, client_set[client_id].file_dir);
	strcat(client_set[client_id].file_dir, "/");
	printf("%s", client_set[client_id].file_dir);
}

/* Usage: MKD cmd
   Return: 0 for ok; 1 for error */
int make_dir(char* msg, int i) {
	char cmd[10];
	char dir[100];
	memset(dir, 0, 100);
	sscanf(msg, "%s %s", cmd, dir);
	if (dir[0] == '/') {
		/* absolute path */
		if (mkdir(dir, 0777) == 0) {
			printf("yesmake\n");
			return 0;
		}
		else {
			return -1;
		}
	}
	else {
		/* relative path */
		char dir_head[100];
		memset(dir_head, 0, 100);
		strcpy(dir_head, client_set[i].file_dir);
		strcat(dir_head, dir);
		printf("%s\n", dir_head);
		if (mkdir(dir_head, 0777) == 0) {
			printf("yesmake\n");
			return 0;
		}
		else {
			return -1;
		}

	}
}

/* Usage: RMD cmd
Return: 0 for ok; 1 for error */
int remove_dir(char* msg, int i) {
	char cmd[10];
	char dir[100];
	memset(dir, 0, 100);
	sscanf(msg, "%s %s", cmd, dir);
	if (dir[0] == '/') {
		/* absolute path */
		if (rmdir(dir) == 0) {
			printf("yesmake\n");
			return 0;
		}
		else {
			return -1;
		}
	}
	else {
		/* relative path */
		char dir_head[100];
		memset(dir_head, 0, 100);
		strcpy(dir_head, client_set[i].file_dir);
		strcat(dir_head, dir);
		printf("%s\n", dir_head);
		if (rmdir(dir_head) == 0) {
			printf("yesmake\n");
			return 0;
		}
		else {
			return -1;
		}

	}
}


/* Usage: get perms about files */
void getperms(struct stat* sbuf, char* perms) {
	// 判断文件类型
	switch (sbuf->st_mode & S_IFMT)
	{
	case S_IFSOCK:   // 套接字文件
		perms[0] = 's';
		break;
	case S_IFLNK:	 // 软连接文件
		perms[0] = 'l';
		break;
	case S_IFREG:	 // 普通文件
		perms[0] = '-';
		break;
	case S_IFBLK:    // 块设备文件
		perms[0] = 'b';
		break;
	case S_IFDIR:    // 目录文件

		perms[0] = 'd';
		break;
	case S_IFCHR:    // 字符设备文件

		perms[0] = 'c';
		break;
	case S_IFIFO:    // 管道文件

		perms[0] = 'p';
		break;
	default:
		break;

	}

	// 判断文件的访问权限
	// 文件的所有者
	perms[1] = (sbuf->st_mode & S_IRUSR) ? 'r' : '-';
	perms[2] = (sbuf->st_mode & S_IWUSR) ? 'w' : '-';
	perms[3] = (sbuf->st_mode & S_IXUSR) ? 'x' : '-';

	// 文件的所属组
	perms[4] = (sbuf->st_mode & S_IRGRP) ? 'r' : '-';
	perms[5] = (sbuf->st_mode & S_IWGRP) ? 'w' : '-';
	perms[6] = (sbuf->st_mode & S_IXGRP) ? 'x' : '-';

	// 文件的其他用户

	perms[7] = (sbuf->st_mode & S_IROTH) ? 'r' : '-';
	perms[8] = (sbuf->st_mode & S_IWOTH) ? 'w' : '-';
	perms[9] = (sbuf->st_mode & S_IXOTH) ? 'x' : '-';
}


/* write file info for LIST cmd */
void wrtie_file_info(char* buf, struct stat* sbuf, char* tmppath) {
	//if (detail){

	char perms[11] = { 0 };
	int off = 0;
	getperms(sbuf, perms);

	off = 0;
	off += sprintf(buf, "%s ", perms);
	off += sprintf(buf + off, " %3ld %-8s %-8s ", sbuf->st_nlink, getpwuid(sbuf->st_uid)->pw_name, getgrgid(sbuf->st_gid)->gr_name);
	off += sprintf(buf + off, "%8lu ", (unsigned long)sbuf->st_size);
	// 文件修改时间

	char *time = ctime(&sbuf->st_mtime);
	char mtime[512] = "";
	strncpy(mtime, time, strlen(time) - 9);

	int i = 0;
	int k = 0;
	for (i = 0; tmppath[i] != '\0'&&tmppath[i] != '\r'&&tmppath[i] != '\n'; ++i) {
		if (tmppath[i] == '/') {
			k = i;
		}
	}
	if (k > 0) {
		k = k + 1;
	}

	off += sprintf(buf + off, "%s ", &mtime[3]);
	if (S_ISLNK(sbuf->st_mode)) {
		char tmp[1024] = { 0 };
		readlink(&tmppath[k], tmp, sizeof(tmp));
		off += sprintf(buf + off, "%s -> %s\r\n", &tmppath[k], tmp);
	}
	else {
		off += sprintf(buf + off, "%s\r\n", &tmppath[k]);
	}
}

/* LIST cmd
   Reeturn: 0 for ok; -1 for error */
int get_list(int sd, int i, char* msg) {
	char cmd[10];
	char path[100];
	int flag = sscanf(msg, "%s %s", cmd, path);
	printf("flags:%d", flag);
	char buf[1024] = { 0 };
	DIR* dirptr;
	char tmppath[100];
	memset(tmppath, 0, 100);
	if (flag == 1) {
		/* default root directory of each client */

		strncpy(tmppath, client_set[i].file_dir, strlen(client_set[i].file_dir) - 1);
	}
	else if (flag == 2) {
		if (path[0] == '/') {
			/* absolute path */
			strcpy(tmppath, path);
		}
		else {
			/* relative path */
			strcpy(tmppath, client_set[i].file_dir);
			strcat(tmppath, path);
		}
	}
	else {
		/* parse parameters failure */
		return -1;
	}
	printf("start\n");
	struct dirent *dt;
	struct stat sbuf;
	if ((dirptr = opendir(tmppath)) == NULL) {
		printf("opendir failed!\n");
		if (lstat(tmppath, &sbuf) >= 0) {
			/* is a file */
			wrtie_file_info(buf, &sbuf, tmppath);

			printf("%s", buf);

			//printf("%s", buf);
			send(sd, buf, strlen(buf), 0);
			return 0;
		}
		return -1;
	}

	char thispath[100];
	while ((dt = readdir(dirptr)) != NULL) {
		printf("inlist\n");
		memset(thispath, 0, 100);
		sprintf(thispath, "%s", tmppath);
		strcat(thispath, "/");
		strcat(thispath, dt->d_name);
		if (lstat(thispath, &sbuf) < 0) {
			continue;
		}
		if (dt->d_name[0] == '.')
			/* hidden file */
			continue;
		wrtie_file_info(buf, &sbuf, thispath);
		printf("%s", buf);

		//printf("%s", buf);
		send(sd, buf, strlen(buf), 0);
	}

	closedir(dirptr);
	return 0;
}

