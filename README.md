# Sandboxing_NaCl
A modification of NaCl to support sandoxing of dynamic libraries from the main app

# Build instructions
For first build from the native client library run './scons --nacl_glibc'. For incremental builds that skip some tests use './scons MODE=opt-linux,nacl --nacl_glibc --verbose'. The 32-bit Build has been tested on an Ubuntu 17.04 64 bit.

To run the test app use "scons-out/opt-linux-x86-32/staging/dyn_ldr_test"

To build a version compatible with Firefox (which does not use Position independent executables you can use the following commands)

```
./scons -f SConstruct_Firefox --nacl_glibc MODE=opt-linux,nacl
```

A helpful script to put the firefox build in a new folder
```
mv scons-out scons-out-clean
mv scons-out-firefox scons-out
./scons -f SConstruct_Firefox --nacl_glibc MODE=opt-linux,nacl
mv scons-out scons-out-firefox
mv scons-out-clean scons-out
```

(Note - instructions for how to fetch a fresh copy of NaCl code are provided in MyBuildInstr.txt. This is not necessary unless you want to rebase this project on the latest NaCl)

# Usage
Example code, which also serves as a quick test is located in native_client/src/trusted/dyn_ldr/testing/dyn_ldr_test.c.
Here is an example of using libjpeg, in a sandboxed manner - https://github.com/shravanrn/libjpeg-turbo_nacltests
Here is an example of using libjpeg, inside firefox in a sandboxed manner - https://github.com/shravanrn/mozilla_firefox_nacl.git
