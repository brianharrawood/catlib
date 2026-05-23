FileIO Example
===============================

This is a simple example program to test and excercise the FileIO
class of the catlib library. It is an example of how to use the class
for doing file IO.

Build and Run
-------------

Like other code in the CTkit, this project is designed for a Linux
system.

The project links to the catlib library in it's BUILD folder,
therefore that must be built before this example program can be built.

This project also depends on libz, libpthread, and libcrypto. 

To assure you have the dependencies installed:

    % sudo apt update -y
    % sudo apt install libz libpthread openssl -y

To build, start in the project root:

    % mkdir BUILD; cd BUILD
    % CMake ../
    % make

To run and test, start in the project root:

    % cd run
    % ./FIOtest



Testing
-------

The test program will open text and binary files both compressed and
uncompressed provided in the run/input directory and write output in
the run/output directory. Any errors encountered are reported.