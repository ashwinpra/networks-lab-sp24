run: 
	gcc mailclient.c -o mc 
	gcc smtpmail.c -o ms
	./ms 2500

test: 
	gcc testcli.c -o tc
	gcc testserv.c -o ts
	./ts 

clean: 
	rm mc ms tc ts

empty: 
	rm ashwin/mymailbox sarika/mymailbox
	touch ashwin/mymailbox sarika/mymailbox