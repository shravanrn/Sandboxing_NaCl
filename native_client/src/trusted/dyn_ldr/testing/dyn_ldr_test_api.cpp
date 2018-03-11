#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "native_client/src/trusted/dyn_ldr/nacl_sandbox.h"
#include "native_client/src/trusted/dyn_ldr/testing/test_dyn_lib.h"


//////////////////////////////////////////////////////////////////

#define sandbox_fields_reflection_exampleId_class_testStruct(f, g) \
	f(unsigned long, fieldLong) \
	g() \
	f(const char*, fieldString) \
	g() \
	f(unsigned int, fieldBool)

#define sandbox_fields_reflection_exampleId_allClasses(f) \
	f(testStruct, exampleId)

sandbox_nacl_load_library_api(exampleId);

//////////////////////////////////////////////////////////////////

char SEPARATOR = '/';
#include <pthread.h>

int invokeSimpleCallbackTest_callback(unsigned a, const char* b)
{
	return a + strlen(b);
}

//////////////////////////////////////////////////////////////////

int fileTestPassed(NaClSandbox* sandbox)
{
	FILE* file = fopenInSandbox(sandbox, "test123.txt", "w");

	if(!file)
	{
		printf("File open failed\n");
		return 0;
	}

	sandbox_invoke(sandbox, simpleWriteToFileTest, file, sandbox_stackarr("test123"));

	if(fcloseInSandbox(sandbox, file))
	{
		printf("File close failed\n");
		return 0;
	}

	return 1;
}

//////////////////////////////////////////////////////////////////

