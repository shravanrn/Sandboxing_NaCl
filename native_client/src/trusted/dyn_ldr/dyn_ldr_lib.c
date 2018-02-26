#include <string.h>

#include "native_client/src/public/nacl_desc.h"
#include "native_client/src/shared/gio/gio.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"
#include "native_client/src/trusted/dyn_ldr/datastructures/ds_stack.h"
#include "native_client/src/trusted/dyn_ldr/datastructures/ds_map.h"
#include "native_client/src/trusted/dyn_ldr/dyn_ldr_lib.h"
#include "native_client/src/trusted/dyn_ldr/dyn_ldr_test_structs.h"
#include "native_client/src/trusted/service_runtime/elf_symboltable_mapping.h"
#include "native_client/src/trusted/service_runtime/env_cleanser.h"
#include "native_client/src/trusted/service_runtime/include/bits/mman.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/load_file.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_all_modules.h"
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"
#include "native_client/src/trusted/service_runtime/nacl_switch_to_app.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_common.h"  
#include "native_client/src/trusted/service_runtime/nacl_tls.h"
#include "native_client/src/trusted/service_runtime/nacl_valgrind_hooks.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_main_common.h"
#include "native_client/src/trusted/service_runtime/sel_qualify.h"
#include "native_client/src/trusted/service_runtime/sys_memory.h"

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#define CALLBACK_SLOTS_AVAILABLE (sizeof( ((struct NaClApp*) 0)->callbackSlot ) / sizeof(uintptr_t))

/********************* Utility functions ***********************/

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32
  #define GetSandboxedStackPointer(sandbox, user) (user.stack_ptr)
  #define SetStackPointerToSandboxedPointer(sandbox, user, val) (user.stack_ptr = val)
#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 64
  #define GetSandboxedStackPointer(sandbox, user) (getSandboxedAddress(sandbox, user.rsp))
  #define SetStackPointerToSandboxedPointer(sandbox, user, val) (user.rsp = getUnsandboxedAddress(sandbox, val))
#elif defined(__ARMEL__) || defined(__MIPSEL__)
  #error Unsupported platform!
#else
  #error Unknown platform!
#endif


