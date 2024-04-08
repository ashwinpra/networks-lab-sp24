                                                Networks Lab Assignment 6 
                                        Implementing a Custom Protocol using Raw Sockets

                                                Sarika Bishnoi - 21CS10058 
                                                Ashwin Prasanth - 21CS30009

----------------------------------------------------------------------------------------------------------------------------
## Instructions to run: 
1. A makefile has been provided to compile the code. There are 2 variables to be set in the makefile:
    - `MAC`: The MAC address of the interface to be used. This can be found using the command `ifconfig`.
    - `INTERFACE`: The name of the interface to be used. This can be also found using the command `ifconfig`.
2. Once these variables are set, you can run the server and client using the following commands:
    - `sudo make serv` to run the server
    - `sudo make cli` to run the client
  Note that `sudo` is required to run the code as raw sockets require root privileges.
3. The client takes queries sequentially. In case of timeout, it will resend the query and move on to ask the next query.
4. The server will keep running until it is manually stopped.
----------------------------------------------------------------------------------------------------------------------------


