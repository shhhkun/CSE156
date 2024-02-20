fill later
# Programming Lab 3: Simple Reliable File Transfer
Serjo Barron
sepbarro

Winter 2024

## [files/directories]
 - bin (will hold binaries/executables when "make" is called)
 - doc (holds lab documentation)
    - lab3.pdf (lab documentation)
 - src (holds source code)
    - myserver.c (server source code)
    - myclient.c (client source code)
 - Makefile (for compiling)
 - README.md (brief description of program files/directories; this file)

## [description]
This project comprises of two programs, a server and client. The server takes 2 command line arguements (CLI) a port number to bind the socket and open a connection as a means of server and client communication. The server also takes a drop percentage (droppc) which dictates the amount of incoming packets the server recieved that should be dropped. The client takes 7 CLI which are the server IP, server port number, MTU, window size, infile path, and outfile path. The client will use the server IP and port number to create a socket to connect to the server, and will send the infile path in packets up to size MTU to the server within a specified window size. The server will receive the packets and reconstruct the file specified by the infile path by writing the received bytes to the outfile path.

However, since the server has the ability to drop packets due to droppc, if it is enabled (i.e. is not 0) it will have a chance to drop an incoming packet. If the packet is dropped, the client will know via a timeout and will retransmit under such cases. The server decides to drop the packet or not by randomness by using functions srand() using time as a seed and rand(). The chance is then calculated via rand() % 100 and if it happens to be less than droppc, the packet will be dropped, otherwise it will not. Both server and client also have log messages that are printed to stdout/stderr to be able to visualize packet losses and successful packet transmissions.