lib: msocket.o
	ar rcs libfoothread.a msocket.o

msocket.o: msocket.c msocket.h
	gcc -fPIC -Wall -c -I. msocket.c

init: 
	gcc -o init initmsocket.c -L. -lfoothread -lpthread
	./init

clean:
	rm -f *.o *.a