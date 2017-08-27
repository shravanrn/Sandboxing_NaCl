#include <string.h>

#include "native_client/src/public/nacl_desc.h"
#include "native_client/src/shared/gio/gio.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"
#include "native_client/src/trusted/dyn_ldr/datastructures/ds_stack.h"
#include "native_client/src/trusted/dyn_ldr/datastructures/ds_map.h"
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

uintptr_t getUnsandboxedAddress(NaClSandbox* sandbox, uintptr_t uaddr){
  if (uaddr == 0) { return 0;}
  return NaClUserToSys(sandbox->nap, uaddr);
}
uintptr_t getSandboxedAddress(NaClSandbox* sandbox, uintptr_t uaddr){
  if (uaddr == 0) { return 0;}
  return NaClSysToUser(sandbox->nap, uaddr);
}
int isAddressInSandboxMemoryOrNull(NaClSandbox* sandbox, uintptr_t uaddr){
  return uaddr == 0 || NaClIsUserAddr(sandbox->nap, uaddr);
}
int isAddressInNonSandboxMemoryOrNull(NaClSandbox* sandbox, uintptr_t uaddr){
  return uaddr == 0 || !NaClIsUserAddr(sandbox->nap, uaddr);  
}


/********************* Main functions ***********************/

int initializeDlSandboxCreator(int enableLogging)
{
  NaClErrorCode           pq_error;

  // #if NACL_LINUX
  //    NaClSignalHandlerInit();
  // #elif NACL_OSX
  //    if (!NaClInterceptMachExceptions()) {
  //      NaClLog(LOG_ERROR, "ERROR setting up Mach exception interception.\n");
  //      return FALSE;
  //    }
  // #elif NACL_WINDOWS 
  //    #if (NACL_WINDOWS && NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 64)
  //      /*
  //       * Patch the Windows exception dispatcher to be safe in the case of
  //       * faults inside x86-64 sandboxed code.  The sandbox is not secure
  //       * on 64-bit Windows without this.
  //       */
  //      NaClPatchWindowsExceptionDispatcher();
  //    #endif
  // #else
  //    # error Unknown host OS
  // #endif

  NaClAllModulesInit();

  if(enableLogging)
  {
    NaClLogSetVerbosity(5);
  }

  pq_error = NaClRunSelQualificationTests();
  if (LOAD_OK != pq_error) {
    NaClLog(LOG_ERROR, "Error while running platform checks: %s\n", NaClErrorString(pq_error));
    goto error;
  }

  return TRUE;

error:
  fflush(stdout);
  NaClLog(LOG_ERROR, "Failed in creating sandbox\n");
  fflush(stdout);

  closeSandboxCreator();

  return FALSE;
}

int closeSandboxCreator(void)
{
  // #if NACL_LINUX
  //   NaClSignalHandlerFini();
  // #endif

  NaClAllModulesFini();

  return TRUE;
}

unsigned invokeLocalMathTest(NaClSandbox* sandbox, unsigned a, unsigned b, unsigned c);
size_t invokeLocalStringTest(NaClSandbox* sandbox, char* test);
NaClSandbox* constructNaClSandbox(struct NaClApp* nap);

