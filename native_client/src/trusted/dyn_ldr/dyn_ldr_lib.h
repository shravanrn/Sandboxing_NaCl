#include <stddef.h>
#include <stdint.h>

typedef struct
{
	struct NaClApp* nap;
	struct NaClAppThread* mainThread;
	uintptr_t stack_ptr;
	uintptr_t saved_stack_ptr_forFunctionCall;
	uintptr_t stack_ptr_arrayLocation;
	size_t callbackParamsAlreadyRead;
	struct AppSharedState* sharedState;
} NaClSandbox;

void initializeDlSandboxCreator(int enableLogging);
NaClSandbox* createDlSandbox   (char* naclGlibcLibraryPathWithTrailingSlash, char* naclInitAppFullPath);

void* mallocInSandbox(NaClSandbox* sandbox, size_t size);
void  freeInSandbox  (NaClSandbox* sandbox, void* ptr);

void* dlopenInSandbox (NaClSandbox* sandbox, const char *filename, int flag);
char* dlerrorInSandbox(NaClSandbox* sandbox);
void* dlsymInSandbox  (NaClSandbox* sandbox, void *handle, const char *symbol);
int   dlcloseInSandbox(NaClSandbox* sandbox, void *handle);

void preFunctionCall(NaClSandbox* sandbox, size_t paramsSize, size_t arraysSize);
void invokeFunctionCall(NaClSandbox* sandbox, void* functionPtr);
void invokeFunctionCallWithSandboxPtr(NaClSandbox* sandbox, uintptr_t functionPtrInSandbox);

uintptr_t NaClUserToSysOrNull(struct NaClApp *nap, uintptr_t uaddr);
uintptr_t NaClSysToUserOrNull(struct NaClApp *nap, uintptr_t uaddr);

//Note that various GCCs on different architecture seem to want stack alignments - either 4, 8 or 16. So 16 should work generally
#define STACKALIGNMENT 16
#define ROUND_UP_TO_POW2(val, alignment) ((val + alignment - 1) & ~(alignment - 1))

#define ADJUST_STACK_PTR(ptr, size) (ptr + size)

#define PUSH_VAL_TO_STACK(sandbox, type, value) do { \
  /*printf("Entering PUSH_VAL_TO_STACK: %u loc %u\n", (unsigned) value,(unsigned)(sandbox->stack_ptr));*/ \
  *(type *) (sandbox->stack_ptr) = (type) value; \
  sandbox->stack_ptr = ADJUST_STACK_PTR(sandbox->stack_ptr, sizeof(type)); \
} while (0)

#define PUSH_SANDBOXEDPTR_TO_STACK(sandbox, type, value) PUSH_VAL_TO_STACK(sandbox, type, value)

#define PUSH_PTR_TO_STACK(sandbox, type, value) do {                               \
  PUSH_VAL_TO_STACK(sandbox, type, (NaClSysToUserOrNull(sandbox->nap, (uintptr_t) value))); \
} while (0)

#define PUSH_GEN_ARRAY_TO_STACK(sandbox, value, unpaddedSize) do { \
  size_t paddedSize = ROUND_UP_TO_POW2(unpaddedSize, STACKALIGNMENT); \
  memcpy((void *) sandbox->stack_ptr_arrayLocation, (void *) value, unpaddedSize); \
  PUSH_PTR_TO_STACK(sandbox, uintptr_t, (uintptr_t) sandbox->stack_ptr_arrayLocation); \
  sandbox->stack_ptr_arrayLocation = ADJUST_STACK_PTR(sandbox->stack_ptr_arrayLocation, paddedSize); \
} while(0)

#define PUSH_ARRAY_TO_STACK(sandbox, value) PUSH_GEN_ARRAY_TO_STACK(sandbox, value, sizeof(val))

#define PUSH_STRING_TO_STACK(sandbox, value) PUSH_GEN_ARRAY_TO_STACK(sandbox, value, (strlen(value) + 1))

#define ARR_SIZE(val)    ROUND_UP_TO_POW2(sizeof(val)    , STACKALIGNMENT)
#define STRING_SIZE(val) ROUND_UP_TO_POW2(strlen(val) + 1, STACKALIGNMENT)

unsigned getTotalNumberOfCallbackSlots(void);
uintptr_t registerSandboxCallback(NaClSandbox* sandbox, unsigned slotNumber, uintptr_t callback);
int unregisterSandboxCallback(NaClSandbox* sandbox, unsigned slotNumber);
uintptr_t getCallbackParam(NaClSandbox* sandbox, size_t size);

#define COMPLETELY_UNTRUSTED_CALLBACK_PARAM(sandbox, type) (*((type *) getCallbackParam(sandbox, sizeof(type))))
#define COMPLETELY_UNTRUSTED_CALLBACK_PARAM_PTR(sandbox, type) ((type) NaClUserToSysOrNull(sandbox->nap, (uintptr_t) COMPLETELY_UNTRUSTED_CALLBACK_PARAM(sandbox, type)))
#define CALLBACK_PARAMS_FINISHED(sandbox) (sandbox->callbackParamsAlreadyRead = 0)

unsigned functionCallReturnRawPrimitiveInt(NaClSandbox* sandbox);
uintptr_t functionCallReturnPtr(NaClSandbox* sandbox);
uintptr_t functionCallReturnSandboxPtr(NaClSandbox* sandbox);

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