#include <string.h>
#include <stdint.h>

typedef unsigned (*CallbackType)(unsigned, char*);

int simpleAddTest(int a, int b)
{
	return a + b;
}

size_t simpleStrLenTest(char* str)
{
	return strlen(str);
}

unsigned simpleCallbackTest(unsigned a, char* b, CallbackType callback)
{
	unsigned ret = callback(a, b);
	return ret;
}