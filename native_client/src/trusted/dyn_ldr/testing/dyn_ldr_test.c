#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <stdlib.h>
#include "native_client/src/trusted/dyn_ldr/dyn_ldr_lib.h"

#if defined(_WIN32)
	char SEPARATOR = '\\';
	#include <process.h>
#else
	char SEPARATOR = '/';
	#include <pthread.h>
#endif

/**************** Dynamic Library function stubs ****************/
//Some functions that help invoke the functions in dynamic library generated from test_dyn_lib.c

int invokeSimpleAddTest(NaClSandbox* sandbox, void* simpleAddTestPtr, int a, int b)
{
	NaClSandbox_Thread* threadData = preFunctionCall(sandbox, sizeof(a) + sizeof(b), 0 /* size of any arrays being pushed on the stack */);

	PUSH_VAL_TO_STACK(threadData, int, a);
	PUSH_VAL_TO_STACK(threadData, int, b);

	invokeFunctionCall(threadData, simpleAddTestPtr);

	return (int)functionCallReturnRawPrimitiveInt(threadData);
}

//////////////////////////////////////////////////////////////////

size_t invokeSimpleStrLenTestWithStackString(NaClSandbox* sandbox, void* simpleStrLenTestPtr, char* str)
{
	NaClSandbox_Thread* threadData = preFunctionCall(sandbox, sizeof(str), STRING_SIZE(str));

	PUSH_STRING_TO_STACK(threadData, str);

	invokeFunctionCall(threadData, simpleStrLenTestPtr);

	return (size_t)functionCallReturnRawPrimitiveInt(threadData);
}

//////////////////////////////////////////////////////////////////

size_t invokeSimpleStrLenTestWithHeapString(NaClSandbox* sandbox, void* simpleStrLenTestPtr, char* str)
{
	char* strInSandbox;
	size_t ret;
	NaClSandbox_Thread* threadData;

	strInSandbox = (char*) mallocInSandbox(sandbox, strlen(str) + 1);
	strcpy(strInSandbox, str);

	threadData = preFunctionCall(sandbox, sizeof(strInSandbox), 0 /* size of any arrays being pushed on the stack */);

	PUSH_PTR_TO_STACK(threadData, char*, strInSandbox);

	invokeFunctionCall(threadData, simpleStrLenTestPtr);

	ret = (size_t)functionCallReturnRawPrimitiveInt(threadData);

	freeInSandbox(sandbox, strInSandbox);

	return ret;
}

//////////////////////////////////////////////////////////////////

int strLenWithin(char* a, unsigned lenLimit)
{
	if(a != NULL)
	{
		for(unsigned i = 0; i < lenLimit; i++)
		{
			if(a[i] == '\0')
			{
				return 1;
			}
		}
	}

	return 0;
}

int invokeSimpleCallbackTest_callback(unsigned a, char* b)
{
	return a + strlen(b);
}

SANDBOX_CALLBACK unsigned invokeSimpleCallbackTest_callbackStub(uintptr_t sandboxPtr)
{
	int a;
	char* b;
	NaClSandbox* sandbox = (NaClSandbox*) sandboxPtr;
	NaClSandbox_Thread* threadData = callbackParamsBegin(sandbox);

	a = COMPLETELY_UNTRUSTED_CALLBACK_STACK_PARAM(threadData, int);
	b = COMPLETELY_UNTRUSTED_CALLBACK_PTR_PARAM(threadData, char*);

	//We should not assume anything about a, b
	//b could be a pointer to garbage instead of a null terminated string
	//calling any string function on b, such as strlen or others could have unpredictable results
	//we should have some verifications step here
	//maybe something like 
	// - is a between 0 and 100 or whatever range makes sense
	// - does b have a null character in the first 100 characters etc.
	//These validations will most likely have to be domain specific
	if(a < 100 && strLenWithin(b, 100))
	{
		//make a call back into the sandbox to test the ping-pong effect (function
		//  call into sandbox, callback to outside, another function call back in etc.)
		int ret;
		void* ptr = mallocInSandbox(sandbox, sizeof(int));
		ret = invokeSimpleCallbackTest_callback(a, b);
		freeInSandbox(sandbox, ptr);
		return ret;
	}
	else
	{
		//Validation failed
		return 0;
	}	
}

int invokeSimpleCallbackTest(NaClSandbox* sandbox, void* simpleCallbackTestPtr, unsigned a, char* b)
{
	int ret;
	short slotNumber = 0;
	NaClSandbox_Thread* threadData;

	//Note will return NULL if given a slot number greater than getTotalNumberOfCallbackSlots(), a valid ptr if it succeeds
	uintptr_t callback = registerSandboxCallback(sandbox, slotNumber, (uintptr_t) invokeSimpleCallbackTest_callbackStub);

	threadData = preFunctionCall(sandbox, sizeof(a) + sizeof(b) + sizeof(callback), 0 /* size of any arrays being pushed on the stack */);

	PUSH_VAL_TO_STACK(threadData, unsigned, a);
	PUSH_STRING_TO_STACK(threadData, b);
	PUSH_VAL_TO_STACK(threadData, uintptr_t, callback);

	invokeFunctionCall(threadData, simpleCallbackTestPtr);

	ret = (int) functionCallReturnRawPrimitiveInt(threadData);

	//Best to unregister after it is done
	//In an adversarial setting, the sandboxed app may decide to invoke the callback
	//arbitrarily in the future, which may allow it to destabilize the hosting app
	//Note will return 0 if given a slot number greater than getTotalNumberOfCallbackSlots(), 1 if it succeeds
	unregisterSandboxCallback(sandbox, slotNumber);
	return ret;
}

