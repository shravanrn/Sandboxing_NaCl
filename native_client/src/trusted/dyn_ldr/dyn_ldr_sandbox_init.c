#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "native_client/src/trusted/dyn_ldr/dyn_ldr_sharedstate.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"

typedef int32_t (*IntPtrType) (uintptr_t);
typedef int32_t (*IntVoidType)(void);

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
#define MakeNaClSysCall_callback() ((IntVoidType)NACL_SYSCALL_ADDR(NACL_sys_callback))()

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

void callbackFunctionWrapper(void)
{
	MakeNaClSysCall_callback();
}

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
	appSharedState->callbackFunctionWrapper = callbackFunctionWrapper;

	appSharedState->test_localMathPtr = test_localMath;
	appSharedState->test_localStringPtr = test_localString;

    appSharedState->mallocPtr = malloc;
    appSharedState->freePtr = free;

	appSharedState->dlopenPtr = dlopen;
	appSharedState->dlerrorPtr = dlerror;
	appSharedState->dlsymPtr = dlsym;
	appSharedState->dlclosePtr = dlclose;

	MakeNaClSysCall_register_shared_state((uintptr_t)appSharedState);
	MakeNaClSysCall_exit_sandbox();
	return 0;
}
