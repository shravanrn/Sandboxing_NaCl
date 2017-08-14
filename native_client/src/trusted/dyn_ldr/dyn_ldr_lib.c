#include <string.h>

#include "native_client/src/public/nacl_desc.h"
#include "native_client/src/shared/gio/gio.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"
#include "native_client/src/trusted/dyn_ldr/dyn_ldr_lib.h"
#include "native_client/src/trusted/dyn_ldr/dyn_ldr_sharedstate.h"
#include "native_client/src/trusted/service_runtime/env_cleanser.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/load_file.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_all_modules.h"
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"
#include "native_client/src/trusted/service_runtime/nacl_switch_to_app.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_common.h"  
#include "native_client/src/trusted/service_runtime/nacl_valgrind_hooks.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_main_common.h"
#include "native_client/src/trusted/service_runtime/sel_qualify.h"

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#define CALLBACK_SLOTS_AVAILABLE (sizeof( ((struct NaClApp*) 0)->callbackSlot ) / sizeof(uintptr_t))

/********************* Utility functions ***********************/
uintptr_t NaClUserToSysOrNull(struct NaClApp *nap, uintptr_t uaddr) {
  if (uaddr == 0) { return 0;}
  return NaClUserToSys(nap, uaddr);
}

uintptr_t NaClSysToUserOrNull(struct NaClApp *nap, uintptr_t uaddr) {
  if (uaddr == 0) { return 0;}
  return NaClSysToUser(nap, uaddr);
}

/********************* Main functions ***********************/

void initializeDlSandboxCreator(int enableLogging)
{
  NaClAllModulesInit();

  if(enableLogging)
  {
    NaClLogSetVerbosity(5);
  }
}

unsigned invokeLocalMathTest(NaClSandbox* sandbox, unsigned a, unsigned b, unsigned c);
size_t invokeLocalStringTest(NaClSandbox* sandbox, char* test);
static INLINE NaClSandbox* constructNaClSandbox(struct NaClApp* nap);

