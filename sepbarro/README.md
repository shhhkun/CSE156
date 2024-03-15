# Final Project (Lab 5): C2S Proxy
Serjo Barron
sepbarro
1821839

Winter 2024

## [files/directories]
 - bin (will hold binaries/executables when "make" is called)
 - doc (holds lab documentation)
    - finalproject.pdf (lab documentation)
 - src (holds source code)
    - myproxy.c (server source code)
 - Makefile (for compiling)
 - README.md (brief description of program files/directories; this file)

 ## [description]
 This project comprises of one program, a proxy server. The server takes 3 command line inputs/arguements (CLI) a port number, a forbidden sites file, and a access log file/path. The proxy server acts as an intermediary between a client and a requested resource. This proxy server is particular supports HEAD and GET requests, and nothing else.

 The forbidden sites file is a file that contains a list of domain names or IP's that are to be considered forbidden by the proxy server.

 **Forbidden Sites File Example:**

    > www.amazon.com
    > www.google.com
    > www.youtube.com
    > 10.6.6.6

The proxy server will then read this file and load them into an array to check if any requests coming through are forbidden. In addition to this, the forbidden sites file may be updated while the proxy servers' connection is open and may be reloaded when sending a SIGINT with Ctrl + C in the terminal where the server is running. This will result in the forbidden sites array to be up to date with the current forbidden sites file.

Lastly, the proxy server will document/log any requests whether it be successful or not to the access log file. The types of response codes supported are: 200, 400, 403, 501, 502, and 504.