//////////////////////////////////////////////////////////////////

int invokeSimpleWriteToFileTest(NaClSandbox* sandbox, void* simpleWriteToFileTest, FILE* file, char* str)
{
	NaClSandbox_Thread* threadData = preFunctionCall(sandbox, sizeof(FILE*) + sizeof(str), STRING_SIZE(str));

	PUSH_PTR_TO_STACK(threadData, FILE*, file);
	PUSH_STRING_TO_STACK(threadData, str);

	invokeFunctionCall(threadData, simpleWriteToFileTest);

	return (int)functionCallReturnRawPrimitiveInt(threadData);
}

//////////////////////////////////////////////////////////////////

int fileTestPassed(NaClSandbox* sandbox, void* simpleWriteToFileTest)
{
	FILE* file = fopenInSandbox(sandbox, "/home/shr/Desktop/test123.txt", "w");

	if(!file)
	{
		printf("File open failed\n");
		return 0;
	}

	invokeSimpleWriteToFileTest(sandbox, simpleWriteToFileTest, file, "test123");

	if(fcloseInSandbox(sandbox, file))
	{
		printf("File close failed\n");
		return 0;
	}

	return 1;
}

//////////////////////////////////////////////////////////////////

int invokeSimpleEchoTestPassed(NaClSandbox* sandbox, void* simpleEchoTestPtr, char* str)
{
	char* strInSandbox;
	char* retStr;
	int ret;
	NaClSandbox_Thread* threadData;

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

	threadData = preFunctionCall(sandbox, sizeof(strInSandbox), 0 /* size of any arrays being pushed on the stack */);

	PUSH_PTR_TO_STACK(threadData, char*, strInSandbox);

	invokeFunctionCall(threadData, simpleEchoTestPtr);

	retStr = (char*)functionCallReturnPtr(threadData);

	//retStr is allocated in sandbox heap, not our heap
	if(!isAddressInSandboxMemoryOrNull(sandbox, (uintptr_t) retStr))
	{
		return 0;
	}

	ret = strcmp(str, retStr) == 0;

	freeInSandbox(sandbox, strInSandbox);

	return ret;
}

/**************** Main function ****************/

char* getExecFolder(char* executablePath);
char* concatenateAndFixSlash(char* string1, char* string2);

NaClSandbox* sandbox;
void* simpleAddTestSymResult;
void* simpleStrLenTestResult;
void* simpleCallbackTestResult;
void* simpleWriteToFileTestResult;
void* simpleEchoTestResult;
int testResult = -1;

void* runTests(void* unusedParam)
{
	if(unusedParam){}
	testResult = -1;

	if(invokeSimpleAddTest(sandbox, simpleAddTestSymResult, 2, 3) != 5)
	{
		printf("Dyn loader Test 1: Failed\n");
		testResult = 0;
		return NULL;
	}

	if(invokeSimpleStrLenTestWithStackString(sandbox, simpleStrLenTestResult, "Hello") != 5)
	{
		printf("Dyn loader Test 2: Failed\n");
		testResult = 0;
		return NULL;
	}

	if(invokeSimpleStrLenTestWithStackString(sandbox, simpleStrLenTestResult, "Hello") != 5)
	{
		printf("Dyn loader Test 3: Failed\n");
		testResult = 0;
		return NULL;
	}

	if(invokeSimpleCallbackTest(sandbox, simpleCallbackTestResult, 4, "Hello") != 9)
	{
		printf("Dyn loader Test 4: Failed\n");
		testResult = 0;
		return NULL;
	}

	if(!fileTestPassed(sandbox, simpleWriteToFileTestResult))
	{
		printf("Dyn loader Test 5: Failed\n");
		testResult = 0;
		return NULL;	
	}

	if(!invokeSimpleEchoTestPassed(sandbox, simpleEchoTestResult, "Hello"))
	{
		printf("Dyn loader Test 6: Failed\n");
		testResult = 0;
		return NULL;	
	}

	testResult = 1;
	return NULL;
}

