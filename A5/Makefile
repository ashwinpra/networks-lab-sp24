lib: msocket.o
	ar rcs libmsocket.a msocket.o

msocket.o: msocket.c msocket.h
	gcc -fPIC -Wall -c -I. msocket.c

init: lib initmsocket.c
	gcc -Wall -I. -L. -o init initmsocket.c -lmsocket -pthread
	./init

clean:
	rm -f *.o *.a