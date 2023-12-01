This project relies on the following packages:
    - clang
    - llvm-14
    - make
These packages may be installed with the following command:
    sudo apt install -y clang llvm-14 make

Once these packages are installed, the passes for this project can be run with the 'make' command using the makefile.
Usage of the pass is as follows: 
    clang -fpass-plugin="/path/to/pass.so" input.c -o output
where "/path/to/pass.so" is the path to the pass, input.c is the name of the input file, and output is the name of the output file.

Run "make test-cases" to run the passes in this project on the c source files in /test-cases/
