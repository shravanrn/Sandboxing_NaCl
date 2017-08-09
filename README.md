# Sandboxing_NaCl
A modification of NaCl to support sandoxing of dynamic libraries from the main app

# Build instructions
For first build from the native client library run './scons --nacl_glibc'. For incremental builds that skip some tests use './scons MODE=opt-linux,nacl --nacl_glibc'. The 32-bit Build has been tested on an Ubuntu 17.04 64 bit.

# Usage
Example located in native_client/src/trusted/dyn_ldr/testing/dyn_ldr_test.c
