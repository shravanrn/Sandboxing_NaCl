#ifdef __cplusplus
  extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct _NaClSandbox_Thread
{
	struct _NaClSandbox* sandbox;
	struct NaClAppThread* thread;
	uintptr_t stack_ptr_forParameters;
	uintptr_t saved_stack_ptr_forFunctionCall;
	uintptr_t stack_ptr_arrayLocation;
	size_t callbackParamsAlreadyRead;
};

struct _NaClSandbox
{
	struct NaClApp* nap;
	struct _DS_Map* threadDataMap;
	struct AppSharedState* sharedState;
};

typedef struct _NaClSandbox NaClSandbox;
typedef struct _NaClSandbox_Thread NaClSandbox_Thread;

int initializeDlSandboxCreator(int enableLogging);
int closeSandboxCreator(void);
NaClSandbox* createDlSandbox   (char* naclGlibcLibraryPathWithTrailingSlash, char* naclInitAppFullPath);

void* mallocInSandbox(NaClSandbox* sandbox, size_t size);
void  freeInSandbox  (NaClSandbox* sandbox, void* ptr);

void* dlopenInSandbox (NaClSandbox* sandbox, const char *filename, int flag);
char* dlerrorInSandbox(NaClSandbox* sandbox);
void* dlsymInSandbox  (NaClSandbox* sandbox, void *handle, const char *symbol);
int   dlcloseInSandbox(NaClSandbox* sandbox, void *handle);

FILE* fopenInSandbox(NaClSandbox* sandbox, const char * filename, const char * mode);
int fcloseInSandbox(NaClSandbox* sandbox, FILE * stream);

NaClSandbox_Thread* preFunctionCall(NaClSandbox* sandbox, size_t paramsSize, size_t arraysSize);
void invokeFunctionCall(NaClSandbox_Thread* threadData, void* functionPtr);
void invokeFunctionCallWithSandboxPtr(NaClSandbox_Thread* threadData, uintptr_t functionPtrInSandbox);

uintptr_t getUnsandboxedAddress(NaClSandbox* sandbox, uintptr_t uaddr);
uintptr_t getSandboxedAddress(NaClSandbox* sandbox, uintptr_t uaddr);
int isAddressInSandboxMemoryOrNull(NaClSandbox* sandbox, uintptr_t uaddr);
int isAddressInNonSandboxMemoryOrNull(NaClSandbox* sandbox, uintptr_t uaddr);

//Note that various GCCs on different architecture seem to want stack alignments - either 4, 8 or 16. So 16 should work generally
#define STACKALIGNMENT 16
#define ROUND_UP_TO_POW2(val, alignment) ((val + alignment - 1) & ~(alignment - 1))
#define ROUND_DOWN_TO_POW2(val, alignment) ((val) & ~(alignment - 1))

#define ADJUST_STACK_PTR(ptr, size) (ptr + size)

#define PUSH_VAL_TO_STACK(threadData, type, value) do { \
  /*printf("Entering PUSH_VAL_TO_STACK: %u loc %u\n", (unsigned) value,(unsigned)(threadData->stack_ptr_forParameters));*/ \
  *(type *) (threadData->stack_ptr_forParameters) = (type) value; \
  threadData->stack_ptr_forParameters = ADJUST_STACK_PTR(threadData->stack_ptr_forParameters, sizeof(type)); \
} while (0)

#define PUSH_SANDBOXEDPTR_TO_STACK(threadData, type, value) PUSH_VAL_TO_STACK(threadData, type, value)

#define PUSH_PTR_TO_STACK(threadData, type, value) do {                               \
  PUSH_VAL_TO_STACK(threadData, type, (getSandboxedAddress(threadData->sandbox, (uintptr_t) value))); \
} while (0)

#define PUSH_GEN_ARRAY_TO_STACK(threadData, value, unpaddedSize) do { \
  size_t paddedSize = ROUND_UP_TO_POW2(unpaddedSize, STACKALIGNMENT); \
  memcpy((void *) threadData->stack_ptr_arrayLocation, (void *) value, unpaddedSize); \
  PUSH_PTR_TO_STACK(threadData, uintptr_t, (uintptr_t) threadData->stack_ptr_arrayLocation); \
  threadData->stack_ptr_arrayLocation = ADJUST_STACK_PTR(threadData->stack_ptr_arrayLocation, paddedSize); \
} while(0)

#define PUSH_ARRAY_TO_STACK(threadData, value) PUSH_GEN_ARRAY_TO_STACK(threadData, value, sizeof(val))

#define PUSH_STRING_TO_STACK(threadData, value) PUSH_GEN_ARRAY_TO_STACK(threadData, value, (strlen(value) + 1))

#define ARR_SIZE(val)    ROUND_UP_TO_POW2(sizeof(val)    , STACKALIGNMENT)
#define STRING_SIZE(val) ROUND_UP_TO_POW2(strlen(val) + 1, STACKALIGNMENT)

unsigned getTotalNumberOfCallbackSlots(void);
uintptr_t registerSandboxCallback(NaClSandbox* sandbox, unsigned slotNumber, uintptr_t callback);
int unregisterSandboxCallback(NaClSandbox* sandbox, unsigned slotNumber);
NaClSandbox_Thread* callbackParamsBegin(NaClSandbox* sandbox);
uintptr_t getCallbackParam(NaClSandbox_Thread* threadData, size_t size);

#define COMPLETELY_UNTRUSTED_CALLBACK_PTR_TO_STACK_PARAM(threadData, type) ((type *) getCallbackParam(threadData, sizeof(type)))
#define COMPLETELY_UNTRUSTED_CALLBACK_STACK_PARAM(threadData, type) (* COMPLETELY_UNTRUSTED_CALLBACK_PTR_TO_STACK_PARAM(threadData, type))
#define COMPLETELY_UNTRUSTED_CALLBACK_PTR_PARAM(threadData, type) ((type) getUnsandboxedAddress(threadData->sandbox, COMPLETELY_UNTRUSTED_CALLBACK_STACK_PARAM(threadData, uintptr_t)))
#define CALLBACK_RETURN_PTR(threadData, type, value) ((type) getSandboxedAddress(threadData->sandbox, value))

unsigned functionCallReturnRawPrimitiveInt(NaClSandbox_Thread* threadData);
uintptr_t functionCallReturnPtr(NaClSandbox_Thread* threadData);
uintptr_t functionCallReturnSandboxPtr(NaClSandbox_Thread* threadData);

#if defined(_M_IX86) || defined(__i386__)
	#if defined(_MSC_VER) && (_MSC_VER >= 800)
		#define SANDBOX_CALLBACK __cdecl
	#elif defined(__GNUC__) && defined(__i386) && !defined(__INTEL_COMPILER)
		#define SANDBOX_CALLBACK __attribute__((cdecl))
	#else
		#error CDecl not supported in this platform!
	#endif
#elif defined(_M_X64) || defined(__x86_64__) || defined(__ARMEL__) || defined(__MIPSEL__)
	#error Unsupported platform!
#else
	#error Unknown platform!
#endif

#ifdef __cplusplus
  }
#endif