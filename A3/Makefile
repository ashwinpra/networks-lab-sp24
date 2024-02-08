run: 
	gcc mailclient.c -o mc 
	gcc smtpmail.c -o ms
	gcc popserver.c -o ps

test: 
	gcc testcli.c -o tc
	gcc testserv.c -o ts
	./ts 

clean: 
	rm mc ms tc ts

empty: 
	rm ashwin/mymailbox sarika/mymailbox
	touch ashwin/mymailbox sarika/mymailbox