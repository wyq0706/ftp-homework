void do_USER(char* msg, int i);
void do_PASS(char* msg, int i);
void do_QUIT(char* msg, int i);
void do_PASV(char* msg, int i);
void do_PORT(char* msg, int i);
void do_RETR(char* msg, int i);
void do_STOR(char* msg, int i);
void do_SYST(char* msg, int i);
void do_TYPE(char* msg, int i);
void do_RNFR(char* msg, int i);
void do_RNTO(char* msg, int i);
void do_MKD(char* msg, int i);
void do_RMD(char* msg, int i);
void do_PWD(char* msg, int i);
void do_CWD(char* msg, int i);
void do_LIST(char* msg, int i);
void do_REST(char* msg, int i);
void do_ABOR(char* msg, int i);

void parse_command_and_do(char* command, int i);