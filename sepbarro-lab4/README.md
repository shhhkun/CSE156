# Programming Lab 4: Simple Reliable File Replication
Serjo Barron
sepbarro
1821839

Winter 2024

## [files/directories]
 - bin (will hold binaries/executables when "make" is called)
 - doc (holds lab documentation)
    - lab4.pdf (lab documentation)
 - src (holds source code)
    - myserver.c (server source code)
    - myclient.c (client source code)
 - Makefile (for compiling)
 - README.md (brief description of program files/directories; this file)

 ## [description]
 Thisproject comprises of two programs, a server and client. The server takes 3 command line arguements (CLI) a port number to bind the socket and open a connection as a means of server and client communication. The server also takes a drop percentage (droppc) which dictates the amount of incoming packets the server received that should be dropped. Lastly, the server also takes a root folder which will determine where the replicated file will be stored.

 The client takes 7 CLI which are the number of servers (to replicate to), server configuration file, MTU, window size, infile path, and outfile path. Different from the previous labs the client takes a configuration file that should contain server port and IP addresses.

 **Server Configuration File Example:**

    > 127.0.0.1 8000
    > 127.0.0.1 8001
    > 127.0.0.1 8002
    > 127.0.0.1 8003

The client will then read this file and save each line of IP address and port number pairs to know all the possible servers to replicate to. The client will use the server IP and port number to create a socket to connect to the server, and will send the infile path in packets up to size MTU to the server within a specified window size. The server will then receive the packets and reconstruct the file under the root folder in the outfile path.

However, since there is a new CLI which is the number of servers to replicate to, the client will read that number and replicate the infile as the outfile to that amount of servers. For instance, if the number of servers were 4 it would replicate to server ports 8000, 8001, 8002, and 8003 as seen in the example above. But, if the number of servers were only 1 it would only replicate to the first server on port 8000.
