#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "native_client/src/trusted/dyn_ldr/dyn_ldr_lib.h"
#include "native_client/src/trusted/dyn_ldr/nacl_sandbox.h"
#include "native_client/src/trusted/dyn_ldr/testing/test_dyn_lib.h"

char SEPARATOR = '/';
#include <pthread.h>

int invokeSimpleCallbackTest_callback(unsigned a, const char* b)
{
	return a + strlen(b);
}

//////////////////////////////////////////////////////////////////

int fileTestPassed(NaClSandbox* sandbox, void* simpleWriteToFileTestPtr)
{
	FILE* file = fopenInSandbox(sandbox, "test123.txt", "w");

	if(!file)
	{
		printf("File open failed\n");
		return 0;
	}

	sandbox_invoke(sandbox, simpleWriteToFileTest, simpleWriteToFileTestPtr, file, sandbox_stackarr("test123"));

	if(fcloseInSandbox(sandbox, file))
	{
		printf("File close failed\n");
		return 0;
	}

	return 1;
}

//////////////////////////////////////////////////////////////////

int invokeSimpleEchoTestPassed(NaClSandbox* sandbox, void* simpleEchoTestPtr, const char* str)
{
	char* strInSandbox;
	char* retStr;
	int ret;

	//str is allocated in our heap, not the sandbox's heap
	if(!isAddressInNonSandboxMemoryOrNull(sandbox, (uintptr_t) str))
	{
		return 0;
	}

	strInSandbox = (char*) mallocInSandbox(sandbox, strlen(str) + 1);
	strcpy(strInSandbox, str);

	//str is allocated in sandbox heap, not our heap
	if(!isAddressInSandboxMemoryOrNull(sandbox, (uintptr_t) strInSandbox))
	{
		return 0;
	}

	retStr = sandbox_invoke(sandbox, simpleEchoTest, simpleEchoTestPtr, strInSandbox);

	//retStr is allocated in sandbox heap, not our heap
	if(!isAddressInSandboxMemoryOrNull(sandbox, (uintptr_t) retStr))
	{
		return 0;
	}

	ret = strcmp(str, retStr) == 0;

	freeInSandbox(sandbox, strInSandbox);

	return ret;
}

//////////////////////////////////////////////////////////////////

/**************** Main function ****************/

char* getExecFolder(char* executablePath);
char* concatenateAndFixSlash(const char* string1, const char* string2);

struct runTestParams
{
	NaClSandbox* sandbox;
	int testResult;
	void* simpleAddTestSymResult;
	void* simpleStrLenTestResult;
	void* simpleCallbackTestResult;
	void* simpleWriteToFileTestResult;
	void* simpleEchoTestResult;
	void* simpleDoubleAddTestResult;
	void* simpleLongAddTestResult;
	std::shared_ptr<sandbox_callback_helper<int(unsigned int, const char*)>> registeredCallback;

	//for multi threaded test only
	pthread_t newThread;
};


void* runTests(void* runTestParamsPtr)
{
	struct runTestParams* testParams = (struct runTestParams*) runTestParamsPtr;
	NaClSandbox* sandbox = testParams->sandbox;

	int* testResult = &(testParams->testResult);
	*testResult = -1;

	if(sandbox_invoke(sandbox, simpleAddTest, testParams->simpleAddTestSymResult, 2, 3) != 5)
	{
		printf("Dyn loader Test 1: Failed\n");
		*testResult = 0;
		return NULL;
	}

	if(sandbox_invoke(sandbox, simpleStrLenTest, testParams->simpleStrLenTestResult, sandbox_stackarr("Hello")) != 5)
	{
		printf("Dyn loader Test 2: Failed\n");
		*testResult = 0;
		return NULL;
	}

	if(sandbox_invoke(sandbox, simpleStrLenTest, testParams->simpleStrLenTestResult, sandbox_heaparr_sharedptr(sandbox, "Hello")) != 5)
	{
		printf("Dyn loader Test 3: Failed\n");
		*testResult = 0;
		return NULL;
	}

	if(sandbox_invoke(sandbox, simpleCallbackTest, testParams->simpleCallbackTestResult, (unsigned) 4, sandbox_stackarr("Hello"), testParams->registeredCallback) != 10)
	{
		printf("Dyn loader Test 4: Failed\n");
		*testResult = 0;
		return NULL;
	}

	if(!fileTestPassed(sandbox, testParams->simpleWriteToFileTestResult))
	{
		printf("Dyn loader Test 5: Failed\n");
		*testResult = 0;
		return NULL;	
	}

	if(!invokeSimpleEchoTestPassed(sandbox, testParams->simpleEchoTestResult, "Hello"))
	{
		printf("Dyn loader Test 6: Failed\n");
		*testResult = 0;
		return NULL;	
	}

	if(sandbox_invoke(sandbox, simpleDoubleAddTest, testParams->simpleDoubleAddTestResult, 1.0, 2.0) != 3.0)
	{
		printf("Dyn loader Test 7: Failed\n");
		*testResult = 0;
		return NULL;
	}

	if(sandbox_invoke(sandbox, simpleLongAddTest, testParams->simpleLongAddTestResult, ULONG_MAX - 1ul, 1ul) != ULONG_MAX)
	{
		printf("Dyn loader Test 8: Failed\n");
		*testResult = 0;
		return NULL;	
	}

	*testResult = 1;
	return NULL;
}

