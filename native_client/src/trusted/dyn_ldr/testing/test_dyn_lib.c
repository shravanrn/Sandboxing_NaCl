#include <string.h>
#include <stdint.h>

typedef unsigned (*CallbackType)(unsigned, unsigned);

int simpleAdd(int a, int b)
{
	return a + b;
}

size_t simpleStrLen(char* str)
{
	return strlen(str);
}

unsigned simpleCallback(unsigned a, unsigned b, CallbackType callback)
{
	unsigned ret = callback(a, b);
	return ret;
}