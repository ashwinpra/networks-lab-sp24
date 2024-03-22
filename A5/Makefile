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

user3: lib user3.c
	gcc -Wall -I. -L. -o user3 user3.c -lmsocket 
	./user3

user4: lib user4.c
	gcc -Wall -I. -L. -o user4 user4.c -lmsocket 
	./user4

clean:
	rm -f *.o *.a user1 user2 init