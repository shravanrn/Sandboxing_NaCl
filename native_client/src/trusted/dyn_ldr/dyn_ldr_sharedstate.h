#include <stdio.h>

typedef int   (*threadMain_type)(void);
typedef void  (*exitFunctionWrapper_type)(void);
typedef void  (*callbackFunctionWrapper_type)(void);

typedef unsigned (*test_localMath_type)     (unsigned, unsigned, unsigned);
typedef size_t   (*test_localString_type)   (char*);

typedef void* (*malloc_type) (size_t);
typedef void  (*free_type)   (void *);

typedef void* (*dlopen_type) (const char *, int);
typedef char* (*dlerror_type)(void);
typedef void* (*dlsym_type)  (void *, const char *);
typedef int   (*dlclose_type)(void *);

typedef FILE* (*fopen_type)  (const char *, const char *);
typedef int   (*fclose_type) (FILE * stream);

struct AppSharedState
{
	threadMain_type              threadMainPtr;
    exitFunctionWrapper_type     exitFunctionWrapperPtr;
    callbackFunctionWrapper_type callbackFunctionWrapper[8];

    test_localMath_type          test_localMathPtr;
    test_localString_type        test_localStringPtr;

    malloc_type                  mallocPtr;
    free_type                    freePtr;

    dlopen_type                  dlopenPtr;
    dlerror_type                 dlerrorPtr;
    dlsym_type                   dlsymPtr;
    dlclose_type                 dlclosePtr;

    fopen_type                   fopenPtr;
    fclose_type                  fclosePtr;
};
