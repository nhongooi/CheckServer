server: checkServer.o threadPool.o
	gcc  threadPool.o checkServer.o -o server -pthread

checkServer.o: checkServer.c threadPool.h
	gcc -c checkServer.c

threadPool.o: threadPool.c threadPool.h
	gcc -c threadPool.c -pthread

clean:
	rm -v checkServer.o threadPool.o

val: server
	valgrind --leak-check=full --tool=memcheck ./server

debug: checkServer.o threadPool.o
	gcc -g -o test checkServer.o threadPool.o -pthread

check: checkServer.o threadPool.o
	gcc -Wall checkServer.o threadPool.o -o server -pthread
run: server
	./server
