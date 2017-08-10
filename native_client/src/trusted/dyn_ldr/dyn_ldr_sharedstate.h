#if defined(_M_IX86) || defined(__i386__)
    typedef uint32_t  nacl_register; 
#elif defined(_M_X64) || defined(__x86_64__)
    typedef uint64_t  nacl_register;
#elif defined(__ARMEL__)
    typedef uint32_t  nacl_register;
#elif defined(__MIPSEL__)
    typedef uint32_t  nacl_register;
#else
    #error Unknown platform!
#endif

typedef void     (*exitFunctionWrapper_type)(void);
typedef void     (*callbackFunctionWrapper_type)(void);

typedef unsigned (*test_localMath_type)     (unsigned, unsigned, unsigned);
typedef size_t   (*test_localString_type)   (char*);

typedef void* (*malloc_type) (size_t);
typedef void  (*free_type)   (void *);

typedef void* (*dlopen_type) (const char *, int);
typedef char* (*dlerror_type)(void);
typedef void* (*dlsym_type)  (void *, const char *);
typedef int   (*dlclose_type)(void *);

struct AppSharedState
{
    nacl_register                register_eax;

    exitFunctionWrapper_type     exitFunctionWrapperPtr;
    callbackFunctionWrapper_type callbackFunctionWrapper;
    
    test_localMath_type          test_localMathPtr;
    test_localString_type        test_localStringPtr;

    malloc_type                  mallocPtr;
    free_type                    freePtr;

    dlopen_type                  dlopenPtr;
    dlerror_type                 dlerrorPtr;
    dlsym_type                   dlsymPtr;
    dlclose_type                 dlclosePtr;
};
