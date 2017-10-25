server: checkServer.o threadPool.o
	gcc  threadPool.o checkServer.o -o server -pthread

checkServer.o: checkServer.c threadPool.h
	gcc -c checkServer.c

threadPool.o: threadPool.c threadPool.h
	gcc -c threadPool.c -pthread

clean:
	rm -v checkServer.o threadPool.o

test: server
	valgrind --leak-check=full ./server

debug: checkServer.c threadPool.c
	gcc -g -o test checkServer.c threadPool.c -pthread

warn: checkServer.c threadPool.c
	gcc -Wall checkServer.c threadPool.c -o server -pthread
run:
	./server