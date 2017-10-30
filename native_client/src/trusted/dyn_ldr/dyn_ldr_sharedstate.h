#include <stdio.h>
#include <stdint.h>

typedef int   (*threadMain_type)(void);
typedef void  (*exitFunctionWrapper_type)(void);
typedef void  (*callbackFunctionWrapper_type)(void);

typedef unsigned (*test_localMath_type)     (unsigned, unsigned, unsigned);
typedef size_t   (*test_localString_type)   (char*);

typedef void (*CallbackHelperType) (uint32_t, uint32_t, uint32_t,
                                    uint32_t, uint32_t, uint32_t,
                                    uint32_t, uint32_t, uint32_t, uint32_t);

typedef void (*identifyCallbackOffsetHelper_type)(CallbackHelperType callback);

typedef void* (*malloc_type) (size_t);
typedef void  (*free_type)   (void *);

typedef FILE* (*fopen_type)  (const char *, const char *);
typedef int   (*fclose_type) (FILE * stream);

struct AppSharedState
{
	threadMain_type                   threadMainPtr;
    exitFunctionWrapper_type          exitFunctionWrapperPtr;
    callbackFunctionWrapper_type      callbackFunctionWrapper[8];

    test_localMath_type               test_localMathPtr;
    test_localString_type             test_localStringPtr;
    identifyCallbackOffsetHelper_type identifyCallbackOffsetHelperPtr;

    malloc_type                       mallocPtr;
    free_type                         freePtr;

    fopen_type                        fopenPtr;
    fclose_type                       fclosePtr;

    int32_t checkChar;
};
