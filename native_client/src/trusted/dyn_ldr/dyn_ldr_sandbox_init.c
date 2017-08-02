#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "native_client/src/trusted/dyn_ldr/dyn_ldr_sharedstate.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"

typedef int32_t (*register_shared_state_type) (uintptr_t);
typedef int32_t (*exit_sandbox_type) (void);

struct AppSharedState* appSharedState = NULL;

void MakeNaClSysCall_register_shared_state(uintptr_t sharedState)
{
	((register_shared_state_type)NACL_SYSCALL_ADDR(NACL_sys_register_shared_state))(sharedState);
}

void MakeNaClSysCall_exit_sandbox(void)
{
	((exit_sandbox_type)NACL_SYSCALL_ADDR(NACL_sys_exit_sandbox))();
}

void exitFunctionWrapper(void)
{
	register nacl_register eax asm("eax");
	appSharedState->register_eax = eax;
	MakeNaClSysCall_exit_sandbox();
}

unsigned test_localMath(unsigned a, unsigned  b, unsigned c)
{
	unsigned ret;
	ret = (a*100) + (b * 10) + (c); //((uintptr_t) &a) > ((uintptr_t) &ret);(unsigned) &a;//
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
