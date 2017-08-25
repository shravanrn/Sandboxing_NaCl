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
	register nacl_reg_t eax asm("eax");
    //Depending on the callback return type, the return value could be in the 
    //   eax register - simple integers or pointers
    //   ST0 x87 for floating point
    //We need to save these values as the NaCl springboard clobbers these
	nacl_reg_t register_eax = eax;
	MakeNaClSysCall_exit_sandbox(EXIT_FROM_CALL, register_eax);
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
	struct AppSharedState* appSharedState = (struct AppSharedState*) malloc(sizeof(struct AppSharedState));

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
	MakeNaClSysCall_exit_sandbox(EXIT_FROM_MAIN, 0 /* eax - this is unused and is used only in EXIT_FROM_CALL */);
	return 0;
}
