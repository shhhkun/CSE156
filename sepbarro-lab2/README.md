# Programming Lab 2: Simple File Echo
Serjo Barron
sepbarro

Winter 2024

## [files/directories]
 - bin (will hold binaries/executables when "make" is called)
 - doc (holds lab documentation)
    - lab2.pdf (lab documentation)
 - src (holds source code)
    - myserver.c (server source code)
    - myclient.c (client source code)
 - Makefile (for compiling)
 - README.md (brief description of program files/directories; this file)

## [description]
This project comprises of two major components (programs), a server and client. The server takes 1 command line argument (CLI) a port number, which it uses to bind its socket to and open a connection as a means of server and client communication. The client takes 6 CLI which are the server IP, server port number, MTU, infile path, and outfile path. The client will use the server IP and port number to create a socket to connect to the server, and will send the infile path in packets of up to size MTU to the server. The server will then receive these packets and echo (send) it back to the client, where it will reconstruct the original file (infile) by writing the received bytes to the outfile path.

During the server echo to client process, the client has a timeout to detect packet loss. In the situation of packet loss, it is assumed that bytes were lost. If nothing is printed to stdout after running the client program, the file reconstruction was successful which can be double checked by using the diff command between the infile and outfile paths.