#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "native_client/src/trusted/dyn_ldr/dyn_ldr_sharedstate.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"
#include "native_client/src/trusted/service_runtime/sel_rt.h"
#define EXIT_FROM_MAIN 0
#define EXIT_FROM_CALL 1

typedef int32_t (*IntPtrType) (uintptr_t);
typedef int32_t (*IntU32RegType)(uint32_t, nacl_reg_t);
typedef int32_t (*IntU32Type)(uint32_t);

void MakeNaClSysCall_register_shared_state(uintptr_t sharedState)
{
	((IntPtrType)NACL_SYSCALL_ADDR(NACL_sys_register_shared_state))(sharedState);
}

void MakeNaClSysCall_exit_sandbox(uint32_t exitLocation, nacl_reg_t eax)
{
	((IntU32RegType)NACL_SYSCALL_ADDR(NACL_sys_exit_sandbox))(exitLocation, eax);
}

//Specifically not making this a new function as this may add a new stack frame
#define MakeNaClSysCall_callback(slotNumber) ((IntU32Type)NACL_SYSCALL_ADDR(NACL_sys_callback))(slotNumber)

void exitFunctionWrapper(void)
{
	// Note : On 32 bit platforms Windows, Linux/Unix NaCl always builds ELF32 following the cdecl  (not sure if other calling conventions are supported in NaCl 86. PNaCl only supports x86) 
	//        On 64 bit platforms Windows, Linux/Unix NaCl always builds ELF64 and the System V AMD64 ABI calling convention
	// This function needs to read return values from different registers based on the calling convension of the architecture
	// In 32 bit 
	// 		If the sandboxed function being called has the cdecl calling convention (not sure if other calling conventions are supported in NaCl 86. PNaCl only supports x86)
	//			- If the return value is 32 bits or smaller and is not a float or double or struct, the return value is stored in the eax register
	//			- If the return value is > 32 bits and <= 64 bits and is not a float or double or struct, the return value is split into the eax (lower bits) and edx(higher bits) register
	//			- If the return value is a struct, it is stored in space created the caller in the callers stack frame, the pointer to the space is the first arg to the callee, the returned value is the pointer to the free space and is returned to eax
	//			- If the return value is a float or double, the returned value is in x87 ST0 register
	//		If the sandboxed function being called has the C++ ABI, since the 2 supported NaCl compilers are gcc and clang, these use basically the same ABI (independent of platform - windows/linux/unix)
	//			- This is very similar to the C ABI, with classes treated as structs
	// In 64 bit
	// 		The sandboxed function being called has to have the System V AMD64 ABI calling convention
	//			- If the return value is 64 bits or smaller and is not a float or double, the return value is stored in the rax register
	//			- If the return value is > 64 bits and <= 128 bits and is not a float or double, the return value is split into the rax (lower bits) and edx(higher bits) register
	//			- If the return value is a struct > 128 bits, it is stored in space created the caller in the callers stack frame, the pointer to the space is the first arg to the callee, the returned value is the pointer to the free space and is returned to rax
	//			- If the return value is a float or double, the returned value is in XMM0 register
	//			- If the return value is a float256 bit and float512 bit, the returned value is in YMM0 and ZMM0
	//		If the sandboxed function being called has the C++ ABI, since the 2 supported NaCl compilers are gcc and clang, these use basically the same ABI (independent of platform - windows/linux/unix)
	//			- This is very similar to the C ABI, with classes treated as structs, except for classes < 128 bits with non trivial constructors or destructors - these go on the stack

	// Building a very small part of the full functionality here. For now we will just support the most common case
	// Return eax/rax as most api's just want to return raw integer values or pointers

	#if defined(_M_IX86) || defined(__i386__)
		register nacl_reg_t return_reg asm("eax");
	#elif defined(_M_X64) || defined(__x86_64__)
		register nacl_reg_t return_reg asm("rax");		
	#elif defined(__ARMEL__) || defined(__MIPSEL__)
		#error Unsupported platform!
	#else 
		#error Unknown platform!
	#endif

	nacl_reg_t register_return_reg = return_reg;
	MakeNaClSysCall_exit_sandbox(EXIT_FROM_CALL, register_return_reg);
}

void callbackFunctionWrapper0(void) { MakeNaClSysCall_callback(0); }
void callbackFunctionWrapper1(void) { MakeNaClSysCall_callback(1); }
void callbackFunctionWrapper2(void) { MakeNaClSysCall_callback(2); }
void callbackFunctionWrapper3(void) { MakeNaClSysCall_callback(3); }
void callbackFunctionWrapper4(void) { MakeNaClSysCall_callback(4); }
void callbackFunctionWrapper5(void) { MakeNaClSysCall_callback(5); }
void callbackFunctionWrapper6(void) { MakeNaClSysCall_callback(6); }
void callbackFunctionWrapper7(void) { MakeNaClSysCall_callback(7); }

unsigned test_localMath(unsigned a, unsigned  b, unsigned c)
{
	unsigned ret;
	ret = (a*100) + (b * 10) + (c);
	return ret;
}

size_t test_localString(char* test)
{
	return strlen(test);
}

int threadMain(void)
{
	MakeNaClSysCall_exit_sandbox(EXIT_FROM_MAIN, 0 /* eax - this is unused and is used only in EXIT_FROM_CALL */);
	return 0;
}

int main(int argc, char** argv)
{
	struct AppSharedState* appSharedState = (struct AppSharedState*) malloc(sizeof(struct AppSharedState));

	appSharedState->threadMainPtr = threadMain;
	appSharedState->exitFunctionWrapperPtr = exitFunctionWrapper;
	appSharedState->callbackFunctionWrapper[0] = callbackFunctionWrapper0;
	appSharedState->callbackFunctionWrapper[1] = callbackFunctionWrapper1;
	appSharedState->callbackFunctionWrapper[2] = callbackFunctionWrapper2;
	appSharedState->callbackFunctionWrapper[3] = callbackFunctionWrapper3;
	appSharedState->callbackFunctionWrapper[4] = callbackFunctionWrapper4;
	appSharedState->callbackFunctionWrapper[5] = callbackFunctionWrapper5;
	appSharedState->callbackFunctionWrapper[6] = callbackFunctionWrapper6;
	appSharedState->callbackFunctionWrapper[7] = callbackFunctionWrapper7;

	appSharedState->test_localMathPtr = test_localMath;
	appSharedState->test_localStringPtr = test_localString;

	appSharedState->mallocPtr = malloc;
	appSharedState->freePtr = free;

	appSharedState->dlopenPtr = dlopen;
	appSharedState->dlerrorPtr = dlerror;
	appSharedState->dlsymPtr = dlsym;
	appSharedState->dlclosePtr = dlclose;

	appSharedState->fopenPtr = fopen;
	appSharedState->fclosePtr = fclose;

	appSharedState->checkChar = 1234;

	MakeNaClSysCall_register_shared_state((uintptr_t)appSharedState);
	MakeNaClSysCall_exit_sandbox(EXIT_FROM_MAIN, 0 /* eax - this is unused and is used only in EXIT_FROM_CALL */);
	return 0;
}