//Adapted from ./native_client/src/trusted/service_runtime/sel_main.c NaClSelLdrMain
NaClSandbox* createDlSandbox(char* naclGlibcLibraryPathWithTrailingSlash, char* naclInitAppFullPath)
{
  NaClSandbox*            sandbox = NULL;
  struct NaClApp*         nap = NULL;
  // struct DynArray         env_vars;
  // struct NaClEnvCleanser  env_cleanser;
  // char const *const*      envp;
  NaClErrorCode           pq_error;
  char                    dynamic_loader[256];
  int                     app_argc;
  char**                  app_argv;
  char*                   app_argv_arr[4];
  jmp_buf*                jmp_buf_loc = NULL;
  unsigned                testResult = 0;
  size_t                  testResult2 = 5;

  nap = NaClAppCreate();
  if (nap == NULL) {
    NaClLog(LOG_ERROR, "NaClAppCreate() failed\n");
    goto error;
  }

  fflush((FILE *) NULL);

  // if (!DynArrayCtor(&env_vars, 0)) {
  //   NaClLog(LOG_FATAL, "Failed to allocate env var array\n");
  // }

  // /*
  //  * Define the environment variables for untrusted code.
  //  */
  // if (!DynArraySet(&env_vars, env_vars.num_entries, NULL)) {
  //   NaClLog(LOG_FATAL, "Adding env_vars NULL terminator failed\n");
  // }
  // NaClEnvCleanserCtor(&env_cleanser, 0, TRUE/* enable_env_passthrough */);
  // if (!NaClEnvCleanserInit(&env_cleanser, NaClGetEnviron(),
  //                          (char const *const *) env_vars.ptr_array)) {
  //   NaClLog(LOG_FATAL, "Failed to initialise env cleanser\n");
  // }
  // envp = NaClEnvCleanserEnvironment(&env_cleanser);


  NaClInsecurelyBypassAllAclChecks();

  nap->ignore_validator_result = TRUE;//(options->debug_mode_ignore_validator > 0);
  nap->skip_validator = TRUE;//(options->debug_mode_ignore_validator > 1);
  nap->enable_exception_handling = FALSE;//options->enable_exception_handling;

  // #if NACL_WINDOWS
  //   nap->attach_debug_exception_handler_func =
  //     NaClDebugExceptionHandlerStandaloneAttach;
  // #elif NACL_LINUX
  //     /* NaCl's signal handler is always enabled on Linux. */
  // #elif NACL_OSX
  //     if (!NaClInterceptMachExceptions()) {
  //       NaClLog(LOG_ERROR, "ERROR setting up Mach exception interception.\n");
  //       return FALSE;
  //     }
  // #else
  // # error Unknown host OS
  // #endif

  pq_error = NaClRunSelQualificationTests();
  if (LOAD_OK != pq_error) {
    NaClLog(LOG_ERROR, "Error while running platform checks: %s\n", NaClErrorString(pq_error));
    goto error;
  }

 #if NACL_LINUX
  NaClSignalHandlerInit();
 #endif
//    /*
//     * Patch the Windows exception dispatcher to be safe in the case of
//     * faults inside x86-64 sandboxed code.  The sandbox is not secure
//     * on 64-bit Windows without this.
//     */
//  #if (NACL_WINDOWS && NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 64)
//    NaClPatchWindowsExceptionDispatcher();
//  #endif

  NaClAppInitialDescriptorHookup(nap);

  if(strlen(naclGlibcLibraryPathWithTrailingSlash) > ((sizeof(dynamic_loader) / sizeof(char)) - 20))
  {
    NaClLog(LOG_ERROR, "Path length in naclGlibcLibraryPathWithTrailingSlash is too long\n");
    goto error;
  }

  strcpy(dynamic_loader, naclGlibcLibraryPathWithTrailingSlash);
  strcat(dynamic_loader, "runnable-ld.so");

  pq_error = NaClAppLoadFileFromFilename(nap, dynamic_loader);

  if (LOAD_OK != pq_error) {
    NaClLog(LOG_ERROR, "Error while loading from naclInitAppFullPath: %s\n", NaClErrorString(pq_error));
    goto error;
  }

  // NaClFileNameForValgrind(naclInitAppFullPath);

  /*
   * Print out a marker for scripts to use to mark the start of app
   * output.
   */
  NaClLog(1, "NACL: Application output follows\n");

  /*
   * Make sure all the file buffers are flushed before entering
   * the application code.
   */
  fflush((FILE *) NULL);

  NaClAppStartModule(nap);

  app_argc = 4;
  app_argv_arr[0] = "dyn_ldr_sandbox_init";
  app_argv_arr[1] = "--library-path";
  app_argv_arr[2] = naclGlibcLibraryPathWithTrailingSlash;
  app_argv_arr[3] = naclInitAppFullPath;
  app_argv = app_argv_arr;

  jmp_buf_loc = Stack_GetTopPtrForPush(&(nap->jumpBufferStack));

  //Normally, the NaClCreateMainThread invokes the NaCl application, nap
  // in a new thread. This is not necessary here. So, call a function we 
  // have added to the runtime, that ignores the next request to create a
  // thread. It instead calls the target app on the current thread.
  NaClOverrideNextThreadCreateToRunOnCurrentThread(TRUE, jmp_buf_loc);

  if (!NaClCreateMainThread(nap,
                            app_argc,
                            app_argv,
                            NaClGetEnviron())) {
    NaClLog(LOG_ERROR, "creating main thread failed\n");
    goto error;
  }

  sandbox = constructNaClSandbox(nap);
  if(sandbox == NULL) 
  {
    goto error;
  }

  for(unsigned i = 0; i < (unsigned) CALLBACK_SLOTS_AVAILABLE; i++)
  {
    nap->callbackSlot[i] = 0;
  }

  nap->custom_app_state = (uintptr_t) sandbox;

  NaClLog(LOG_INFO, "Running a sandbox test\n");
  testResult = invokeLocalMathTest(sandbox, 2, 3, 4);

  if(testResult != 234)
  {
    NaClLog(LOG_ERROR, "Sandbox test failed: Expected return of 234. Got %d\n", testResult);
    goto error;
  }

  testResult2 = invokeLocalStringTest(sandbox, "Hello");

  if(testResult2 != 5)
  {
    NaClLog(LOG_ERROR, "Sandbox test failed: Expected return of 5. Got %d\n", testResult2);
    goto error;
  }

  NaClLog(LOG_INFO, "Succeeded in creating sandbox\n");

  return sandbox;

error:
  fflush(stdout);
  NaClLog(LOG_ERROR, "Failed in creating sandbox\n");
  fflush(stdout);

#if NACL_LINUX
  NaClSignalHandlerFini();
#endif
  NaClAllModulesFini();

  return NULL;
}

