# NisaBuzzer
NisaBuzzer is a header based library that uses libasio to communicate with the buzzers build for the NiSa Con - a anime con in germany.

### compile
Compiling should be easy. NisaBuzzer brings asio with it (see include/asio*version*) (you find the asio libs licence there, too). 
Alternatively, if you want to use the asio lib on your system, define `NISABUZZER_DIFFERENT_ASIO_LOCATION` right before you include the `NisaBuzzer.hpp`

To compile, use your favorite c++11 compiler, include NisaBuzzer in your project and have fun. Documentation is In-Code.

### example
There is the file src/example.cpp. In it is a verry basic example that shows how to use the lib and also tests some things (it works on my system so it totally must be ok...).

### copying
see LICENSE.txt