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

int invokeSimpleAdd(NaClSandbox* sandbox, void* simpleAddPtr, int a, int b)
{
  int ret;

  preFunctionCall(sandbox, sizeof(a) + sizeof(b));

  PUSH_VAL_TO_STACK(sandbox, int, a);
  PUSH_VAL_TO_STACK(sandbox, int, b);

  invokeFunctionCall(sandbox, simpleAddPtr);

  ret = (int)functionCallReturnRawPrimitiveInt(sandbox);
  return ret;
}

char* getExecFolder(char* executablePath);
char* concatenateAndFixSlash(char* string1, char* string2);


int main(int argc, char** argv)
{
	NaClSandbox* sandbox;
	void* dlHandle;
	void* dlSymResult;

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

	dlHandle = dlopenInSandbox(sandbox, dlToOpen, RTLD_LAZY);

	if(dlHandle == NULL)
	{
		char *err = dlerrorInSandbox(sandbox); 
		printf("Dyn loader Test: dlopen returned null\n");
		printf("Got message: %s\n", err);

		return 1;
	}

	dlSymResult = dlsymInSandbox(sandbox, dlHandle, "simpleAdd");

	if(dlSymResult == NULL)
	{
		printf("Dyn loader Test: dlSym returned null\n");
		return 1;
	}	

	if(invokeSimpleAdd(sandbox, dlSymResult, 2, 3) != 5)
	{
		printf("Dyn loader Test: Failed\n");
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