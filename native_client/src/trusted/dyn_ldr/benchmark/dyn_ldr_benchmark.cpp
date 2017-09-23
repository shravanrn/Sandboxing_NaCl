#include "native_client/src/trusted/dyn_ldr/dyn_ldr_lib.h"
#include "native_client/src/trusted/dyn_ldr/dyn_ldr_sharedstate.h"

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <chrono>
using namespace std::chrono;

#if defined(_WIN32)
	char SEPARATOR = '\\';
#else
	char SEPARATOR = '/';
#endif

NaClSandbox* sandbox;

unsigned __attribute__ ((noinline)) unsandboxedLocalMath(unsigned a, unsigned  b, unsigned c)
{
	unsigned ret;
	ret = (a*100) + (b * 10) + (c);
	return ret;
}


unsigned sandboxedLocalMath(unsigned a, unsigned b, unsigned c)
{
  NaClSandbox_Thread* threadData = preFunctionCall(sandbox, sizeof(a) + sizeof(b) + sizeof(c), 0);

  PUSH_VAL_TO_STACK(threadData, unsigned, a);
  PUSH_VAL_TO_STACK(threadData, unsigned, b);
  PUSH_VAL_TO_STACK(threadData, unsigned, c);

  invokeFunctionCallWithSandboxPtr(threadData, (uintptr_t)sandbox->sharedState->test_localMathPtr);

  return (unsigned)functionCallReturnRawPrimitiveInt(threadData);
}

char* getExecFolder(const char* executablePath);
char* concatenateAndFixSlash(const char* string1, const char* string2);


int main(int argc, char** argv)
{
	/**************** Some calculations of relative paths ****************/
	char* execFolder;
	char* libraryPath;
	char* sandbox_init_app;

	if(argc < 1)
	{
		printf("Argv not filled correctly");
		return 1;
	}

	//exec folder is something like: "native_client/src/trusted/dyn_ldr/benchmark/"
	execFolder = getExecFolder(argv[0]);

	#if defined(_M_IX86) || defined(__i386__)
		//libraryPath is something like: "/home/shr/Code/nacl2/native_client/toolchain/linux_x86/nacl_x86_glibc/x86_64-nacl/lib32/"
		libraryPath = concatenateAndFixSlash(execFolder, "../../../../toolchain/linux_x86/nacl_x86_glibc/x86_64-nacl/lib32/");
		//sandbox_init_app is something like: "/home/shr/Code/nacl2/native_client/scons-out/nacl-x86-32-glibc/staging/dyn_ldr_sandbox_init.nexe"
		sandbox_init_app = concatenateAndFixSlash(execFolder, "../../../../scons-out/nacl-x86-32-glibc/staging/dyn_ldr_sandbox_init.nexe");
	#else
		#error Unknown platform!
	#endif

	printf("libraryPath: %s\n", libraryPath);
	printf("sandbox_init_app: %s\n", sandbox_init_app);

	/**************** Actual sandbox with dynamic lib test ****************/

	printf("Starting Dyn loader Benchmark\n");

	if(!initializeDlSandboxCreator(0 /* Should enable detailed logging */))
	{
		printf("Dyn loader Benchmark: initializeDlSandboxCreator returned null\n");
		return 1;
	}

	sandbox = createDlSandbox(libraryPath, sandbox_init_app);

	if(sandbox == NULL)
	{
		printf("Dyn loader Benchmark: createDlSandbox returned null\n");
		return 1;
	}

	printf("Sandbox created\n");
	printf("Enter the number of interations to test: ");

	int iterations;
	if(!scanf("%d", &iterations))
	{
		printf("Scanf failure\n");
		return 1;
	}

	unsigned ret1;
	high_resolution_clock::time_point functionEnterTime = high_resolution_clock::now();
	for(unsigned i = 0; i < iterations; i++)
	{
		ret1 = unsandboxedLocalMath(2, 3, 4);
	}
	high_resolution_clock::time_point functionExitTime = high_resolution_clock::now();

	unsigned ret2;
	high_resolution_clock::time_point sandboxEnterTime = high_resolution_clock::now();
	for(unsigned i = 0; i < iterations; i++)
	{
		ret2 = sandboxedLocalMath(2, 3, 4);
	}
	
	high_resolution_clock::time_point sandboxExitTime = high_resolution_clock::now();

	if(ret1 != ret2)
	{
		printf("Return values don't agree\n");
		return 1;
	}

	long long timeSpentInFunc    = duration_cast<nanoseconds>(functionExitTime - functionEnterTime).count();
	long long timeSpentInSandbox = duration_cast<nanoseconds>(sandboxExitTime  - sandboxEnterTime ).count();

    printf("Function Call = %10" PRId64 ", Sandbox Time = %10" PRId64 " ns\n", timeSpentInFunc, timeSpentInSandbox);


	/**************** Cleanup ****************/

	free(execFolder);
	free(libraryPath);
	free(sandbox_init_app);

	return 0;
}

/**************** Path Helpers ****************/

int lastIndexOf(const char * s, char target)
{
	 int ret = -1;
	 int curIdx = 0;
	 while(s[curIdx] != '\0')
	 {
	    if (s[curIdx] == target) { ret = curIdx; }
	    curIdx++;
	 }
	 return ret;
}

void replaceChar(char* str, char toReplace, char replaceWith)
{
	if(toReplace == replaceWith)
	{
		return;
	}

	while(*str != '\0')
	{
		if(*str == toReplace) { *str = replaceWith; }
		str++;
	}
}

char* getExecFolder(const char* executablePath)
{
	int index;
	char* execFolder;

	index = lastIndexOf(executablePath, SEPARATOR);

	if(index < 0)
	{
		execFolder = (char*)malloc(4);
		execFolder[0] = '.';
		execFolder[1] = SEPARATOR;
		execFolder[2] = '\0';
	}
	else
	{
		size_t len = strlen(executablePath);
		execFolder = (char*)malloc(len + 2);
		strcpy(execFolder, executablePath);

		if((size_t)index < len)
		{
			execFolder[index + 1] = '\0';
		}
	}

	return execFolder;
}

char* concatenateAndFixSlash(const char* string1, const char* string2)
{
	char* ret;

	ret = (char*)malloc(strlen(string1) + strlen(string2) + 2);
	strcpy(ret, string1);
	strcat(ret, string2);

	replaceChar(ret, '/', SEPARATOR);
	return ret;
}