run: 
	gcc mailclient.c -o mc 
	gcc smtpmail.c -o ms
	./ms 2500

test: 
	gcc testcli.c -o tc
	gcc testserv.c -o ts
	./ts 

clean: 
	rm mc ms ashwin/mymailbox sarika/mymailbox
	rm tc ts
	touch ashwin/mymailbox sarika/mymailbox