static INLINE NaClSandbox* constructNaClSandbox(struct NaClApp* nap)
{
  NaClSandbox* sandbox = NULL;

  sandbox = (NaClSandbox*) malloc(sizeof(NaClSandbox));
  sandbox->nap = nap;

  /*Attempting to retrieve the nacl thread context as we need this to get the location of the stack*/
  if(nap->num_threads != 1)
  {
    NaClLog(LOG_ERROR, "Failed in retrieving thread information. Expected count: 1. Actual thread count: %d\n", sandbox->nap->num_threads);
    return NULL;
  }
                                                                                                                                            
  sandbox->mainThread = (struct NaClAppThread *) DynArrayGet(&(nap->threads), 0);

  {
    uintptr_t alignedValue = ROUND_UP_TO_POW2(sandbox->mainThread->user.stack_ptr, 16);

    if(sandbox->mainThread->user.stack_ptr != alignedValue)
    {
      NaClLog(LOG_INFO, "Re-aligning the NaCl stack to %u bytes\n", 16);
      sandbox->mainThread->user.stack_ptr = alignedValue;
    }
  }

  sandbox->stack_ptr = NaClUserToSysOrNull(nap, sandbox->mainThread->user.stack_ptr);
  NaClLog(LOG_INFO, "NaCl stack aligned value %u bytes\n", (unsigned) sandbox->stack_ptr);

  sandbox->sharedState = (struct AppSharedState*) nap->custom_shared_app_state;
  sandbox->callbackParamsAlreadyRead = 0;
  return sandbox;
}

/********************** "Function call stub" helpers *****************************/

void preFunctionCall(NaClSandbox* sandbox, size_t paramsSize, size_t arraysSize)
{
  #if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32

    sandbox->saved_stack_ptr_forFunctionCall = sandbox->stack_ptr;

    //Our stack would look as follows
    //return address
    //function param 1
    //function param 2
    //function param 3
    //inline array 1
    //inline array 2
    //Existing StackFrames

    //We set the location for any stack arrays
    sandbox->stack_ptr_arrayLocation = sandbox->stack_ptr - ROUND_UP_TO_POW2(arraysSize, 16);

    //make space for the return address
    paramsSize = ROUND_UP_TO_POW2(paramsSize + sizeof(uintptr_t), STACKALIGNMENT);

    /* make space for arrays, args and the return address. */
    sandbox->stack_ptr = sandbox->stack_ptr_arrayLocation - paramsSize;

    sandbox->mainThread->user.stack_ptr = NaClSysToUserOrNull(sandbox->nap, sandbox->stack_ptr);
    /* push the return address of the exit wrapper on the stack. The exit wrapper is  */
    /* for cleanup and exiting the sandbox */
    PUSH_SANDBOXEDPTR_TO_STACK(sandbox, uintptr_t, (uintptr_t) sandbox->sharedState->exitFunctionWrapperPtr);
  #else
    #error Unsupported architecture
  #endif
}

void invokeFunctionCallWithSandboxPtr(NaClSandbox* sandbox, uintptr_t functionPtrInSandbox)
{
  jmp_buf*              jmp_buf_loc;
  int                   setJmpReturn;

  jmp_buf_loc = Stack_GetTopPtrForPush(&(sandbox->nap->jumpBufferStack));
  setJmpReturn = setjmp(*jmp_buf_loc);

  if(setJmpReturn == 0)
  {
    NaClLog(LOG_INFO, "Invoking func\n");
    /*To resume execution with NaClStartThreadInApp, NaCl assumes*/
    /*that the app thread is in UNTRUSTED state*/
    sandbox->mainThread->suspend_state = NACL_APP_THREAD_UNTRUSTED;
    /*this is like a jump instruction, in that it does not return*/
    NaClStartThreadInApp(sandbox->mainThread, (nacl_reg_t) functionPtrInSandbox);
  }
}

void invokeFunctionCall(NaClSandbox* sandbox, void* functionPtr)
{
  uintptr_t functionPtrInSandbox = NaClSysToUserOrNull(sandbox->nap, (uintptr_t) functionPtr);
  invokeFunctionCallWithSandboxPtr(sandbox, functionPtrInSandbox);
}

unsigned getTotalNumberOfCallbackSlots(void)
{
  return (unsigned) CALLBACK_SLOTS_AVAILABLE;
}

uintptr_t registerSandboxCallback(NaClSandbox* sandbox, unsigned slotNumber, uintptr_t callback)
{
  unsigned callbackSlots = (unsigned) CALLBACK_SLOTS_AVAILABLE;

  if(slotNumber >= callbackSlots)
  {
    NaClLog(LOG_ERROR, "Only %u slots exists i.e. slots 0 to %u. slotNumber %u does not exist \n", 
      callbackSlots, callbackSlots - 1, slotNumber);
    return 0;
  }

  sandbox->nap->callbackSlot[slotNumber] = callback;
  return (uintptr_t) sandbox->sharedState->callbackFunctionWrapper[slotNumber];
}