uintptr_t getUnsandboxedAddress(NaClSandbox* sandbox, uintptr_t uaddr){
  if (uaddr == 0) { return 0;}
  return NaClUserToSys(sandbox->nap, uaddr
    #if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 64
      & 0xFFFFFFFF
    #endif
  );
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
  // NaClErrorCode           pq_error;

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

  if(enableLogging == 2)
  {
    NaClLogSetVerbosity(5);
  }
  else if(enableLogging == 1)
  {
    //use the default which logs anything < 0 such as LOG_INFO, LOG_ERROR but not any internal logs
  }
  else
  {
    NaClLogSetVerbosity(LOG_FATAL);
  }

  NaClSetUseExtendedTls(TRUE);

  // pq_error = NaClRunSelQualificationTests();
  // if (LOAD_OK != pq_error) {
  //   NaClLog(LOG_ERROR, "Error while running platform checks: %s\n", NaClErrorString(pq_error));
  //   goto error;
  // }

  return TRUE;

// error:
//   fflush(stdout);
//   NaClLog(LOG_ERROR, "Failed in creating sandbox\n");
//   fflush(stdout);

//   closeSandboxCreator();

//   return FALSE;
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
void invokeIdentifyCallbackOffsetHelper(NaClSandbox* sandbox);
int invokeCheckStructSizesTest
(
  NaClSandbox* sandbox,
  int size_DoubleAlign,
  int size_PointerSize,
  int size_IntSize,
  int size_LongSize,
  int size_LongLongSize
);

//Adapted from ./native_client/src/trusted/service_runtime/sel_main.c NaClSelLdrMain
NaClSandbox* createDlSandbox(const char* naclLibraryPath, const char* naclInitAppFullPath)
{
  NaClSandbox*            sandbox = NULL;
  struct NaClApp*         nap = NULL;
  // struct DynArray         env_vars;
  // struct NaClEnvCleanser  env_cleanser;
  // char const *const*      envp;
  NaClErrorCode           pq_error;
  unsigned                testResult = 0;
  int                     testResult2 = 0;
  int                     testResult3 = 0;
  struct NaClDesc*        blob_file = NULL;

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


  blob_file = (struct NaClDesc *) NaClDescIoDescOpen(naclLibraryPath, NACL_ABI_O_RDONLY, 0);
  if (NULL == blob_file) {
    NaClLog(LOG_FATAL, "Cannot open \"%s\".\n", naclLibraryPath);
  }

  NaClAppInitialDescriptorHookup(nap);

  //Normally the symbol table in the file is basically ignored
  // This basically turns on the some code that has been added for the purpose of this library
  // that loads the symbol table in the struct NaClApp
  NaClAppLoadSymbolTableMapping(TRUE);

  pq_error = NaClAppLoadFileFromFilename(nap, naclInitAppFullPath);

  if (LOAD_OK != pq_error) {
    NaClLog(LOG_ERROR, "Error while loading from naclInitAppFullPath: %s\n", NaClErrorString(pq_error));
    goto error;
  }

  // NaClFileNameForValgrind(naclInitAppFullPath);
  pq_error = NaClMainLoadIrt(nap, blob_file, NULL);
  if (LOAD_OK != pq_error) {
    NaClLog(LOG_ERROR, "Error while loading \"%s\": %s\n", naclLibraryPath, NaClErrorString(pq_error));
    goto error;
  }

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

  //Normally, the NaClCreateMainThread/NaClCreateAdditionalThread invokes the NaCl application, nap
  // in a new thread. This is not necessary here. So, call a function we 
  // have added to the runtime, that ignores the next request to create a
  // thread. It instead calls the target app on the current thread.
  NaClOverrideNextThreadCreateToRunOnCurrentThread(TRUE, &(nap->mainJumpBuffer));

  if (!NaClCreateMainThread(nap,
                            0, //argc,
                            NULL, //argv,
                            NaClGetEnviron())) {
    NaClLog(LOG_ERROR, "creating main thread failed\n");
    goto error;
  }

  #if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 64
    //The 64 bit trampoline does not copy the function parameters from registers to the called function in the sandbox by default
    //So, it has been added, but has been disabled by default as this should not occur when invoking the main function
    //However after the main function, we can enable the parameter copying
    NaClSwitchToFunctionCallMode();
  #endif

  for(unsigned i = 0; i < (unsigned) CALLBACK_SLOTS_AVAILABLE; i++)
  {
    nap->callbackSlot[i] = 0;
  }

  sandbox = constructNaClSandbox(nap);
  if(sandbox == NULL) 
  {
    goto error;
  }

  //Get pointers to commonly used functions
  //Since these are used commonly, we will store their sandboxed address, to avoid converting each time we call them
  {

    for(unsigned i = 0; i < (unsigned) CALLBACK_SLOTS_AVAILABLE; i++)
    {
      char callbackName[50];
      sprintf(callbackName, "callbackFunctionWrapper%u", i);
      sandbox->callbackFunctionWrapper[i] = (callbackFunctionWrapper_type) getSandboxedAddress(sandbox, (uintptr_t) symbolTableLookupInSandbox(sandbox, callbackName));

      if(sandbox->callbackFunctionWrapper[i] == NULL)
      {
        NaClLog(LOG_ERROR, "Sandbox could not find the address of callback wrapper %u", i);
        goto error;        
      }
    }

    sandbox->threadMainPtr = (threadMain_type) getSandboxedAddress(sandbox, (uintptr_t) symbolTableLookupInSandbox(sandbox, "threadMain"));
    sandbox->exitFunctionWrapperPtr = (exitFunctionWrapper_type) getSandboxedAddress(sandbox, (uintptr_t) symbolTableLookupInSandbox(sandbox, "exitFunctionWrapper"));

    if(sandbox->threadMainPtr == NULL || sandbox->exitFunctionWrapperPtr == NULL)
    {
      NaClLog(LOG_ERROR, "Sandbox could not find thread main or exit wrapper");
      goto error;
    }

    sandbox->mallocPtr = (malloc_type) getSandboxedAddress(sandbox, (uintptr_t) symbolTableLookupInSandbox(sandbox, "malloc"));
    sandbox->freePtr   = (free_type)   getSandboxedAddress(sandbox, (uintptr_t) symbolTableLookupInSandbox(sandbox, "free"));
    sandbox->fopenPtr  = (fopen_type)  getSandboxedAddress(sandbox, (uintptr_t) symbolTableLookupInSandbox(sandbox, "fopen"));
    sandbox->fclosePtr = (fclose_type) getSandboxedAddress(sandbox, (uintptr_t) symbolTableLookupInSandbox(sandbox, "fclose"));
  
    if(sandbox->mallocPtr == NULL || sandbox->freePtr == NULL || sandbox->fopenPtr == NULL || sandbox->fclosePtr == NULL)
    {
      NaClLog(LOG_ERROR, "Sandbox could not find the address of crt functions: malloc, free, fopen, fclose");
      goto error;
    }
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

  testResult3 = invokeCheckStructSizesTest(sandbox, 
    sizeof(struct TestStructDoubleAlign),
    sizeof(struct TestStructPointerSize),
    sizeof(struct TestStructIntSize),
    sizeof(struct TestStructLongSize),
    sizeof(struct TestStructLongLongSize)
  );

  if(!testResult3)
  {
    NaClLog(LOG_ERROR, "Sandbox test failed: Sizes of datastructures inside and outside the sandbox do not agree\n");
    goto error; 
  }

  NaClLog(LOG_INFO, "Acquiring the callback parameter start offset\n");
  invokeIdentifyCallbackOffsetHelper(sandbox);

  if(sandbox->callbackParameterStartOffset == -1)
  {
    NaClLog(LOG_ERROR, "Sandbox failed to acquire callback parameter start offset\n");
    goto error;
  }
  else
  {
    NaClLog(LOG_INFO, "Sandbox callback parameter start offset: %" PRId32 "\n", sandbox->callbackParameterStartOffset);
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

  if(threadData == NULL)
  {
    return NULL;
  }

  threadData->sandbox = sandbox;
  threadData->thread = (struct NaClAppThread *) DynArrayGet(&(sandbox->nap->threads), sandbox->nap->threads.num_entries - 1);
  threadData->thread->jumpBufferStack = (DS_Stack *) malloc(sizeof(DS_Stack));

  if(threadData->thread->jumpBufferStack == NULL)
  {
    free(threadData);
    return NULL;
  }

  threadData->thread->custom_app_state = (uintptr_t) threadData;
  Stack_Init(threadData->thread->jumpBufferStack);

  {
    uintptr_t alignedValue = ROUND_DOWN_TO_POW2(GetSandboxedStackPointer(sandbox, threadData->thread->user), 16);

    if(GetSandboxedStackPointer(sandbox, threadData->thread->user) != alignedValue)
    {
      NaClLog(LOG_INFO, "Re-aligning the NaCl stack to %u bytes\n", 16);
      SetStackPointerToSandboxedPointer(sandbox, threadData->thread->user, alignedValue);
    }
  }

  threadData->callbackParamsAlreadyRead = 0;
  #if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 64
    threadData->registerParameterNumber = 0;
    threadData->callbackParameterNumber = 0;
    threadData->floatRegisterParameterNumber = 0;
  #endif

  return threadData;
}

NaClSandbox* constructNaClSandbox(struct NaClApp* nap)
{
  NaClSandbox* sandbox;
  NaClSandbox_Thread* threadData;
  uint32_t threadId = NaClThreadId();

  sandbox = (NaClSandbox*) malloc(sizeof(NaClSandbox));

  if(sandbox == NULL)
  {
    NaClLog(LOG_ERROR, "Malloc failed while creating sandbox datastructure\n");
    return NULL;
  }

  sandbox->threadCreateMutex = (struct NaClMutex*) malloc(sizeof(struct NaClMutex));
  if (!sandbox->threadCreateMutex)
  {
    NaClLog(LOG_ERROR, "Failed to create mutex\n");
    goto err_createdSandbox;
  }

  if (!NaClMutexCtor(sandbox->threadCreateMutex)) 
  {
    NaClLog(LOG_ERROR, "Failed to init mutex\n");
    goto err_createdMutex;
  }

  sandbox->nap = nap;
  sandbox->threadDataMap = (DS_Map *) malloc(sizeof(DS_Map));

  if(sandbox->threadDataMap == NULL)
  {
    NaClLog(LOG_ERROR, "Failed to allocate thread map\n");
    goto err_createdThreadMap;
  }

  Map_Init(sandbox->threadDataMap);

  /*Attempting to retrieve the nacl thread context as we need this to get the location of the stack*/
  if(nap->num_threads != 1)
  {
    NaClLog(LOG_ERROR, "Failed in retrieving thread information. Expected count: 1. Actual thread count: %d\n", sandbox->nap->num_threads);
    goto err_createdThreadMap;
  }

  threadData = constructNaClSandboxThread(sandbox);

  if(threadData == NULL)
  {
    NaClLog(LOG_ERROR, "Failed to create data structure for thread\n");
    goto err_createdThreadMap;
  }

  Map_Put(sandbox->threadDataMap, threadId, (uintptr_t) threadData);
  sandbox->extraState = NULL;
  return sandbox;

err_createdThreadMap:
  free(sandbox->threadDataMap);
err_createdMutex:
  free(sandbox->threadCreateMutex);
err_createdSandbox:
  free(sandbox);
  return NULL;
}

unsigned long getSandboxMemoryBase(NaClSandbox* sandbox)
{
	return sandbox->nap->mem_start;
}

/********************** "Function call stub" helpers *****************************/
NaClSandbox_Thread* getThreadData(NaClSandbox* sandbox)
{
  uint32_t threadId; 
  NaClSandbox_Thread* threadData;
  threadId = NaClThreadId();
  threadData = (NaClSandbox_Thread*) Map_Get(sandbox->threadDataMap, threadId);

  if(threadData == NULL)
  {
    uintptr_t newStackSandboxed;
    int32_t threadCreateFailed;
    struct NaClAppThread* existingThread;

    NaClLog(LOG_INFO, "Creating new thread structure for id: %u\n", (unsigned) threadId);

    existingThread = ((NaClSandbox_Thread*)sandbox->threadDataMap->values[0])->thread;

    NaClLog(LOG_INFO, "Data start %p, (sandboxed) %p. Stack size : %p\n", 
      (void *) getUnsandboxedAddress(sandbox, sandbox->nap->data_start),
      (void*) sandbox->nap->data_start,
      (void*) sandbox->nap->stack_size);

    newStackSandboxed = (uintptr_t) NaClSysMmapIntern(
      sandbox->nap,
      //We need to create the new stack in the memory region the app can access
      (void *) sandbox->nap->data_start,
      sandbox->nap->stack_size,
      NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE, 
      NACL_ABI_MAP_ANONYMOUS | NACL_ABI_MAP_PRIVATE,
      //We are creating anonymous memory so file descriptor is -1 and offset is 0
      -1,
      0
    );

    if(
      ((void *)newStackSandboxed == NACL_ABI_MAP_FAILED) || 
      NaClPtrIsNegErrno(&newStackSandboxed)
    )
    {
      NaClLog(LOG_FATAL, "Failed to create a new stack for the thread: %u\n", (unsigned) threadId);
    }

    NaClLog(LOG_INFO, "New Stack Range %p to %p (sandboxed: %p to %p)\n",
      (void *) getUnsandboxedAddress(sandbox, newStackSandboxed),
      (void *) getUnsandboxedAddress(sandbox, newStackSandboxed + sandbox->nap->stack_size),
      (void *) (newStackSandboxed),
      (void *) (newStackSandboxed + sandbox->nap->stack_size)
    );

    //Move the stack pointer to the bottom of the stack as it grows upwards
    newStackSandboxed = newStackSandboxed + sandbox->nap->stack_size;

    //Normally, the NaClCreateMainThread/NaClCreateAdditionalThread invokes the NaCl application, nap
    // in a new thread. This is not necessary here. So, call a function we 
    // have added to the runtime, that ignores the next request to create a
    // thread. It instead calls the target app on the current thread.
    NaClXMutexLock(sandbox->threadCreateMutex);
    {
      NaClOverrideNextThreadCreateToRunOnCurrentThread(TRUE, &(sandbox->nap->mainJumpBuffer));

      threadCreateFailed = NaClCreateAdditionalThread(sandbox->nap, 
        (uintptr_t) sandbox->threadMainPtr, 
        getUnsandboxedAddress(sandbox, newStackSandboxed),
        //These are Thread local storage variables
        //NaCl uses these to store address of code that helps switch in and out of NaCl'd code
        //We just reuse the values from the existing thread
        NaClTlsGetTlsValue1(existingThread),
        NaClTlsGetTlsValue2(existingThread)
      );

      if(threadCreateFailed)
      {
        NaClXMutexUnlock(sandbox->threadCreateMutex);
        NaClLog(LOG_FATAL, "Failed in creating thread data structure\n");
        return NULL;
      }

      threadData = constructNaClSandboxThread(sandbox);
    
      if(threadData == NULL)
      {
        NaClXMutexUnlock(sandbox->threadCreateMutex);
        NaClLog(LOG_FATAL, "Failed to create data structure for thread\n");
        return NULL;
      }

      Map_Put(sandbox->threadDataMap, threadId, (uintptr_t) threadData);
    }
    NaClXMutexUnlock(sandbox->threadCreateMutex);
  }

  #if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 64 && NACL_LINUX
    NaClTlsSetCurrentThreadExtended(threadData->thread);
  #endif
  return threadData;
}

NaClSandbox_Thread* preFunctionCall(NaClSandbox* sandbox, size_t paramsSize, size_t arraysSize)
{
  #if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 //32 or 64 bit

    NaClSandbox_Thread* threadData = getThreadData(sandbox);
    #if NACL_BUILD_SUBARCH == 64
      threadData->registerParameterNumber = 0;
    #endif
    threadData->saved_stack_ptr_forFunctionCall = GetSandboxedStackPointer(sandbox, threadData->thread->user);
    threadData->stack_ptr_forParameters = getUnsandboxedAddress(sandbox, ROUND_DOWN_TO_POW2(GetSandboxedStackPointer(sandbox, threadData->thread->user), STACKALIGNMENT));

    //Our stack would look as follows
    //return address
    //----------if 32 bit----------
    //function param 1
    //function param 2
    //function param 3
    //...
    //function param 9
    //-------else if 64 bit--------
    //function param 7
    //function param 8
    //function param 9
    // <..unused space..>
    //----------end if----------
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
    paramsSize = ROUND_UP_TO_POW2(paramsSize + STACK_JUST_IN_CASE_BUFFER_SPACE, STACKALIGNMENT) + sizeof(uintptr_t);

    /* make space for arrays, args and the return address. */
    threadData->stack_ptr_forParameters = threadData->stack_ptr_arrayLocation - paramsSize;

    SetStackPointerToSandboxedPointer(sandbox, threadData->thread->user,getSandboxedAddress(sandbox, threadData->stack_ptr_forParameters));
    /* push the return address of the exit wrapper on the stack. The exit wrapper is  */
    /* for cleanup and exiting the sandbox */
    PUSH_RET_TO_STACK(threadData, uintptr_t, (uintptr_t) sandbox->exitFunctionWrapperPtr);

    return threadData;
  #else
    #error Unsupported architecture
  #endif
}

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 64

  uint64_t* getParamRegister(NaClSandbox_Thread* threadData, unsigned parameterNumber)
  {
    if(parameterNumber == 0) 
    {
      return &(threadData->thread->user.rdi);
    }
    else if(parameterNumber == 1) 
    {
      return &(threadData->thread->user.rsi);
    }
    else if(parameterNumber == 2) 
    {
      return &(threadData->thread->user.rdx);
    }
    else if(parameterNumber == 3) 
    {
      return &(threadData->thread->user.rcx);
    }
    else if(parameterNumber == 4) 
    {
      return &(threadData->thread->user.r8);
    }
    else if(parameterNumber == 5) 
    {
      return &(threadData->thread->user.r9);
    }

    return NULL;
  }

  uint64_t* getFloatParamRegister(NaClSandbox_Thread* threadData, unsigned parameterNumber)
  {
    if(parameterNumber == 0)
    {
      return &threadData->thread->user.xmm0;
    }
    else if(parameterNumber == 1)
    {
      return &threadData->thread->user.xmm1;
    }
    else if(parameterNumber == 2)
    {
      return &threadData->thread->user.xmm2;
    }
    else if(parameterNumber == 3)
    {
      return &threadData->thread->user.xmm3;
    }
    else if(parameterNumber == 4)
    {
      return &threadData->thread->user.xmm4;
    }
    else if(parameterNumber == 5)
    {
      return &threadData->thread->user.xmm5;
    }
    else if(parameterNumber == 6)
    {
      return &threadData->thread->user.xmm6;
    }
    else if(parameterNumber == 7)
    {
      return &threadData->thread->user.xmm7;
    }

    return NULL;
  }

#endif

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
    uintptr_t functionPtrToUse;
    NaClLog(LOG_INFO, "Invoking func\n");
    /*To resume execution with NaClStartThreadInApp, NaCl assumes*/
    /*that the app thread is in UNTRUSTED state*/
    NaClAppThreadSetSuspendState(threadData->thread, /* old state */ NACL_APP_THREAD_TRUSTED, /* new state */ NACL_APP_THREAD_UNTRUSTED);

    //Looks like nacl expects a sandboxed ptr to start the thread for 32 bit and an unsandboxed ptr for 64 bit
    #if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 64
      functionPtrToUse = getUnsandboxedAddress(threadData->sandbox, (uintptr_t) functionPtrInSandbox);
    #else
      functionPtrToUse = functionPtrInSandbox;
    #endif
    /*this is like a jump instruction, in that it does not return*/
    NaClStartThreadInApp(threadData->thread, (nacl_reg_t) functionPtrToUse);
  }
  else
  {
    #if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 // 32 or 64 bit

      //make sure the saved stack pointer makes sense
      if(
        //saved stack pointer should be below the current stack pointer
        saved_stack_ptr_forFunctionCall >= GetSandboxedStackPointer(threadData->sandbox, threadData->thread->user) && 
        //saved stack pointer should be in the valid range for a NaCl sandbox address
        //this check was inferred from the NaClIsUserAddr function in native_client/src/trusted/service_runtime/sel_ldr-inl.h
        saved_stack_ptr_forFunctionCall <= ((uintptr_t) 1U << threadData->sandbox->nap->addr_bits)
      ) 
      {
        SetStackPointerToSandboxedPointer(threadData->sandbox, threadData->thread->user, saved_stack_ptr_forFunctionCall);
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
          (void *) GetSandboxedStackPointer(threadData->sandbox, threadData->thread->user),
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

uintptr_t registerSandboxCallbackWithState(NaClSandbox* sandbox, unsigned slotNumber, uintptr_t callback, void* state)
{
  if(callback == 0)
  {
    return 0;
  }
  else
  {
    unsigned callbackSlots = (unsigned) CALLBACK_SLOTS_AVAILABLE;

    if(slotNumber >= callbackSlots)
    {
      NaClLog(LOG_ERROR, "Only %u slots exists i.e. slots 0 to %u. slotNumber %u does not exist \n", 
        callbackSlots, callbackSlots - 1, slotNumber);
      return 0;
    }

    sandbox->nap->callbackSlot[slotNumber] = callback;
    sandbox->nap->callbackSlotState[slotNumber] = state;
    return (uintptr_t) sandbox->callbackFunctionWrapper[slotNumber];
  }
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
  sandbox->nap->callbackSlotState[slotNumber] = 0;
  return TRUE;
}

int getFreeSandboxCallbackSlot(NaClSandbox* sandbox, unsigned* slot)
{
  for(unsigned i = 0; i < (unsigned) CALLBACK_SLOTS_AVAILABLE; i++)
  {
    if(sandbox->nap->callbackSlot[i] == 0)
    {
      *slot = i;
      return 1;
    }
  }

  return 0;
}

NaClSandbox_Thread* callbackParamsBegin(NaClSandbox* sandbox)
{
  NaClSandbox_Thread* threadData = getThreadData(sandbox);
  threadData->callbackParamsAlreadyRead = 0;
  #if defined(_M_X64) || defined(__x86_64__)
    threadData->callbackParameterNumber = 0;
  #endif
  return threadData;
}

SANDBOX_CALLBACK void identifyCallbackParamOffset(uintptr_t sandboxPtr)
{
  //The set of calls to exit the sandbox look as follows
  //
  // 1) functionInLibThatMakesCallback(param1, param2, param3)
  // 2) callbackFunctionWrapperN in dyn_ldr_sandbox_init.c
  // 3) NaClSysCall - NaClSysCallback in nacl_syscall_common.c

  //We don't want to make assumptions about whether the NACL trampoline leaves the stack pointer
  // i.e. whether the stack pointer is at the top of the stack
  //So we figure out the offset that points to the first callback parameter
  // by looking for known data from a test callback

  //Additionally, this would ensure things like stack smash pointers etc. 
  // just work, without worrying if callback param offsets have account for them

  //It is expected that the top of the NaCl stack would look something like
  //
  // callbackFunctionWrapperN Stackframe
  // functionInLibThatMakesCallback Stackframe
  // 
  // i.e.
  //
  // (callbackFunctionWrapperN) Return addr
  // (callbackFunctionWrapperN) Param 1 
  // (callbackFunctionWrapperN) Padding for alignment
  // (functionInLibThatMakesCallback) Return addr
  // (functionInLibThatMakesCallback) Param 1
  // (functionInLibThatMakesCallback) Param 2
  // (functionInLibThatMakesCallback) Param 3

  //We know the values to "functionInLibThatMakesCallback" Param 1, Param 2 ...
  // and we want to locate the address of Param1
  //The function we are looking for is a function that has 10 uint32 integer parameters going from 1 to 10

  NaClSandbox* sandbox = (NaClSandbox*) sandboxPtr;
  NaClSandbox_Thread* threadData = callbackParamsBegin(sandbox);

  #if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32

    uint32_t paramToLookFor = 1;
    uint32_t multiplier = 1;

  #elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 64

    //In x64 first 6 parameters are in registers parameter 7 onwards is on the stack
    uint32_t paramToLookFor = 7;
    uint32_t multiplier = 2;

  #else
    #error Unsupported architecture
  #endif

  uintptr_t currPointer = getUnsandboxedAddress(threadData->sandbox, GetSandboxedStackPointer(sandbox, threadData->thread->user));
  
  for (uint32_t i = 0; i <= 256; i += 4, currPointer += 4)
  {
    uint32_t* paramPtr = (uint32_t*) currPointer;
    if(
      *(paramPtr) == (paramToLookFor) &&
      *(paramPtr + multiplier*1) == (paramToLookFor + 1) &&
      *(paramPtr + multiplier*2) == (paramToLookFor + 2) &&
      *(paramPtr + multiplier*3) == (paramToLookFor + 3)
    )
    {
      threadData->sandbox->callbackParameterStartOffset = i;
      return;
    }
  }
  
  threadData->sandbox->callbackParameterStartOffset = -1;
  #undef ParamType
}

uintptr_t getCallbackParam(NaClSandbox_Thread* threadData, size_t size)
{
  #if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32

    uintptr_t paramPointer = getUnsandboxedAddress(threadData->sandbox,
      GetSandboxedStackPointer(sandbox, threadData->thread->user) + threadData->sandbox->callbackParameterStartOffset + threadData->callbackParamsAlreadyRead);

    threadData->callbackParamsAlreadyRead += size;
    return paramPointer;

  #elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 64
    //Similar to x86 version except the first 6 parameters are in registers
    //Parameter 7 onwards is on the stack
    if(threadData->callbackParameterNumber < 6 && size <= 64) 
    {
      uint64_t* regPtr = getParamRegister(threadData, threadData->callbackParameterNumber);
      threadData->callbackParameterNumber++;
      return (uintptr_t) regPtr;
    }
    else if(threadData->callbackParameterNumber < 5 && size <= 128) 
    {
      //This is a wierd case where the struct is split across the registers, but we need send back a pointer to the object
      //We can't just return a pointer to the NACL's copy of the registers like the previous case
      //as NACL doesn't store the parameter register contents in adjascent locations in memeory (See native_client/src/trusted/service_runtime/arch/x86_64/sel_rt_64.h)
      //So we need to copy the values into a new memory location. 
      //This makes this function wierd in that there is now one case where it allocates memory which the caller has to de-allocate later
      //We will ignore this for now - but this will obviously cause a memory leak in the program till it is fixed      
      uint64_t* regPtr1;
      uint64_t* regPtr2;

      uint64_t* valCasted = (uint64_t *) malloc(128);

      regPtr1 = getParamRegister(threadData, threadData->callbackParameterNumber);
      threadData->callbackParameterNumber++;

      regPtr2 = getParamRegister(threadData, threadData->callbackParameterNumber);
      threadData->callbackParameterNumber++;

      valCasted[0] = *regPtr1;
      valCasted[1] = *regPtr2;

      return (uintptr_t) valCasted;
    }
    else
    {
      //See x86 section above for explanation 
      uintptr_t paramPointer = getUnsandboxedAddress(threadData->sandbox,
        GetSandboxedStackPointer(threadData->sandbox, threadData->thread->user) + threadData->sandbox->callbackParameterStartOffset + threadData->callbackParamsAlreadyRead);

      threadData->callbackParamsAlreadyRead += size;
      return paramPointer;
    }
  #else
    #error Unsupported architecture
  #endif
}

long functionCallReturnRawPrimitiveInt(NaClSandbox_Thread* threadData)
{
  long ret;

  #if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 //32 and 64 bit
    //Note for 64 bit this is actuall the rax register
    ret = (long) threadData->thread->register_eax;
    NaClLog(LOG_INFO, "Return int %lu\n", ret);
  #else
    #error Unsupported architecture
  #endif

  return ret;
}

float functionCallReturnFloat(NaClSandbox_Thread* threadData)
{
  #if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 //32 and 64 bit
    //Note for 64 bit this is actuall the rax register
    float ret = *((float *) (&(threadData->thread->register_xmm0)));
    NaClLog(LOG_INFO, "Return float %f\n", ret);
    return ret;
  #else
    #error Unsupported architecture
  #endif
}

double functionCallReturnDouble(NaClSandbox_Thread* threadData)
{
  #if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 //32 and 64 bit
    //Note for 64 bit this is actuall the rax register
    double ret = *((double *) (&(threadData->thread->register_xmm0)));
    NaClLog(LOG_INFO, "Return double %f\n", ret);
    return ret;
  #else
    #error Unsupported architecture
  #endif
}

uintptr_t functionCallReturnPtr(NaClSandbox_Thread* threadData)
{
  uintptr_t ret;  

  #if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 //32 and 64 bit
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

void* symbolTableLookupInSandbox(NaClSandbox* sandbox, const char *symbol)
{
  //interface 2 has to resolve functions by looking at the symbolTable
  for(uint32_t i = 0, symbolCount = sandbox->nap->symbolTableMapping->symbolCount; i < symbolCount; i++)
  {
    struct SymbolTableMapEntry* entry = &sandbox->nap->symbolTableMapping->symbolMap[i];
    if(strcmp(entry->name, symbol) == 0)
    {
      return (void *) getUnsandboxedAddress(sandbox, (uintptr_t) entry->address);
    }
  }

  return NULL;
}

/********************** Stubs for some basic functions *****************************/

unsigned invokeLocalMathTest(NaClSandbox* sandbox, unsigned a, unsigned b, unsigned c)
{
  NaClSandbox_Thread* threadData;
  void* test_localMathPtr = (void*) symbolTableLookupInSandbox(sandbox, "test_localMath");

  if(test_localMathPtr == NULL) return 0;

  threadData = preFunctionCall(sandbox, sizeof(a) + sizeof(b) + sizeof(c), 0);

  PUSH_VAL_TO_STACK(threadData, unsigned, a);
  PUSH_VAL_TO_STACK(threadData, unsigned, b);
  PUSH_VAL_TO_STACK(threadData, unsigned, c);

  invokeFunctionCall(threadData, test_localMathPtr);

  return (unsigned)functionCallReturnRawPrimitiveInt(threadData);
}

size_t invokeLocalStringTest(NaClSandbox* sandbox, char* test)
{
  NaClSandbox_Thread* threadData;
  void* test_localStringPtr = (void*) symbolTableLookupInSandbox(sandbox, "test_localString");

  if(test_localStringPtr == NULL) return 0;

  threadData = preFunctionCall(sandbox, sizeof(test), STRING_SIZE(test));

  PUSH_STRING_TO_STACK(threadData, test);

  invokeFunctionCall(threadData, test_localStringPtr);

  return (size_t)functionCallReturnRawPrimitiveInt(threadData);
}

int invokeCheckStructSizesTest
(
  NaClSandbox* sandbox,
  int size_DoubleAlign,
  int size_PointerSize,
  int size_IntSize,
  int size_LongSize,
  int size_LongLongSize
)
{
  NaClSandbox_Thread* threadData;
  void* test_checkStructSizes = (void*) symbolTableLookupInSandbox(sandbox, "test_checkStructSizes");

  if(test_checkStructSizes == NULL) return 0;

  threadData = preFunctionCall(sandbox, sizeof(size_DoubleAlign) + sizeof(size_PointerSize) + sizeof(size_IntSize) + sizeof(size_LongSize) + sizeof(size_LongLongSize), 0);

  PUSH_VAL_TO_STACK(threadData, int, size_DoubleAlign);
  PUSH_VAL_TO_STACK(threadData, int, size_PointerSize);
  PUSH_VAL_TO_STACK(threadData, int, size_IntSize);
  PUSH_VAL_TO_STACK(threadData, int, size_LongSize);
  PUSH_VAL_TO_STACK(threadData, int, size_LongLongSize);

  invokeFunctionCall(threadData, test_checkStructSizes);

  return (int)functionCallReturnRawPrimitiveInt(threadData);
}

void invokeIdentifyCallbackOffsetHelper(NaClSandbox* sandbox)
{
  short slotNumber = 0;
  NaClSandbox_Thread* threadData;
  uintptr_t callback;

  void* identifyCallbackOffsetHelperPtr = (void*) symbolTableLookupInSandbox(sandbox, "identifyCallbackOffsetHelper");

  if(identifyCallbackOffsetHelperPtr == NULL) { sandbox->callbackParameterStartOffset = -1; return; }

  callback = registerSandboxCallback(sandbox, slotNumber, (uintptr_t) identifyCallbackParamOffset);

  threadData = preFunctionCall(sandbox, sizeof(callback), 0);
  PUSH_VAL_TO_STACK(threadData, uintptr_t, callback);

  invokeFunctionCall(threadData, identifyCallbackOffsetHelperPtr);
  unregisterSandboxCallback(sandbox, slotNumber);
}

void* mallocInSandbox(NaClSandbox* sandbox, size_t size)
{
  NaClSandbox_Thread* threadData = preFunctionCall(sandbox, sizeof(size), 0);

  PUSH_VAL_TO_STACK(threadData, size_t, size);

  invokeFunctionCallWithSandboxPtr(threadData, (uintptr_t)sandbox->mallocPtr);

  return (void *) functionCallReturnPtr(threadData);
}

void freeInSandbox(NaClSandbox* sandbox, void* ptr)
{
  NaClSandbox_Thread* threadData = preFunctionCall(sandbox, sizeof(ptr), 0);

  PUSH_PTR_TO_STACK(threadData, void*, ptr);

  invokeFunctionCallWithSandboxPtr(threadData, (uintptr_t)sandbox->freePtr);
}

FILE* fopenInSandbox(NaClSandbox* sandbox, const char * filename, const char * mode)
{
  NaClSandbox_Thread* threadData = preFunctionCall(sandbox, sizeof(filename) + sizeof(mode), STRING_SIZE(filename) + STRING_SIZE(mode));

  PUSH_STRING_TO_STACK(threadData, filename);
  PUSH_STRING_TO_STACK(threadData, mode);

  invokeFunctionCallWithSandboxPtr(threadData, (uintptr_t)sandbox->fopenPtr);

  return (FILE*) functionCallReturnPtr(threadData);
}

int fcloseInSandbox(NaClSandbox* sandbox, FILE * stream)
{
  NaClSandbox_Thread* threadData = preFunctionCall(sandbox, sizeof(stream), 0);

  PUSH_PTR_TO_STACK(threadData, FILE *, stream);

  invokeFunctionCallWithSandboxPtr(threadData, (uintptr_t)sandbox->fclosePtr);

  return (int)functionCallReturnRawPrimitiveInt(threadData);
}
