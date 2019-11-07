server: server.o dir.o file_op.o cmd_op.o utils.o
	gcc -g -Wall server.o dir.o file_op.o cmd_op.o utils.o -o server -lpthread

server.o: server.c
	gcc -c -g -Wall server.c -o server.o
dir.o: dir.c
	gcc -c -g -Wall dir.c -o dir.o

file_op.o: file_op.c
	gcc -c -g -Wall file_op.c -o file_op.o -lpthread

cmd_op.o: cmd_op.c
	gcc -c -g -Wall cmd_op.c -o cmd_op.o -lpthread

utils.o:utils.c
	gcc -c -g -Wall utils.c -o utils.o

.PHONY: clean
	
clean:
	rm -rf *.o