//Adapted from ./native_client/src/trusted/service_runtime/sel_main.c NaClSelLdrMain
NaClSandbox* createDlSandbox(char* naclGlibcLibraryPathWithTrailingSlash, char* naclInitAppFullPath)
{
  NaClSandbox*            sandbox = NULL;
  struct NaClApp*         nap = NULL;
  // struct DynArray         env_vars;
  // struct NaClEnvCleanser  env_cleanser;
  // char const *const*      envp;
  NaClErrorCode           pq_error;
  char                    dynamic_loader[1024];
  int                     app_argc;
  char**                  app_argv;
  char*                   app_argv_arr[4];
  unsigned                testResult = 0;
  size_t                  testResult2 = 0;

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
  //   nap->attach_debug_exception_handler_func = NaClDebugExceptionHandlerStandaloneAttach;
  // #endif

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

  //Normally, the NaClCreateMainThread invokes the NaCl application, nap
  // in a new thread. This is not necessary here. So, call a function we 
  // have added to the runtime, that ignores the next request to create a
  // thread. It instead calls the target app on the current thread.
  NaClOverrideNextThreadCreateToRunOnCurrentThread(TRUE, &(nap->mainJumpBuffer));

  if (!NaClCreateMainThread(nap,
                            app_argc,
                            app_argv,
                            NaClGetEnviron())) {
    NaClLog(LOG_ERROR, "creating main thread failed\n");
    goto error;
  }

  for(unsigned i = 0; i < (unsigned) CALLBACK_SLOTS_AVAILABLE; i++)
  {
    nap->callbackSlot[i] = 0;
  }

  sandbox = constructNaClSandbox(nap);
  if(sandbox == NULL) 
  {
    goto error;
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

  return NULL;
}

NaClSandbox_Thread* constructNaClSandboxThread(NaClSandbox* sandbox)
{
  NaClSandbox_Thread* threadData = (NaClSandbox_Thread*) malloc(sizeof(NaClSandbox_Thread));
  threadData->sandbox = sandbox;
  threadData->thread = (struct NaClAppThread *) DynArrayGet(&(sandbox->nap->threads), sandbox->nap->threads.num_entries - 1);
  threadData->thread->jumpBufferStack = (DS_Stack *) malloc(sizeof(DS_Stack));
  threadData->thread->custom_app_state = (uintptr_t) threadData;
  Stack_Init(threadData->thread->jumpBufferStack);

  {
    uintptr_t alignedValue = ROUND_DOWN_TO_POW2(threadData->thread->user.stack_ptr, 16);

    if(threadData->thread->user.stack_ptr != alignedValue)
    {
      NaClLog(LOG_INFO, "Re-aligning the NaCl stack to %u bytes\n", 16);
      threadData->thread->user.stack_ptr = alignedValue;
    }
  }

  threadData->callbackParamsAlreadyRead = 0;
  return threadData;
}

NaClSandbox* constructNaClSandbox(struct NaClApp* nap)
{
  NaClSandbox* sandbox;
  NaClSandbox_Thread* threadData;
  uint32_t threadId = NaClThreadId();

  sandbox = (NaClSandbox*) malloc(sizeof(NaClSandbox));
  sandbox->nap = nap;
  sandbox->sharedState = (struct AppSharedState*) nap->custom_shared_app_state;
  Map_Init(sandbox->threadDataMap);

  /*Attempting to retrieve the nacl thread context as we need this to get the location of the stack*/
  if(nap->num_threads != 1)
  {
    NaClLog(LOG_ERROR, "Failed in retrieving thread information. Expected count: 1. Actual thread count: %d\n", sandbox->nap->num_threads);
    return NULL;
  }

  threadData = constructNaClSandboxThread(sandbox);
  Map_Put(sandbox->threadDataMap, threadId, (uintptr_t) threadData);
  return sandbox;
}

/********************** "Function call stub" helpers *****************************/
NaClSandbox_Thread* getThreadData(NaClSandbox* sandbox)
{
  uint32_t threadId = NaClThreadId();
  return (NaClSandbox_Thread*) Map_Get(sandbox->threadDataMap, threadId);
}

NaClSandbox_Thread* preFunctionCall(NaClSandbox* sandbox, size_t paramsSize, size_t arraysSize)
{
  #if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32

    NaClSandbox_Thread* threadData = getThreadData(sandbox);
    threadData->saved_stack_ptr_forFunctionCall = threadData->thread->user.stack_ptr;
    threadData->stack_ptr_forParameters = getUnsandboxedAddress(sandbox, ROUND_DOWN_TO_POW2(threadData->thread->user.stack_ptr, STACKALIGNMENT));

    //Our stack would look as follows
    //return address
    //function param 1
    //function param 2
    //function param 3
    //inline array 1
    //inline array 2
    //Existing StackFrames

    //We set the location for any stack arrays
    threadData->stack_ptr_arrayLocation = threadData->stack_ptr_forParameters - ROUND_UP_TO_POW2(arraysSize, 16);

    //We also add a small buffer just in case.
    //We have seen that during when a sandbox makes a callback to a function outside the sandbox, the 
    //  stack pointer is not quite where we would expect it to be due to the fact that this callback 
    //  is mediated by the trampoline, which is hand written assembly that messes with these values
    //  In this situation, if we now make a function call back into the sandbox, we could overwrite part of the stack
    //  if we continue from the current stack pointer.
    //  This buffer guards against such problems by leaving some space to ensure we aren't overwriting 
    //  anything on the stack.
    #define STACK_JUST_IN_CASE_BUFFER_SPACE 128
    //make space for the return address
    paramsSize = ROUND_UP_TO_POW2(paramsSize + sizeof(uintptr_t) + STACK_JUST_IN_CASE_BUFFER_SPACE, STACKALIGNMENT);

    /* make space for arrays, args and the return address. */
    threadData->stack_ptr_forParameters = threadData->stack_ptr_arrayLocation - paramsSize;

    threadData->thread->user.stack_ptr = getSandboxedAddress(sandbox, threadData->stack_ptr_forParameters);
    /* push the return address of the exit wrapper on the stack. The exit wrapper is  */
    /* for cleanup and exiting the sandbox */
    PUSH_SANDBOXEDPTR_TO_STACK(threadData, uintptr_t, (uintptr_t) sandbox->sharedState->exitFunctionWrapperPtr);

    return threadData;
  #else
    #error Unsupported architecture
  #endif
}

void invokeFunctionCallWithSandboxPtr(NaClSandbox_Thread* threadData, uintptr_t functionPtrInSandbox)
{
  jmp_buf*              jmp_buf_loc;
  int                   setJmpReturn;
  uintptr_t             saved_stack_ptr_forFunctionCall;

  //It is very important to save this data locally as this data could be overwritten
  //in the following scenario
  //If we make a function call (let's call this F1), and that function call makes a callback 
  //(let's call this C1) and that callback makes another call into the sandbox, (let's call this F2)
  //then F1's saved threadData->saved_stack_ptr_forFunctionCall would be overwritten by F2's
  //threadData->saved_stack_ptr_forFunctionCall
  //However if we save the value locally, we can restore the appropriate value
  saved_stack_ptr_forFunctionCall = threadData->saved_stack_ptr_forFunctionCall;

  jmp_buf_loc = Stack_GetTopPtrForPush(threadData->thread->jumpBufferStack);
  setJmpReturn = setjmp(*jmp_buf_loc);

  if(setJmpReturn == 0)
  {
    NaClLog(LOG_INFO, "Invoking func\n");
    /*To resume execution with NaClStartThreadInApp, NaCl assumes*/
    /*that the app thread is in UNTRUSTED state*/
    NaClAppThreadSetSuspendState(threadData->thread, /* old state */ NACL_APP_THREAD_TRUSTED, /* new state */ NACL_APP_THREAD_UNTRUSTED);
    /*this is like a jump instruction, in that it does not return*/
    NaClStartThreadInApp(threadData->thread, (nacl_reg_t) functionPtrInSandbox);
  }
  else
  {
    #if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32

      //make sure the saved stack pointer makes sense
      if(
        //saved stack pointer should be below the current stack pointer
        saved_stack_ptr_forFunctionCall >= threadData->thread->user.stack_ptr && 
        //saved stack pointer should be in the valid range for a NaCl sandbox address
        saved_stack_ptr_forFunctionCall <= ((uintptr_t) 1U << threadData->sandbox->nap->addr_bits)
      ) 
      {
        threadData->thread->user.stack_ptr = saved_stack_ptr_forFunctionCall;
      }
      else
      {
        //This branch should never be taken
        //For some reason, this happened briefly during development, where 
        // threadData->saved_stack_ptr_forFunctionCall got modified, without any explanation
        //This problem went away after development was completed, but saving the notes on this 
        // problem when it was occuring
        //
        //This could possibly be the case that this value is stored in a register which is not saved
        //Then as part of the register restoration, when we longjmp after the function call
        //this value isn't restored and this causes it to get an unexpected stack pointer
        //However it is not clear if this is what is causing this issue, as some preliminary testing
        //refutes this idea
        //That said, we can just ignore the replaced values in this case, as
        //we won't bother reclaiming a part of the stack
        NaClLog(LOG_WARNING, "WARNING: Got an unexpected value for saved stack pointer. Curr stack pointer(sandboxed): %p , saved(sandboxed): %p\n",
          (void *) threadData->thread->user.stack_ptr,
          (void *) threadData->saved_stack_ptr_forFunctionCall
        );
      }

    #else
      #error Unsupported architecture
    #endif
  }
}

void invokeFunctionCall(NaClSandbox_Thread* threadData, void* functionPtr)
{
  uintptr_t functionPtrInSandbox = getSandboxedAddress(threadData->sandbox, (uintptr_t) functionPtr);
  invokeFunctionCallWithSandboxPtr(threadData, functionPtrInSandbox);
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

NaClSandbox_Thread* callbackParamsBegin(NaClSandbox* sandbox)
{
  NaClSandbox_Thread* threadData = getThreadData(sandbox);
  threadData->callbackParamsAlreadyRead = 0;
  return threadData;
}

uintptr_t getCallbackParam(NaClSandbox_Thread* threadData, size_t size)
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
    uintptr_t paramPointer = getUnsandboxedAddress(threadData->sandbox,
      threadData->thread->user.stack_ptr + CallbackParmetersStartOffset + threadData->callbackParamsAlreadyRead);

    threadData->callbackParamsAlreadyRead += size;
    return paramPointer;

  #else
    #error Unsupported architecture
  #endif
}

unsigned functionCallReturnRawPrimitiveInt(NaClSandbox_Thread* threadData)
{
  unsigned ret;

  #if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32
    ret = (unsigned) threadData->thread->register_eax;
    NaClLog(LOG_INFO, "Return int %u\n", ret);
  #else
    #error Unsupported architecture
  #endif

  return ret;
}

uintptr_t functionCallReturnPtr(NaClSandbox_Thread* threadData)
{
  uintptr_t ret;  

  #if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32 
    ret = (uintptr_t) getUnsandboxedAddress(threadData->sandbox, (uintptr_t) threadData->thread->register_eax);
    NaClLog(LOG_INFO, "Return pointer. Sandbox: %p, App: %p\n", (void*) threadData->thread->register_eax, (void*) ret);
  #else
    #error Unsupported architecture
  #endif

  return ret;
}

uintptr_t functionCallReturnSandboxPtr(NaClSandbox_Thread* threadData)
{
  return (uintptr_t) functionCallReturnRawPrimitiveInt(threadData);
}

/********************** Stubs for some basic functions *****************************/

unsigned invokeLocalMathTest(NaClSandbox* sandbox, unsigned a, unsigned b, unsigned c)
{
  NaClSandbox_Thread* threadData = preFunctionCall(sandbox, sizeof(a) + sizeof(b) + sizeof(c), 0);

  PUSH_VAL_TO_STACK(threadData, unsigned, a);
  PUSH_VAL_TO_STACK(threadData, unsigned, b);
  PUSH_VAL_TO_STACK(threadData, unsigned, c);

  invokeFunctionCallWithSandboxPtr(threadData, (uintptr_t)sandbox->sharedState->test_localMathPtr);

  return (unsigned)functionCallReturnRawPrimitiveInt(threadData);
}

size_t invokeLocalStringTest(NaClSandbox* sandbox, char* test)
{
  NaClSandbox_Thread* threadData = preFunctionCall(sandbox, sizeof(test), STRING_SIZE(test));

  PUSH_STRING_TO_STACK(threadData, test);

  invokeFunctionCallWithSandboxPtr(threadData, (uintptr_t)sandbox->sharedState->test_localStringPtr);

  return (size_t)functionCallReturnRawPrimitiveInt(threadData);
}

void* mallocInSandbox(NaClSandbox* sandbox, size_t size)
{
  NaClSandbox_Thread* threadData = preFunctionCall(sandbox, sizeof(size), 0);

  PUSH_VAL_TO_STACK(threadData, size_t, size);

  invokeFunctionCallWithSandboxPtr(threadData, (uintptr_t)sandbox->sharedState->mallocPtr);

  return (void *) functionCallReturnPtr(threadData);
}

void freeInSandbox(NaClSandbox* sandbox, void* ptr)
{
  NaClSandbox_Thread* threadData = preFunctionCall(sandbox, sizeof(ptr), 0);

  PUSH_PTR_TO_STACK(threadData, void*, ptr);

  invokeFunctionCallWithSandboxPtr(threadData, (uintptr_t)sandbox->sharedState->freePtr);
}

void* dlopenInSandbox(NaClSandbox* sandbox, const char *filename, int flag)
{
  NaClSandbox_Thread* threadData = preFunctionCall(sandbox, sizeof(filename) + sizeof(flag), STRING_SIZE(filename));

  PUSH_STRING_TO_STACK(threadData, filename);
  PUSH_VAL_TO_STACK(threadData, int, flag);

  invokeFunctionCallWithSandboxPtr(threadData, (uintptr_t)sandbox->sharedState->dlopenPtr);

  return (void *) functionCallReturnPtr(threadData);
}

char* dlerrorInSandbox(NaClSandbox* sandbox)
{
  NaClSandbox_Thread* threadData = preFunctionCall(sandbox, 0 /* no args */, 0);

  invokeFunctionCallWithSandboxPtr(threadData, (uintptr_t)sandbox->sharedState->dlerrorPtr);

  return (char *) functionCallReturnPtr(threadData);
}

void* dlsymInSandbox(NaClSandbox* sandbox, void *handle, const char *symbol)
{
  NaClSandbox_Thread* threadData = preFunctionCall(sandbox, sizeof(handle) + sizeof(symbol), STRING_SIZE(symbol));

  PUSH_PTR_TO_STACK(threadData, void *, handle);
  PUSH_STRING_TO_STACK(threadData, symbol);

  invokeFunctionCallWithSandboxPtr(threadData, (uintptr_t)sandbox->sharedState->dlsymPtr);

  return (void *) functionCallReturnPtr(threadData);
}

int dlcloseInSandbox(NaClSandbox* sandbox, void *handle)
{
  NaClSandbox_Thread* threadData = preFunctionCall(sandbox, sizeof(handle), 0);

  PUSH_PTR_TO_STACK(threadData, void *, handle);

  invokeFunctionCallWithSandboxPtr(threadData, (uintptr_t)sandbox->sharedState->dlclosePtr);

  return (int)functionCallReturnRawPrimitiveInt(threadData);
}

FILE* fopenInSandbox(NaClSandbox* sandbox, const char * filename, const char * mode)
{
  NaClSandbox_Thread* threadData = preFunctionCall(sandbox, sizeof(filename) + sizeof(mode), STRING_SIZE(filename) + STRING_SIZE(mode));

  PUSH_STRING_TO_STACK(threadData, filename);
  PUSH_STRING_TO_STACK(threadData, mode);

  invokeFunctionCallWithSandboxPtr(threadData, (uintptr_t)sandbox->sharedState->fopenPtr);

  return (FILE*) functionCallReturnPtr(threadData);
}

int fcloseInSandbox(NaClSandbox* sandbox, FILE * stream)
{
  NaClSandbox_Thread* threadData = preFunctionCall(sandbox, sizeof(stream), 0);

  PUSH_PTR_TO_STACK(threadData, FILE *, stream);

  invokeFunctionCallWithSandboxPtr(threadData, (uintptr_t)sandbox->sharedState->fclosePtr);

  return (int)functionCallReturnRawPrimitiveInt(threadData);
}