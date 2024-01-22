# Programming Lab 1: myweb.c
Serjo Barron
sepbarro

Winter 2024

## [files/directories]
 - bin (will hold binary/executable when "make" is called)
 - doc (holds lab documentation)
    - lab1.pdf (lab documentation)
 - src (holds source code)
    - myweb.c (source code)
 - Makefile (for compiling)
 - README.md (brief description of program files/directories; this file)

## [description]
This is a simple program that takes in 2 to 4 command line arguments (CLI) which are a hostname, server address, an optional port number, and a optional "-h" flag. This program will effectively replicate the functionality of wget and curl which are both command-line tools for downloading.

The program executes HEAD or GET requests, and will do GET by default or HEAD when the "-h" flag is specified. The contents of the file during a GET request is outputted to file called "output.dat" in the top directory, and during a HEAD request the header fields will simply be printed to stdout, nothing gets written.