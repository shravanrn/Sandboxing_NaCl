#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "native_client/src/trusted/dyn_ldr/dyn_ldr_sharedstate.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"

typedef int32_t (*IntPtrType) (uintptr_t);
typedef int32_t (*IntVoidType)(void);
typedef int32_t (*IntUnsignedType)(unsigned);

struct AppSharedState* appSharedState = NULL;

void MakeNaClSysCall_register_shared_state(uintptr_t sharedState)
{
	((IntPtrType)NACL_SYSCALL_ADDR(NACL_sys_register_shared_state))(sharedState);
}

void MakeNaClSysCall_exit_sandbox(void)
{
	((IntVoidType)NACL_SYSCALL_ADDR(NACL_sys_exit_sandbox))();
}

//Specifically not making this a new function as this may add a new stack frame
#define MakeNaClSysCall_callback(slotNumber) ((IntUnsignedType)NACL_SYSCALL_ADDR(NACL_sys_callback))(slotNumber)

void exitFunctionWrapper(void)
{
	register nacl_register eax asm("eax");
    //Depending on the callback return type, the return value could be in the 
    //   eax register - simple integers or pointers
    //   ST0 x87 for floating point
    //We need to save these values as the NaCl springboard clobbers these
	appSharedState->register_eax = eax;
	MakeNaClSysCall_exit_sandbox();
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

int main(int argc, char** argv)
{
	appSharedState = (struct AppSharedState*) malloc(sizeof(struct AppSharedState));

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

	MakeNaClSysCall_register_shared_state((uintptr_t)appSharedState);
	MakeNaClSysCall_exit_sandbox();
	return 0;
}