int unregisterSandboxCallback(NaClSandbox* sandbox, unsigned slotNumber)
{
  unsigned callbackSlots = (unsigned) CALLBACK_SLOTS_AVAILABLE;

  if(slotNumber >= callbackSlots)
  {
    NaClLog(LOG_ERROR, "Only %u slots exists i.e. slots 0 to %u. slotNumber %u does not exist \n", 
      callbackSlots, callbackSlots - 1, slotNumber);
    return FALSE;
  }

  sandbox->nap->callbackSlot[slotNumber] = 0;
  return TRUE;
}

uintptr_t getCallbackParam(NaClSandbox* sandbox, size_t size)
{
  #if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32
    //The parameters start at the location of the NaCl sp
    //
    //The set of calls to exit the sandbox look as follows
    //
    // 1) functionInLibThatMakesCallback(param1, param2, param3)
    // 2) callbackFunctionWrapperN in dyn_ldr_sandbox_init.c
    // 3) NaClSysCall - NaClSysCallback in nacl_syscall_common.c
    //
    //It is expected that the top of the NaCl stack would look like
    //
    // callbackFunctionWrapperN Stackframe
    // functionInLibThatMakesCallback Stackframe
    // 
    // i.e.
    //
    // (callbackFunctionWrapperN) Return addr <=
    // (callbackFunctionWrapperN) Param 1 
    // (callbackFunctionWrapperN) Padding for alignment
    // (functionInLibThatMakesCallback) Return addr
    // (functionInLibThatMakesCallback) Param 1
    // (functionInLibThatMakesCallback) Param 2
    // (functionInLibThatMakesCallback) Param 3
    //
    //
    //For some reason, at this stage the sp does not point to return address
    //Instead it points to the first arg
    //
    // (callbackFunctionWrapperN) Return addr
    // (callbackFunctionWrapperN) Param 1 <=
    // (callbackFunctionWrapperN) Padding for alignment
    // (functionInLibThatMakesCallback) Return addr
    // (functionInLibThatMakesCallback) Param 1
    // (functionInLibThatMakesCallback) Param 2
    // (functionInLibThatMakesCallback) Param 3
    //
    //Since the NaCl Springboard does mess with assembly, it is possible, that it has messed with sp as well
    //
    //Through trial, the padding used was found to be 32 bytes. This offset was derived by printing the contents of the NaCl stack. This means
    //(functionInLibThatMakesCallback) Param 1 is located 32 bytes ahead of the sp
    //
    #define CallbackParmetersStartOffset 32
    uintptr_t paramPointer = NaClUserToSysOrNull(sandbox->nap,
      sandbox->mainThread->user.stack_ptr + CallbackParmetersStartOffset + sandbox->callbackParamsAlreadyRead);

    sandbox->callbackParamsAlreadyRead += size;
    return paramPointer;

  #else
    #error Unsupported architecture
  #endif
}

unsigned functionCallReturnRawPrimitiveInt(NaClSandbox* sandbox)
{
  unsigned ret;

  #if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32
    ret = (unsigned) sandbox->sharedState->register_eax;      
    sandbox->stack_ptr = sandbox->saved_stack_ptr_forFunctionCall;
    NaClLog(LOG_INFO, "Return int %u\n", ret);
  #else
    #error Unsupported architecture
  #endif

  return ret;
}

uintptr_t functionCallReturnPtr(NaClSandbox* sandbox)
{
  uintptr_t ret;  

  #if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32 
    ret = (uintptr_t) NaClUserToSysOrNull(sandbox->nap, (uintptr_t) sandbox->sharedState->register_eax);
    sandbox->stack_ptr = sandbox->saved_stack_ptr_forFunctionCall;
    NaClLog(LOG_INFO, "Return pointer. Sandbox: %p, App: %p\n", (void*) sandbox->sharedState->register_eax, (void*) ret);
  #else
    #error Unsupported architecture
  #endif

  return ret;
}

uintptr_t functionCallReturnSandboxPtr(NaClSandbox* sandbox)
{
  return (uintptr_t) functionCallReturnRawPrimitiveInt(sandbox);
}

/********************** Stubs for some basic functions *****************************/

