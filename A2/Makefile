run: file_server.c file_client.c
	gcc file_server.c -o fs
	gcc file_client.c -o fc
	./fs

test: test_server.c test_client.c
	gcc test_server.c -o ts
	gcc test_client.c -o tc
	./ts

clean: 
	rm -rf fs fc ts tc 127* *.enc