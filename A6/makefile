MAC=94:57:a5:b9:61:bd
INTERFACE=enp0s25

cli: 
	gcc -o client simDNSClient.c 
	./client $(MAC) $(INTERFACE)

serv:
	gcc -o server simDNSServer.c
	./server $(INTERFACE)

clean: 
	rm -f client server
