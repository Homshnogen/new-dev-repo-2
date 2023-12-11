This project relies on the following packages:
    - clang
    - llvm-14-dev
    - make
These packages may be installed with the following command:
    sudo apt install -y clang llvm-14-dev make

Once these packages are installed, the passes for this project can be run with the 'make' command using the makefile.
Usage of the pass is as follows: 
    clang -g -fno-discard-value-names -O0 -fpass-plugin="/path/to/pass.so" -S input.c -o output.ll
    clang -g -fno-discard-value-names -O0 -fpass-plugin="/path/to/pass.so" input.c -o output
where "/path/to/pass.so" is the path to the pass, input.c is the name of the input file, output.ll is the name of the output llvm file, and output is the name of the output executable.

Run "make prof-all" to run the passes in this project on the c source files in /test-cases/
