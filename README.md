# Sandboxing_NaCl
A modification of NaCl to support sandoxing of dynamic libraries from the main app

# First time setup instructions

The following commands should be run the first time only

First install the depot tools software as per http://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html#_setting_up

Run the following commands
```
# Install some required libs for 32 bit libs
sudo native_client/tools/linux.x86_64.prep.sh
# Get the default gcc and clang toolchains
gclient runhooks
```

Get the clang sources
```
../tools/clang/scripts/update.py
toolchain_build/toolchain_build_pnacl.py --verbose --sync --clobber --install toolchain/linux_x86/pnacl_newlib_raw
rm -rf toolchain/linux_x86/pnacl_newlib
ln -s pnacl_newlib_raw toolchain/linux_x86/pnacl_newlib
```


# Build instructions

Note build commands should be run in the 'native_client' folder.

For build of the 32 bit native client library run 
```
./scons MODE=opt-linux,nacl werror=0
# Debug build
# ./scons MODE=dbg-linux,nacl werror=0
```

For build of the 64 bit native client library run
```
./scons MODE=opt-linux,nacl werror=0 platform=x86-64
# Debug build
# ./scons MODE=dbg-linux,nacl werror=0 platform=x86-64
```

For detailed logs add the '--verbose' flag.

After building, to run the test app use 
```
scons-out/opt-linux-x86-32/staging/dyn_ldr_test
# Debug build
scons-out/dbg-linux-x86-32/staging/dyn_ldr_test
```

To build a version compatible with Firefox (which does not use Position independent executables you can use the following commands)

```
./scons -f SConstruct_Firefox werror=0 MODE=opt-linux,nacl
# Debug build
# ./scons -f SConstruct_Firefox werror=0 MODE=dbg-linux,nacl
```

A helpful script to put the firefox build in a new folder
```
mv scons-out scons-out-clean
mv scons-out-firefox scons-out
./scons -f SConstruct_Firefox werror=0 MODE=opt-linux,nacl
mv scons-out scons-out-firefox
mv scons-out-clean scons-out
```

Perf benchmark is built and run separately
To build the perf benchmark run
native_client/src/trusted/dyn_ldr/benchmark/buildBenchmark
To run use
native_client/src/trusted/dyn_ldr/benchmark/dyn_ldr_benchmark

To build the customized NACL compiler tools use. You will most likely see errors during this process. See below on how to resolve this
```
cd ./tools
make -j8 build-with-glibc
```

1) If you see an error that looks like 'These critical programs are missing or too old: make', update file './tools/SRC/glibc/configure' line 4740 from 
'3.79* | 3.[89]*)' to '3.79* | 3.[89]*|4.*)'

2) If you see an error 'Makefile:235: *** mixed implicit and normal rules: deprecated syntax' change the block
```
$(objpfx)stubs ../po/manual.pot $(objpfx)stamp%:
	$(make-target-directory)
	touch $@
```
to
```
$(objpfx)stubs ../po/manual.pot $(objpfx)stamp%:
	$(make-target-directory)
	touch $@
../po/manual.pot:
	$(make-target-directory)
	touch $@
$(objpfx)stamp%:
	$(make-target-directory)
	touch $@
```

(Note - instructions for how to fetch a fresh copy of NaCl code are provided in MyBuildInstr.txt. This is not necessary unless you want to rebase this project on the latest NaCl)

# Usage
Example code, which also serves as a quick test is located in native_client/src/trusted/dyn_ldr/testing/dyn_ldr_test.c.
Here is an example of using libjpeg, in a sandboxed manner - https://github.com/shravanrn/libjpeg-turbo_nacltests
Here is an example of using libjpeg, inside firefox in a sandboxed manner - https://github.com/shravanrn/mozilla_firefox_nacl.git
Some related Code - A modification of NASM to assembly NACL complicant assembly - https://github.com/shravanrn/NASM_NaCl/blob/master/README