int invokeSimpleEchoTestPassed(NaClSandbox* sandbox, const char* str)
{
	char* strInSandbox;
	char* retStr;
	int ret;

	//str is allocated in our heap, not the sandbox's heap
	if(!isAddressInNonSandboxMemoryOrNull(sandbox, (uintptr_t) str))
	{
		return 0;
	}

	unverified_data<char*> temp = newInSandbox<char>(sandbox, strlen(str) + 1);
	strInSandbox = temp.sandbox_onlyVerifyAddress();
	strcpy(strInSandbox, str);

	//str is allocated in sandbox heap, not our heap
	if(!isAddressInSandboxMemoryOrNull(sandbox, (uintptr_t) strInSandbox))
	{
		return 0;
	}

	auto retStrRaw = sandbox_invoke(sandbox, simpleEchoTest, strInSandbox);
	*retStrRaw = 'g';
	*retStrRaw = 'H';
	retStr = retStrRaw.sandbox_copyAndVerifyString([](char* val) { return strlen(val) < 100; }, nullptr);
	printf("RetStr: %s\n", retStr);

	//retStr.field is a copy on our heap
	if(isAddressInSandboxMemoryOrNull(sandbox, (uintptr_t) retStr))
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


	auto result1 = sandbox_invoke(sandbox, simpleAddTest, 2, 3)
		.sandbox_copyAndVerify([](int val){ return val > 0 && val < 10;}, -1);
	if(result1 != 5)
	{
		printf("Dyn loader Test 1: Failed\n");
		*testResult = 0;
		return NULL;
	}

	auto result2 = sandbox_invoke(sandbox, simpleStrLenTest, sandbox_stackarr("Hello"))
		.sandbox_copyAndVerify([](size_t val) -> size_t { return (val <= 0 || val >= 10)? -1 : val; });
	if(result2 != 5)
	{
		printf("Dyn loader Test 2: Failed %d \n", (int)result2);
		*testResult = 0;
		return NULL;
	}

	auto result3 = sandbox_invoke(sandbox, simpleStrLenTest, sandbox_heaparr_sharedptr(sandbox, "Hello"))
		.sandbox_copyAndVerify([](size_t val) -> size_t { return (val <= 0 || val >= 10)? -1 : val; });
	if(result3 != 5)
	{
		printf("Dyn loader Test 3: Failed\n");
		*testResult = 0;
		return NULL;
	}

	auto result4 = sandbox_invoke(sandbox, simpleCallbackTest, (unsigned) 4, sandbox_stackarr("Hello"), testParams->registeredCallback)
		.sandbox_copyAndVerify([](int val){ return val > 0 && val < 100;}, -1);
	if(result4 != 10)
	{
		printf("Dyn loader Test 4: Failed\n");
		*testResult = 0;
		return NULL;
	}

	if(!fileTestPassed(sandbox))
	{
		printf("Dyn loader Test 5: Failed\n");
		*testResult = 0;
		return NULL;
	}

	if(!invokeSimpleEchoTestPassed(sandbox, "Hello"))
	{
		printf("Dyn loader Test 6: Failed\n");
		*testResult = 0;
		return NULL;
	}

	auto result7 = sandbox_invoke(sandbox, simpleDoubleAddTest, 1.0, 2.0)
		.sandbox_copyAndVerify([](double val){ return val > 0 && val < 100;}, -1.0);
	if(result7 != 3.0)
	{
		printf("Dyn loader Test 7: Failed\n");
		*testResult = 0;
		return NULL;
	}

	auto result8 = sandbox_invoke(sandbox, simpleLongAddTest, ULONG_MAX - 1ul, 1ul)
		.sandbox_copyAndVerify([](unsigned long val){ return val > 100;}, 0);
	if(result8 != ULONG_MAX)
	{
		printf("Dyn loader Test 8: Failed\n");
		*testResult = 0;
		return NULL;
	}

	//////////////////////////////////////////////////////////////////

	auto result9T = sandbox_invoke(sandbox, simpleTestStructVal);
	auto result9 = result9T
		.sandbox_copyAndVerify([](unverified_data<testStruct>& val){ 
			testStruct ret;
			ret.fieldLong = val.fieldLong.sandbox_copyAndVerify([](unsigned long val) { return val; });
			ret.fieldString = val.fieldString.sandbox_copyAndVerifyString([](const char* val) { return strlen(val) < 100; }, nullptr);
			ret.fieldBool = val.fieldBool.sandbox_copyAndVerify([](unsigned int val) { return val; });
			return ret; 
		});
	if(result9.fieldLong != 7 || strcmp(result9.fieldString, "Hello") != 0 || result9.fieldBool != 1)
	{
		printf("Dyn loader Test 9: Failed\n");
		*testResult = 0;
		return NULL;
	}

	//writes should still go through
	result9T.fieldLong = 17;
	long val = result9T.fieldLong.sandbox_copyAndVerify([](unsigned long val) { return val; });
	if(val != 17)
	{
		printf("Dyn loader Test 9.1: Failed\n");
		*testResult = 0;
		return NULL;
	}

	//////////////////////////////////////////////////////////////////
	auto result10T = sandbox_invoke(sandbox, simpleTestStructPtr);
	auto result10 = result10T
		.sandbox_copyAndVerify([](sandbox_unverified_data<testStruct>* val) { 
			testStruct ret;
			ret.fieldLong = val->fieldLong.sandbox_copyAndVerify([](unsigned long val) { return val; });
			ret.fieldString = val->fieldString.sandbox_copyAndVerifyString([](const char* val) { return strlen(val) < 100; }, nullptr);
			ret.fieldBool = val->fieldBool.sandbox_copyAndVerify([](unsigned int val) { return val; });
			return ret; 
		});
	if(result10.fieldLong != 7 || strcmp(result10.fieldString, "Hello") != 0 || result10.fieldBool != 1)
	{
		printf("Dyn loader Test 10: Failed\n");
		*testResult = 0;
		return NULL;
	}

	//writes should still go through
	result10T->fieldLong = 17;
	long val2 = result10T->fieldLong.sandbox_copyAndVerify([](unsigned long val) { return val; });
	if(val2 != 17)
	{
		printf("Dyn loader Test 10.1: Failed\n");
		*testResult = 0;
		return NULL;
	}

	//////////////////////////////////////////////////////////////////
	void* ptr = (void *)(uintptr_t) 0x1234567812345678;
	void* result11 = sandbox_invoke_ret_unsandboxed_ptr(sandbox, echoPointer, sandbox_unsandboxed_ptr(ptr));

	if(result11 != ptr)
	{
		printf("Dyn loader Test 11: Failed\n");
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
		initCPPApi(sandboxParams[i].sandbox);

		if(sandboxParams[i].sandbox == NULL)
		{
			printf("Dyn loader Test: createDlSandbox returned null: %d\n", i);
			return 1;
		}

		printf("Sandbox created: %d\n", i);

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
