lib: msocket.o
	ar rcs libmsocket.a msocket.o

msocket.o: msocket.c msocket.h
	gcc -fPIC -Wall -c -I. msocket.c

init: lib initmsocket.c
	gcc -Wall -I. -L. -o init initmsocket.c -lmsocket -pthread
	./init

user1: lib user1.c
	gcc -Wall -I. -L. -o user1 user1.c -lmsocket 
	./user1

user2: lib user2.c
	gcc -Wall -I. -L. -o user2 user2.c -lmsocket 
	./user2

clean:
	rm -f *.o *.a