unsigned invokeLocalMathTest(NaClSandbox* sandbox, unsigned a, unsigned b, unsigned c)
{
  preFunctionCall(sandbox, sizeof(a) + sizeof(b) + sizeof(c), 0);

  PUSH_VAL_TO_STACK(sandbox, unsigned, a);
  PUSH_VAL_TO_STACK(sandbox, unsigned, b);
  PUSH_VAL_TO_STACK(sandbox, unsigned, c);

  invokeFunctionCallWithSandboxPtr(sandbox, (uintptr_t)sandbox->sharedState->test_localMathPtr);

  return (unsigned)functionCallReturnRawPrimitiveInt(sandbox);
}

size_t invokeLocalStringTest(NaClSandbox* sandbox, char* test)
{
  preFunctionCall(sandbox, sizeof(test), STRING_SIZE(test));

  PUSH_STRING_TO_STACK(sandbox, test);

  invokeFunctionCallWithSandboxPtr(sandbox, (uintptr_t)sandbox->sharedState->test_localStringPtr);

  return (size_t)functionCallReturnRawPrimitiveInt(sandbox);
}

void* mallocInSandbox(NaClSandbox* sandbox, size_t size)
{
  preFunctionCall(sandbox, sizeof(size), 0);

  PUSH_VAL_TO_STACK(sandbox, size_t, size);

  invokeFunctionCallWithSandboxPtr(sandbox, (uintptr_t)sandbox->sharedState->mallocPtr);

  return (void *) functionCallReturnPtr(sandbox);
}

void freeInSandbox(NaClSandbox* sandbox, void* ptr)
{
  preFunctionCall(sandbox, sizeof(ptr), 0);

  PUSH_PTR_TO_STACK(sandbox, void*, ptr);

  invokeFunctionCallWithSandboxPtr(sandbox, (uintptr_t)sandbox->sharedState->freePtr);
}

void* dlopenInSandbox(NaClSandbox* sandbox, const char *filename, int flag)
{
  preFunctionCall(sandbox, sizeof(filename) + sizeof(flag), STRING_SIZE(filename));

  PUSH_STRING_TO_STACK(sandbox, filename);
  PUSH_VAL_TO_STACK(sandbox, int, flag);

  invokeFunctionCallWithSandboxPtr(sandbox, (uintptr_t)sandbox->sharedState->dlopenPtr);

  return (void *) functionCallReturnPtr(sandbox);
}

char* dlerrorInSandbox(NaClSandbox* sandbox)
{
  preFunctionCall(sandbox, 0 /* no args */, 0);

  invokeFunctionCallWithSandboxPtr(sandbox, (uintptr_t)sandbox->sharedState->dlerrorPtr);

  return (char *) functionCallReturnPtr(sandbox);
}

void* dlsymInSandbox(NaClSandbox* sandbox, void *handle, const char *symbol)
{
  preFunctionCall(sandbox, sizeof(handle) + sizeof(symbol), STRING_SIZE(symbol));

  PUSH_PTR_TO_STACK(sandbox, void *, handle);
  PUSH_STRING_TO_STACK(sandbox, symbol);

  invokeFunctionCallWithSandboxPtr(sandbox, (uintptr_t)sandbox->sharedState->dlsymPtr);

  return (void *) functionCallReturnPtr(sandbox);
}

int dlcloseInSandbox(NaClSandbox* sandbox, void *handle)
{
  preFunctionCall(sandbox, sizeof(handle), 0);

  PUSH_PTR_TO_STACK(sandbox, void *, handle);

  invokeFunctionCallWithSandboxPtr(sandbox, (uintptr_t)sandbox->sharedState->dlclosePtr);

  return (int)functionCallReturnRawPrimitiveInt(sandbox);
}

FILE* fopenInSandbox(NaClSandbox* sandbox, const char * filename, const char * mode)
{
  preFunctionCall(sandbox, sizeof(filename) + sizeof(mode), STRING_SIZE(filename) + STRING_SIZE(mode));

  PUSH_STRING_TO_STACK(sandbox, filename);
  PUSH_STRING_TO_STACK(sandbox, mode);

  invokeFunctionCallWithSandboxPtr(sandbox, (uintptr_t)sandbox->sharedState->fopenPtr);

  return (FILE*) functionCallReturnPtr(sandbox);
}

int fcloseInSandbox(NaClSandbox* sandbox, FILE * stream)
{
  preFunctionCall(sandbox, sizeof(stream), 0);

  PUSH_PTR_TO_STACK(sandbox, FILE *, stream);

  invokeFunctionCallWithSandboxPtr(sandbox, (uintptr_t)sandbox->sharedState->fclosePtr);

  return (int)functionCallReturnRawPrimitiveInt(sandbox);
}