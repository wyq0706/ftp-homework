void merge_response(char* buf, int code, char* response);
char *ip_search(void);
void merge_response_PASV(char*buf, int code, char* ip, int port_num);
void initialize_all_clientinfo();
int get_random_port_number();
int parse_ip_and_port(char* msg, int client_id);