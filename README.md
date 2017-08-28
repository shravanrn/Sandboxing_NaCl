# Sandboxing_NaCl
A modification of NaCl to support sandoxing of dynamic libraries from the main app

# Build instructions
For first build from the native client library run './scons --nacl_glibc'. For incremental builds that skip some tests use './scons MODE=opt-linux,nacl --nacl_glibc --verbose'. The 32-bit Build has been tested on an Ubuntu 17.04 64 bit.

To build a version compatible with Firefox

mv scons-out scons-out-clean
mv scons-out-firefox scons-out
./scons -f SConstruct_Firefox --nacl_glibc MODE=opt-linux,nacl #DESTINATION_ROOT=scons-out-firefox
mv scons-out scons-out-firefox
mv scons-out-clean scons-out

(Note - instructions for how to fetch a fresh copy are provided in MyBuildInstr.txt. This is not necessary unless, say you want to rebase this project on the latest NaCl)

# Usage
Example located in native_client/src/trusted/dyn_ldr/testing/dyn_ldr_test.c and https://github.com/shravanrn/libjpeg-turbo_nacltests
