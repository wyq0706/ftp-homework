
void* get_and_write_file(void* arg);
void* read_file_to_send(void* arg);
void get_file_addr(char* msg, int client_id);
int establish_connection(int i);
void parse_filename(char* msg, int client_id);
void rest_cmd(char* resv_massage, int i);