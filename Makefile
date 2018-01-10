# For verbose build: make SCONS_FLAGS=--verbose [target]
CXX=clang++
CXXFLAGS=-std=c++11 -fPIC -O0 -I.
CPU = 0

.PHONY: buildopt32 buildopt64 runopt32 runopt64 builddebug32 builddebug64 rundebug32 rundebug64 buildperftest32 buildperftest64 runperftest32 runperftest64 clean init init_if_necessary

.DEFAULT_GOAL = buildopt64

init_if_necessary:
	if [ ! -d native_client/toolchain/linux_x86/pnacl_newlib_raw ]; then $(MAKE) init; fi

init:
	sudo apt install flex bison git gcc-7-arm-linux-gnueabihf gcc-arm-linux-gnueabihf
	sudo native_client/tools/linux.x86_64.prep.sh
	gclient runhooks
	#Build the modified compiler
	tools/clang/scripts/update.py
	native_client/toolchain_build/toolchain_build_pnacl.py --verbose --sync --clobber --install native_client/toolchain/linux_x86/pnacl_newlib_raw
	#Install the modified compiler
	rm -rf native_client/toolchain/linux_x86/pnacl_newlib
	ln -s pnacl_newlib_raw native_client/toolchain/linux_x86/pnacl_newlib

buildopt32 : init_if_necessary
	cd native_client && ./scons MODE=opt-linux,nacl werror=0 $(SCONS_FLAGS)
	mv ./native_client/scons-out ./native_client/scons-out-clean
	if [ -d ./native_client/scons-out-firefox ]; then mv ./native_client/scons-out-firefox ./native_client/scons-out; fi
	cd native_client && ./scons -f SConstruct_Firefox werror=0 MODE=opt-linux,nacl $(SCONS_FLAGS)
	mv ./native_client/scons-out ./native_client/scons-out-firefox
	mv ./native_client/scons-out-clean ./native_client/scons-out

buildopt64 : init_if_necessary
	cd native_client && ./scons MODE=opt-linux,nacl werror=0 platform=x86-64 $(SCONS_FLAGS)
	mv ./native_client/scons-out ./native_client/scons-out-clean
	if [ -d ./native_client/scons-out-firefox ]; then mv ./native_client/scons-out-firefox ./native_client/scons-out; fi
	cd native_client && ./scons -f SConstruct_Firefox werror=0 MODE=opt-linux,nacl platform=x86-64 $(SCONS_FLAGS)
	mv ./native_client/scons-out ./native_client/scons-out-firefox
	mv ./native_client/scons-out-clean ./native_client/scons-out

builddebug32 : init_if_necessary
	cd native_client && ./scons MODE=dbg-linux,nacl werror=0 $(SCONS_FLAGS)

builddebug64 : init_if_necessary
	cd native_client && ./scons MODE=dbg-linux,nacl werror=0 platform=x86-64 $(SCONS_FLAGS)

runopt32 : buildopt32
	./native_client/scons-out/opt-linux-x86-32/staging/dyn_ldr_test

runopt64 : buildopt64
	./native_client/scons-out/opt-linux-x86-64/staging/dyn_ldr_test

rundebug32 : builddebug32
	./native_client/scons-out/dbg-linux-x86-32/staging/dyn_ldr_test

rundebug64 : builddebug64
	./native_client/scons-out/dbg-linux-x86-64/staging/dyn_ldr_test

buildperftest32 : native_client/src/trusted/dyn_ldr/benchmark/dyn_ldr_benchmark.cpp
	$(CXX) $(CXXFLAGS) -m32 -o native_client/src/trusted/dyn_ldr/benchmark/dyn_ldr_benchmark_32.o -c $<
	$(CXX) -std=c++11 -fPIC -O0 -o native_client/src/trusted/dyn_ldr/benchmark/dyn_ldr_benchmark_32 -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack -m32 -Wl,-rpath="./native_client/scons-out/opt-linux-x86-32/lib" native_client/src/trusted/dyn_ldr/benchmark/dyn_ldr_benchmark_32.o -L"./native_client/scons-out/opt-linux-x86-32/lib" -ldyn_ldr -lsel -lnacl_error_code -lenv_cleanser -lnrd_xfer -lnacl_perf_counter -lnacl_base -limc -lnacl_fault_inject -lnacl_interval -lplatform_qual_lib -lvalidators -ldfa_validate_caller_x86_32 -lcpu_features -lvalidation_cache -lplatform -lgio -lnccopy_x86_32 -lrt -lpthread

runperftest32 : buildopt32 buildperftest32
	./native_client/src/trusted/dyn_ldr/benchmark/dyn_ldr_benchmark_32

buildperftest64 : native_client/src/trusted/dyn_ldr/benchmark/dyn_ldr_benchmark.cpp
	$(CXX) $(CXXFLAGS) -m64 -o native_client/src/trusted/dyn_ldr/benchmark/dyn_ldr_benchmark_64.o -c $<
	$(CXX) -std=c++11 -fPIC -O0 -o native_client/src/trusted/dyn_ldr/benchmark/dyn_ldr_benchmark_64 -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack -m64 -Wl,-rpath="./native_client/scons-out/opt-linux-x86-64/lib" native_client/src/trusted/dyn_ldr/benchmark/dyn_ldr_benchmark_64.o -L"./native_client/scons-out/opt-linux-x86-64/lib" -ldyn_ldr -lsel -lnacl_error_code -lenv_cleanser -lnrd_xfer -lnacl_perf_counter -lnacl_base -limc -lnacl_fault_inject -lnacl_interval -lplatform_qual_lib -lvalidators -ldfa_validate_caller_x86_64 -lcpu_features -lvalidation_cache -lplatform -lgio -lnccopy_x86_64 -lrt -lpthread

runperftest64 : buildopt64 buildperftest64
	./native_client/src/trusted/dyn_ldr/benchmark/dyn_ldr_benchmark_64

clean :
	cd native_client && ./scons -c MODE=opt-linux,nacl $(SCONS_FLAGS)
	cd native_client && ./scons -c MODE=opt-linux,nacl platform=x86-64 $(SCONS_FLAGS)
	cd native_client && ./scons -c MODE=dbg-linux,nacl $(SCONS_FLAGS)
	cd native_client && ./scons -c MODE=dbg-linux,nacl platform=x86-64 $(SCONS_FLAGS)