int main(int argc, char** argv)
{
	void* dlHandle;

	/**************** Some calculations of relative paths ****************/
	char* execFolder;
	char* libraryPath;
	char* sandbox_init_app;
	char* dlToOpen;

	if(argc < 1)
	{
		printf("Argv not filled correctly");
		return 1;
	}

	//exec folder is something like: "scons-out/opt-linux-x86-32/staging/"
	execFolder = getExecFolder(argv[0]);

	#if defined(_M_IX86) || defined(__i386__)
		//libraryPath is something like: "/home/shr/Code/nacl2/native_client/toolchain/linux_x86/nacl_x86_glibc/x86_64-nacl/lib32/"
		libraryPath = concatenateAndFixSlash(execFolder, "../../../toolchain/linux_x86/nacl_x86_glibc/x86_64-nacl/lib32/");
		//sandbox_init_app is something like: "/home/shr/Code/nacl2/native_client/scons-out/nacl-x86-32-glibc/staging/dyn_ldr_sandbox_init.nexe"
		sandbox_init_app = concatenateAndFixSlash(execFolder, "../../../scons-out/nacl-x86-32-glibc/staging/dyn_ldr_sandbox_init.nexe");
		//dlToOpen is something like: "/home/shr/Code/nacl2/native_client/scons-out/nacl-x86-32-glibc/staging/libtest_dyn_lib.so"
		dlToOpen = concatenateAndFixSlash(execFolder, "../../../scons-out/nacl-x86-32-glibc/staging/libtest_dyn_lib.so");
	#else
		#error Unknown platform!
	#endif

	printf("libraryPath: %s\n", libraryPath);
	printf("sandbox_init_app: %s\n", sandbox_init_app);
	printf("dlToOpen: %s\n", dlToOpen);

	/**************** Actual sandbox with dynamic lib test ****************/

	printf("Starting Dyn loader Test\n");

	if(!initializeDlSandboxCreator(0 /* Should enable detailed logging */))
	{
		printf("Dyn loader Test: initializeDlSandboxCreator returned null\n");
		return 1;
	}

	sandbox = createDlSandbox(libraryPath, sandbox_init_app);

	if(sandbox == NULL)
	{
		printf("Dyn loader Test: createDlSandbox returned null\n");
		return 1;
	}

	printf("Sandbox created\n");

	dlHandle = dlopenInSandbox(sandbox, dlToOpen, RTLD_LAZY);

	if(dlHandle == NULL)
	{
		char *err = dlerrorInSandbox(sandbox); 
		printf("Dyn loader Test: dlopen returned null. Got err msg: %s\n", err);
		return 1;
	}

	printf("dlopen successful: %p\n", dlHandle);

	simpleAddTestSymResult      = dlsymInSandbox(sandbox, dlHandle, "simpleAddTest");
	simpleStrLenTestResult      = dlsymInSandbox(sandbox, dlHandle, "simpleStrLenTest");
	simpleCallbackTestResult    = dlsymInSandbox(sandbox, dlHandle, "simpleCallbackTest");
	simpleWriteToFileTestResult = dlsymInSandbox(sandbox, dlHandle, "simpleWriteToFileTest");
	simpleEchoTestResult        = dlsymInSandbox(sandbox, dlHandle, "simpleEchoTest");

	printf("dlsym successful\n");

	if(simpleAddTestSymResult == NULL 
		|| simpleStrLenTestResult == NULL 
		|| simpleCallbackTestResult == NULL
		|| simpleWriteToFileTestResult == NULL
		|| simpleEchoTestResult == NULL)
	{
		printf("Dyn loader Test: dlSym returned null\n");
		return 1;
	}

	/**************** Invoking functions in sandbox ****************/

	runTests(NULL);
	if(testResult != 1)
	{
		printf("Main thread tests failed\n");
		return 1;
	}

	printf("Main thread tests successful\n");

	#if defined(_WIN32)
	{ 
		unsigned threadID;  
		HANDLE newThread = (HANDLE) _beginthreadex(NULL /* security */, 0 /* use default stack size */, runTests, NULL /* no parameter */, 0/* initflag */, &threadID);

		if(!newThread)
		{
			printf("Error creating thread\n");
			return 1;	
		}

	    DWORD result = WaitForSingleObject(newThread, INFINITE);
		if (result != WAIT_OBJECT_0) 
		{
			printf("Error joining thread\n");
			return 1;
		}

		CloseHandle(newThread);
	}
	#else
	{
		pthread_t newThread;
		
		if(pthread_create(&newThread, NULL /* use default thread attributes */, runTests, NULL /* no parameter */))
		{
			printf("Error creating thread\n");
			return 1;
		}

		if(pthread_join(newThread, NULL))
		{
			printf("Error joining thread\n");
			return 1;
		}
	}
	#endif

	if(testResult != 1)
	{
		printf("Secondary thread tests failed\n");
		return 1;
	}

	printf("Secondary thread tests successful\n");

	printf("Dyn loader Test Succeeded\n");

	/**************** Cleanup ****************/

	free(execFolder);
	free(libraryPath);
	free(sandbox_init_app);
	free(dlToOpen);

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

char* concatenateAndFixSlash(char* string1, char* string2)
{
	char* ret;

	ret = (char*)malloc(strlen(string1) + strlen(string2) + 2);
	strcpy(ret, string1);
	strcat(ret, string2);

	replaceChar(ret, '/', SEPARATOR);
	return ret;
}