#define ThreadsToTest 4

void runSingleThreadedTest(struct runTestParams testParams)
{
	runTests((void *) &testParams);
	if(testParams.testResult != 1)
	{
		printf("Main thread tests failed\n");
		exit(1);
	}

	printf("Main thread tests successful\n");
}

void runMultiThreadedTest(struct runTestParams * testParams, unsigned threadCount)
{
	for(unsigned i = 0; i < threadCount; i++)
	{
		if(pthread_create(&(testParams[i].newThread), NULL /* use default thread attributes */, runTests, (void *) &(testParams[i]) /* parameter */))
		{
			printf("Error creating thread %d\n", i);
			exit(1);
		}
	}
}

void checkMultiThreadedTest(struct runTestParams * testParams, unsigned threadCount)
{
	for(unsigned i = 0; i < threadCount; i++)
	{
		if(pthread_join(testParams[i].newThread, NULL))
		{
			printf("Error joining thread %d\n", i);
			exit(1);
		}
	}

	for(unsigned i = 0; i < threadCount; i++)
	{
		if(testParams[i].testResult != 1)
		{
			printf("Secondary thread tests %d failed\n", i);
			exit(1);
		}

		printf("Secondary thread tests %d successful\n", i);
	}
}

int main(int argc, char** argv)
{
	/**************** Some calculations of relative paths ****************/
	char* execFolder;
	char* libraryPath;
	char* libraryToLoad;
	struct runTestParams sandboxParams[2];

	if(argc < 1)
	{
		printf("Argv not filled correctly");
		return 1;
	}

	//exec folder is something like: "scons-out/opt-linux-x86-32/staging/"
	execFolder = getExecFolder(argv[0]);

	#if defined(_M_IX86) || defined(__i386__)
		//libraryPath is something like: "/home/shr/Code/nacl2/native_client/scons-out/nacl_irt-x86-32/staging/irt_core.nexe"
		libraryPath = concatenateAndFixSlash(execFolder, "../../../scons-out/nacl_irt-x86-32/staging/irt_core.nexe");
		//libraryToLoad is something like: "/home/shr/Code/nacl2/native_client/scons-out/nacl-x86-32/staging/test_dyn_lib.nexe"
		libraryToLoad = concatenateAndFixSlash(execFolder, "../../../scons-out/nacl-x86-32/staging/test_dyn_lib.nexe");
	#elif defined(_M_X64) || defined(__x86_64__)
		//libraryPath is something like: "/home/shr/Code/nacl2/native_client/scons-out/nacl_irt-x86-64/staging/irt_core.nexe"
		libraryPath = concatenateAndFixSlash(execFolder, "../../../scons-out/nacl_irt-x86-64/staging/irt_core.nexe");
		//libraryToLoad is something like: "/home/shr/Code/nacl2/native_client/scons-out/nacl-x86-64/staging/test_dyn_lib.nexe"
		libraryToLoad = concatenateAndFixSlash(execFolder, "../../../scons-out/nacl-x86-64/staging/test_dyn_lib.nexe");
	#else
		#error Unknown platform!
	#endif

	printf("libraryPath: %s\n", libraryPath);
	printf("libraryToLoad: %s\n", libraryToLoad);

	/**************** Actual sandbox with dynamic lib test ****************/

	printf("Starting Dyn loader Test.\n");

	if(!initializeDlSandboxCreator(2 /* Should enable detailed logging */))
	{
		printf("Dyn loader Test: initializeDlSandboxCreator returned null\n");
		return 1;
	}

	for(int i = 0; i < 2; i++)
	{
		sandboxParams[i].sandbox = createDlSandbox(libraryPath, libraryToLoad);

		if(sandboxParams[i].sandbox == NULL)
		{
			printf("Dyn loader Test: createDlSandbox returned null: %d\n", i);
			return 1;
		}

		printf("Sandbox created: %d\n", i);

		sandboxParams[i].simpleAddTestSymResult      = symbolTableLookupInSandbox(sandboxParams[i].sandbox, "simpleAddTest");
		sandboxParams[i].simpleStrLenTestResult      = symbolTableLookupInSandbox(sandboxParams[i].sandbox, "simpleStrLenTest");
		sandboxParams[i].simpleCallbackTestResult    = symbolTableLookupInSandbox(sandboxParams[i].sandbox, "simpleCallbackTest");
		sandboxParams[i].simpleWriteToFileTestResult = symbolTableLookupInSandbox(sandboxParams[i].sandbox, "simpleWriteToFileTest");
		sandboxParams[i].simpleEchoTestResult        = symbolTableLookupInSandbox(sandboxParams[i].sandbox, "simpleEchoTest");
		sandboxParams[i].simpleDoubleAddTestResult   = symbolTableLookupInSandbox(sandboxParams[i].sandbox, "simpleDoubleAddTest");
		sandboxParams[i].simpleLongAddTestResult     = symbolTableLookupInSandbox(sandboxParams[i].sandbox, "simpleLongAddTest");

		printf("symbol lookup successful: %d\n", i);

		if(sandboxParams[i].simpleAddTestSymResult == NULL 
			|| sandboxParams[i].simpleStrLenTestResult == NULL 
			|| sandboxParams[i].simpleCallbackTestResult == NULL
			|| sandboxParams[i].simpleWriteToFileTestResult == NULL
			|| sandboxParams[i].simpleEchoTestResult == NULL
			|| sandboxParams[i].simpleDoubleAddTestResult == NULL
			|| sandboxParams[i].simpleLongAddTestResult == NULL)
		{
			printf("Dyn loader Test: symbol lookup returned null: %d\n", i);
			return 1;
		}

		/**************** Invoking functions in sandbox ****************/

		//Note will return NULL if given a slot number greater than getTotalNumberOfCallbackSlots(), a valid ptr if it succeeds
		sandboxParams[i].registeredCallback = sandbox_callback_sharedptr(sandboxParams[i].sandbox, invokeSimpleCallbackTest_callback);
	}

	for(int i = 0; i < 2; i++)
	{
		runSingleThreadedTest(sandboxParams[i]);
	}

	for(int i = 0; i < 2; i++)
	{
		struct runTestParams threadParams[ThreadsToTest];

		for(int j = 0; j < ThreadsToTest; j++)
		{
			threadParams[j] = sandboxParams[i];
		}

		runMultiThreadedTest(threadParams, ThreadsToTest);
		checkMultiThreadedTest(threadParams, ThreadsToTest);
	}

	//interleaved sandbox thread
	{
		struct runTestParams threadParams1[ThreadsToTest];
		struct runTestParams threadParams2[ThreadsToTest];

		for(int j = 0; j < ThreadsToTest; j++)
		{
			threadParams1[j] = sandboxParams[0];
			threadParams2[j] = sandboxParams[1];
		}

		runMultiThreadedTest(threadParams1, ThreadsToTest);
		runMultiThreadedTest(threadParams2, ThreadsToTest);
		checkMultiThreadedTest(threadParams1, ThreadsToTest);
		checkMultiThreadedTest(threadParams2, ThreadsToTest);
	}

	printf("Dyn loader Test Succeeded\n");

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

char* getExecFolder(char* executablePath)
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