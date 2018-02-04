#include "native_client/src/trusted/dyn_ldr/dyn_ldr_lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>
#include <chrono>
using namespace std::chrono;

#if defined(_WIN32)
	char SEPARATOR = '\\';
#else
	char SEPARATOR = '/';
#endif

NaClSandbox* sandbox;
uintptr_t test_localMathPtr;

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

  invokeFunctionCallWithSandboxPtr(threadData, test_localMathPtr);

  return (unsigned)functionCallReturnRawPrimitiveInt(threadData);
}

char* getExecFolder(const char* executablePath);
char* concatenateAndFixSlash(const char* string1, const char* string2);


int main(int argc, char** argv)
{
	/**************** Some calculations of relative paths ****************/
	char* execFolder;
	char* libraryPath;
	char* libraryToLoad;

	if(argc < 1)
	{
		printf("Argv not filled correctly");
		return 1;
	}

	//exec folder is something like: "native_client/src/trusted/dyn_ldr/benchmark/"
	execFolder = getExecFolder(argv[0]);

	#if defined(_M_IX86) || defined(__i386__)
		//libraryPath is something like: "/home/shr/Code/nacl2/native_client/scons-out/nacl_irt-x86-32/staging/irt_core.nexe"
		libraryPath = concatenateAndFixSlash(execFolder, "../../../../scons-out/nacl_irt-x86-32/staging/irt_core.nexe");
		//libraryToLoad is something like: "/home/shr/Code/nacl2/native_client/scons-out/nacl-x86-32/staging/test_dyn_lib.nexe"
		libraryToLoad = concatenateAndFixSlash(execFolder, "../../../../scons-out/nacl-x86-32/staging/test_dyn_lib.nexe");
	#elif defined(_M_X64) || defined(__x86_64__)
		//libraryPath is something like: "/home/shr/Code/nacl2/native_client/scons-out/nacl_irt-x86-64/staging/irt_core.nexe"
		libraryPath = concatenateAndFixSlash(execFolder, "../../../../scons-out/nacl_irt-x86-64/staging/irt_core.nexe");
		//libraryToLoad is something like: "/home/shr/Code/nacl2/native_client/scons-out/nacl-x86-64/staging/test_dyn_lib.nexe"
		libraryToLoad = concatenateAndFixSlash(execFolder, "../../../../scons-out/nacl-x86-64/staging/test_dyn_lib.nexe");
	#else
		#error Unknown platform!
	#endif

	printf("libraryPath: %s\n", libraryPath);
	printf("libraryToLoad: %s\n", libraryToLoad);

	/**************** Actual sandbox with dynamic lib test ****************/

	printf("Starting Dyn loader Benchmark\n");

	if(!initializeDlSandboxCreator(0 /* Should enable detailed logging */))
	{
		printf("Dyn loader Benchmark: initializeDlSandboxCreator returned null\n");
		return 1;
	}

	sandbox = createDlSandbox(libraryPath, libraryToLoad);

	if(sandbox == NULL)
	{
		printf("Dyn loader Benchmark: createDlSandbox returned null\n");
		return 1;
	}

	test_localMathPtr = getSandboxedAddress(sandbox, (uintptr_t) symbolTableLookupInSandbox(sandbox, "test_localMath"));

	printf("Sandbox created\n");

	unsigned ret1;
	uint64_t timeSpentInFunc;

	unsigned ret2;
	uint64_t timeSpentInSandbox;

	srand(time(NULL));
	unsigned val0_1;
	unsigned val0_2;
	unsigned val0_3;
	unsigned val1_1;
	unsigned val1_2;
	unsigned val1_3;
	unsigned val2_1;
	unsigned val2_2;
	unsigned val2_3;
	unsigned val3_1;
	unsigned val3_2;
	unsigned val3_3;
	unsigned val4_1;
	unsigned val4_2;
	unsigned val4_3;
	unsigned val5_1;
	unsigned val5_2;
	unsigned val5_3;

	val0_1 = rand();
	val0_2 = rand();
	val0_3 = rand();
	val1_1 = rand();
	val1_2 = rand();
	val1_3 = rand();
	val2_1 = rand();
	val2_2 = rand();
	val2_3 = rand();
	val3_1 = rand();
	val3_2 = rand();
	val3_3 = rand();
	val4_1 = rand();
	val4_2 = rand();
	val4_3 = rand();
	val5_1 = rand();
	val5_2 = rand();
	val5_3 = rand();

	ret1 = 0;
	timeSpentInFunc = 0;
	ret2 = 0;
	timeSpentInSandbox = 0;

	{
		//some warm up rounds
		high_resolution_clock::time_point enterTime = high_resolution_clock::now();
		ret1 += unsandboxedLocalMath(val0_1, val0_2, val0_3);
		ret2 += sandboxedLocalMath(val0_1, val0_2, val0_3);
		ret1 += unsandboxedLocalMath(val0_1, val0_2, val0_3);
		ret2 += sandboxedLocalMath(val0_1, val0_2, val0_3);
		ret1 += unsandboxedLocalMath(val0_1, val0_2, val0_3);
		ret2 += sandboxedLocalMath(val0_1, val0_2, val0_3);
		high_resolution_clock::time_point exitTime = high_resolution_clock::now();
		printf("Warm up for = %10" PRId64 " ns\n", duration_cast<nanoseconds>(exitTime - enterTime).count());
	}

	{
		high_resolution_clock::time_point enterTime = high_resolution_clock::now();
		ret1 += unsandboxedLocalMath(val1_1, val1_2, val1_3);
		high_resolution_clock::time_point exitTime = high_resolution_clock::now();
		timeSpentInFunc += duration_cast<nanoseconds>(exitTime - enterTime).count();
	}

	{
		high_resolution_clock::time_point enterTime = high_resolution_clock::now();
		ret2 += sandboxedLocalMath(val1_1, val1_2, val1_3);
		high_resolution_clock::time_point exitTime = high_resolution_clock::now();
		timeSpentInSandbox += duration_cast<nanoseconds>(exitTime  - enterTime).count();
	}

	if(ret1 != ret2)
	{
		printf("Return values don't agree\n");
		return 1;
	}

	printf("1 Function Call = %10" PRId64 ", Sandbox Time = %10" PRId64 " ns\n", timeSpentInFunc, timeSpentInSandbox);

	///////////////////////////////////////////////////////////////////////////////////////////////////////

	val0_1 = rand();
	val0_2 = rand();
	val0_3 = rand();
	val1_1 = rand();
	val1_2 = rand();
	val1_3 = rand();
	val2_1 = rand();
	val2_2 = rand();
	val2_3 = rand();
	val3_1 = rand();
	val3_2 = rand();
	val3_3 = rand();
	val4_1 = rand();
	val4_2 = rand();
	val4_3 = rand();
	val5_1 = rand();
	val5_2 = rand();
	val5_3 = rand();

	ret1 = 0;
	timeSpentInFunc = 0;
	ret2 = 0;
	timeSpentInSandbox = 0;


	{
		high_resolution_clock::time_point enterTime = high_resolution_clock::now();
		ret1 += unsandboxedLocalMath(val1_1, val1_2, val1_3);
		ret1 += unsandboxedLocalMath(val2_1, val2_2, val2_3);
		high_resolution_clock::time_point exitTime = high_resolution_clock::now();
		timeSpentInFunc += duration_cast<nanoseconds>(exitTime - enterTime).count();
	}
	{
		high_resolution_clock::time_point enterTime = high_resolution_clock::now();
		ret2 += sandboxedLocalMath(val1_1, val1_2, val1_3);
		ret2 += sandboxedLocalMath(val2_1, val2_2, val2_3);
		high_resolution_clock::time_point exitTime = high_resolution_clock::now();
		timeSpentInSandbox += duration_cast<nanoseconds>(exitTime  - enterTime).count();
	}

	if(ret1 != ret2)
	{
		printf("Return values don't agree\n");
		return 1;
	}

	printf("2 Function Call = %10" PRId64 ", Sandbox Time = %10" PRId64 " ns\n", timeSpentInFunc, timeSpentInSandbox);

	///////////////////////////////////////////////////////////////////////////////////////////////////////

	val0_1 = rand();
	val0_2 = rand();
	val0_3 = rand();
	val1_1 = rand();
	val1_2 = rand();
	val1_3 = rand();
	val2_1 = rand();
	val2_2 = rand();
	val2_3 = rand();
	val3_1 = rand();
	val3_2 = rand();
	val3_3 = rand();
	val4_1 = rand();
	val4_2 = rand();
	val4_3 = rand();
	val5_1 = rand();
	val5_2 = rand();
	val5_3 = rand();

	ret1 = 0;
	timeSpentInFunc = 0;
	ret2 = 0;
	timeSpentInSandbox = 0;


	{
		high_resolution_clock::time_point enterTime = high_resolution_clock::now();
		ret1 += unsandboxedLocalMath(val1_1, val1_2, val1_3);
		ret1 += unsandboxedLocalMath(val2_1, val2_2, val2_3);
		ret1 += unsandboxedLocalMath(val3_1, val3_2, val3_3);
		high_resolution_clock::time_point exitTime = high_resolution_clock::now();
		timeSpentInFunc += duration_cast<nanoseconds>(exitTime - enterTime).count();
	}

	{
		high_resolution_clock::time_point enterTime = high_resolution_clock::now();
		ret2 += sandboxedLocalMath(val1_1, val1_2, val1_3);
		ret2 += sandboxedLocalMath(val2_1, val2_2, val2_3);
		ret2 += sandboxedLocalMath(val3_1, val3_2, val3_3);
		high_resolution_clock::time_point exitTime = high_resolution_clock::now();
		timeSpentInSandbox += duration_cast<nanoseconds>(exitTime  - enterTime).count();
	}

	if(ret1 != ret2)
	{
		printf("Return values don't agree\n");
		return 1;
	}

	printf("3 Function Call = %10" PRId64 ", Sandbox Time = %10" PRId64 " ns\n", timeSpentInFunc, timeSpentInSandbox);

	///////////////////////////////////////////////////////////////////////////////////////////////////////

	val0_1 = rand();
	val0_2 = rand();
	val0_3 = rand();
	val1_1 = rand();
	val1_2 = rand();
	val1_3 = rand();
	val2_1 = rand();
	val2_2 = rand();
	val2_3 = rand();
	val3_1 = rand();
	val3_2 = rand();
	val3_3 = rand();
	val4_1 = rand();
	val4_2 = rand();
	val4_3 = rand();
	val5_1 = rand();
	val5_2 = rand();
	val5_3 = rand();

	ret1 = 0;
	timeSpentInFunc = 0;
	ret2 = 0;
	timeSpentInSandbox = 0;


	{
		high_resolution_clock::time_point enterTime = high_resolution_clock::now();
		ret1 += unsandboxedLocalMath(val1_1, val1_2, val1_3);
		ret1 += unsandboxedLocalMath(val2_1, val2_2, val2_3);
		ret1 += unsandboxedLocalMath(val3_1, val3_2, val3_3);
		ret1 += unsandboxedLocalMath(val4_1, val4_2, val4_3);
		high_resolution_clock::time_point exitTime = high_resolution_clock::now();
		timeSpentInFunc += duration_cast<nanoseconds>(exitTime - enterTime).count();
	}

	{
		high_resolution_clock::time_point enterTime = high_resolution_clock::now();
		ret2 += sandboxedLocalMath(val1_1, val1_2, val1_3);
		ret2 += sandboxedLocalMath(val2_1, val2_2, val2_3);
		ret2 += sandboxedLocalMath(val3_1, val3_2, val3_3);
		ret2 += sandboxedLocalMath(val4_1, val4_2, val4_3);
		high_resolution_clock::time_point exitTime = high_resolution_clock::now();
		timeSpentInSandbox += duration_cast<nanoseconds>(exitTime  - enterTime).count();
	}

	if(ret1 != ret2)
	{
		printf("Return values don't agree\n");
		return 1;
	}

	printf("4 Function Call = %10" PRId64 ", Sandbox Time = %10" PRId64 " ns\n", timeSpentInFunc, timeSpentInSandbox);

	///////////////////////////////////////////////////////////////////////////////////////////////////////

	val0_1 = rand();
	val0_2 = rand();
	val0_3 = rand();
	val1_1 = rand();
	val1_2 = rand();
	val1_3 = rand();
	val2_1 = rand();
	val2_2 = rand();
	val2_3 = rand();
	val3_1 = rand();
	val3_2 = rand();
	val3_3 = rand();
	val4_1 = rand();
	val4_2 = rand();
	val4_3 = rand();
	val5_1 = rand();
	val5_2 = rand();
	val5_3 = rand();

	ret1 = 0;
	timeSpentInFunc = 0;
	ret2 = 0;
	timeSpentInSandbox = 0;


	{
		high_resolution_clock::time_point enterTime = high_resolution_clock::now();
		ret1 += unsandboxedLocalMath(val1_1, val1_2, val1_3);
		ret1 += unsandboxedLocalMath(val2_1, val2_2, val2_3);
		ret1 += unsandboxedLocalMath(val3_1, val3_2, val3_3);
		ret1 += unsandboxedLocalMath(val4_1, val4_2, val4_3);
		ret1 += unsandboxedLocalMath(val5_1, val5_2, val5_3);
		high_resolution_clock::time_point exitTime = high_resolution_clock::now();
		timeSpentInFunc += duration_cast<nanoseconds>(exitTime - enterTime).count();
	}

	{
		high_resolution_clock::time_point enterTime = high_resolution_clock::now();
		ret2 += sandboxedLocalMath(val1_1, val1_2, val1_3);
		ret2 += sandboxedLocalMath(val2_1, val2_2, val2_3);
		ret2 += sandboxedLocalMath(val3_1, val3_2, val3_3);
		ret2 += sandboxedLocalMath(val4_1, val4_2, val4_3);
		ret2 += sandboxedLocalMath(val5_1, val5_2, val5_3);
		high_resolution_clock::time_point exitTime = high_resolution_clock::now();
		timeSpentInSandbox += duration_cast<nanoseconds>(exitTime  - enterTime).count();
	}

	if(ret1 != ret2)
	{
		printf("Return values don't agree\n");
		return 1;
	}

	printf("5 Function Call = %10" PRId64 ", Sandbox Time = %10" PRId64 " ns\n", timeSpentInFunc, timeSpentInSandbox);

	///////////////////////////////////////////////////////////////////////////////////////////////////////

	/**************** Cleanup ****************/

	free(execFolder);
	free(libraryPath);
	free(libraryToLoad);

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