#include <stddef.h>
#include <stdint.h>

typedef struct
{
	struct NaClApp* nap;
	struct NaClAppThread* mainThread;
	uintptr_t stack_ptr;
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

void preFunctionCall(NaClSandbox* sandbox, size_t paramsSize);
void invokeFunctionCall(NaClSandbox* sandbox, void* functionPtr);

uintptr_t NaClUserToSysOrNull(struct NaClApp *nap, uintptr_t uaddr);
uintptr_t NaClSysToUserOrNull(struct NaClApp *nap, uintptr_t uaddr);

#define PUSH_VAL_TO_STACK(sandbox, type, value) do {           \
  *(type *) (sandbox->stack_ptr) = (type) value; sandbox->stack_ptr += sizeof(type); \
} while (0)

#define PUSH_PTR_TO_STACK(sandbox, type, value) do {                               \
  PUSH_VAL_TO_STACK(sandbox, type, (NaClSysToUserOrNull(sandbox->nap, (uintptr_t) value))); \
} while (0)    

unsigned functionCallReturnRawPrimitiveInt(NaClSandbox* sandbox);
uintptr_t functionCallReturnPtr(NaClSandbox* sandbox);