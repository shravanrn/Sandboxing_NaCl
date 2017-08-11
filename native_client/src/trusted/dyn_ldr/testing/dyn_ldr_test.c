#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <stdlib.h>
#include "native_client/src/trusted/dyn_ldr/dyn_ldr_lib.h"

#if defined(_WIN32)
	char SEPARATOR = '\\';
#else
	char SEPARATOR = '/';
#endif

/**************** Dynamic Library function stubs ****************/
//Some functions that help invoke the functions in dynamic library generated from test_dyn_lib.c

int invokeSimpleAdd(NaClSandbox* sandbox, void* simpleAddPtr, int a, int b)
{
  preFunctionCall(sandbox, sizeof(a) + sizeof(b), 0 /* size of any arrays being pushed on the stack */);

  PUSH_VAL_TO_STACK(sandbox, int, a);
  PUSH_VAL_TO_STACK(sandbox, int, b);

  invokeFunctionCall(sandbox, simpleAddPtr);

  return (int)functionCallReturnRawPrimitiveInt(sandbox);
}

//////////////////////////////////////////////////////////////////

size_t invokeSimpleStrLenWithStackString(NaClSandbox* sandbox, void* simpleStrLenPtr, char* str)
{
  preFunctionCall(sandbox, sizeof(str), STRING_SIZE(str));

  PUSH_STRING_TO_STACK(sandbox, str);

  invokeFunctionCall(sandbox, simpleStrLenPtr);

  return (size_t)functionCallReturnRawPrimitiveInt(sandbox);
}

//////////////////////////////////////////////////////////////////

size_t invokeSimpleStrLenWithHeapString(NaClSandbox* sandbox, void* simpleStrLenPtr, char* str)
{
  char* strInSandbox;
  size_t ret;

  strInSandbox = (char*) mallocInSandbox(sandbox, strlen(str) + 1);
  strcpy(strInSandbox, str);

  preFunctionCall(sandbox, sizeof(strInSandbox), 0 /* size of any arrays being pushed on the stack */);

  PUSH_PTR_TO_STACK(sandbox, char*, strInSandbox);

  invokeFunctionCall(sandbox, simpleStrLenPtr);

  ret = (size_t)functionCallReturnRawPrimitiveInt(sandbox);

  freeInSandbox(sandbox, strInSandbox);

  return ret;
}

//////////////////////////////////////////////////////////////////

unsigned invokeSimpleCallback_callback(unsigned a, char* b)
{
	return a + strlen(b);
}

SANDBOX_CALLBACK unsigned invokeSimpleCallback_callbackStub(uintptr_t sandboxPtr)
{
	unsigned a;
	char* b;
	NaClSandbox* sandbox = (NaClSandbox*) sandboxPtr;

	a = COMPLETELY_UNTRUSTED_CALLBACK_PARAM(sandbox, unsigned);
	b = COMPLETELY_UNTRUSTED_CALLBACK_PARAM_PTR(sandbox, char*);

	//We should not assume anything about a, b
	//b could be a pointer to garbage instead of a null terminated string
	//calling any string function on b, such as strlen or others could have unpredictable results
	//we should have some verifications step here
	//maybe something like 
	// - is a between 0 and 100 or whatever range makes sense
	// - does b have a null character in the first 100 characters etc.
	//These validations will most likely have to be domain specific
	
	CALLBACK_PARAMS_FINISHED(sandbox);
	
	return invokeSimpleCallback_callback(a, b);
}

unsigned invokeSimpleCallback(NaClSandbox* sandbox, void* simpleCallbackPtr, unsigned a, char* b)
{
  unsigned ret;
  short slotNumber = 0;

  //Note will return NULL if given a slot number greater than getTotalNumberOfCallbackSlots(), a valid ptr if it succeeds
  uintptr_t callback = registerSandboxCallback(sandbox, slotNumber, (uintptr_t) invokeSimpleCallback_callbackStub);

  preFunctionCall(sandbox, sizeof(a) + sizeof(b) + sizeof(callback), 0 /* size of any arrays being pushed on the stack */);

  PUSH_VAL_TO_STACK(sandbox, unsigned, a);
  PUSH_STRING_TO_STACK(sandbox, b);
  PUSH_VAL_TO_STACK(sandbox, uintptr_t, callback);

  invokeFunctionCall(sandbox, simpleCallbackPtr);

  ret = (unsigned) functionCallReturnRawPrimitiveInt(sandbox);

  //Best to unregister after it is done
  //In an adversarial setting, the sandboxed app may decide to invoke the callback
  //arbitrarily in the future, which may allow it to destabilize the hosting app
  //Note will return 0 if given a slot number greater than getTotalNumberOfCallbackSlots(), 1 if it succeeds
  unregisterSandboxCallback(sandbox, slotNumber);
  return ret;
}

//////////////////////////////////////////////////////////////////

/**************** Main function ****************/

char* getExecFolder(char* executablePath);
char* concatenateAndFixSlash(char* string1, char* string2);


int main(int argc, char** argv)
{
	NaClSandbox* sandbox;
	void* dlHandle;
	void* simpleAddSymResult;
	void* simpleStrLenResult;
	void* simpleCallbackResult;

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

	initializeDlSandboxCreator(0 /* Should enable detailed logging */);
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
		printf("Dyn loader Test: dlopen returned null\n");
		printf("Got message: %s\n", err);

		return 1;
	}

	simpleAddSymResult   = dlsymInSandbox(sandbox, dlHandle, "simpleAdd");
	simpleStrLenResult   = dlsymInSandbox(sandbox, dlHandle, "simpleStrLen");
	simpleCallbackResult = dlsymInSandbox(sandbox, dlHandle, "simpleCallback");

	if(simpleAddSymResult == NULL || simpleStrLenResult == NULL || simpleCallbackResult == NULL)
	{
		printf("Dyn loader Test: dlSym returned null\n");
		return 1;
	}

	/**************** Invoking functions in sandbox ****************/

	if(invokeSimpleAdd(sandbox, simpleAddSymResult, 2, 3) != 5)
	{
		printf("Dyn loader Test 1: Failed\n");
		return 1;
	}

	if(invokeSimpleStrLenWithStackString(sandbox, simpleStrLenResult, "Hello") != 5)
	{
		printf("Dyn loader Test 2: Failed\n");
		return 1;
	}

	if(invokeSimpleStrLenWithStackString(sandbox, simpleStrLenResult, "Hello") != 5)
	{
		printf("Dyn loader Test 3: Failed\n");
		return 1;
	}

	if(invokeSimpleCallback(sandbox, simpleCallbackResult, 4, "Hello") != 9)
	{
		printf("Dyn loader Test 4: Failed\n");
		return 1;
	}

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
      if (s[curIdx] == target) ret = curIdx;
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
		if(*str == toReplace)
		{
			*str = replaceWith;
		}

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
		execFolder = (char*)malloc(3);
		execFolder[0] = '.';
		execFolder[1] = SEPARATOR;
		execFolder[2] = '\0';
	}
	else
	{
		execFolder = (char*)malloc(strlen(executablePath) + 1);
		strcpy(execFolder, executablePath);
		execFolder[index + 1] = '\0';		
	}

	return execFolder;
}

char* concatenateAndFixSlash(char* string1, char* string2)
{
	char* ret;

	ret = (char*)malloc(strlen(string1) + strlen(string2) + 1);
	strcpy(ret, string1);
	strcat(ret, string2);

	replaceChar(ret, '/', SEPARATOR);
	return ret;
}