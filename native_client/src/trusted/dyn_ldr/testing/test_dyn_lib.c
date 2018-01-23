#include <string.h>
#include <stdint.h>
#include <stdio.h>

typedef int (*CallbackType)(unsigned, char*);

int simpleAddTest(int a, int b)
{
	printf("simpleAddTest\n");
	fflush(stdout);
	return a + b;
}

size_t simpleStrLenTest(char* str)
{
	printf("simpleStrLenTest\n");
	fflush(stdout);
	return strlen(str);
}


int simpleCallbackTest(unsigned a, char* b, CallbackType callback)
{
	int ret;
	
	printf("simpleCallbackTest\n");
	fflush(stdout);

	ret = callback(a + 1, b);
	return ret;
}

int simpleWriteToFileTest(FILE* file, char* str)
{
	printf("simpleWriteToFileTest\n");
	fflush(stdout);
	return fputs(str, file);
}

char* simpleEchoTest(char * str)
{
	printf("simpleEchoTest\n");
	fflush(stdout);
	return str;
}

double simpleDoubleAddTest(double a, double b)
{
	printf("simpleDoubleAddTest\n");
	return a + b;
}

unsigned long simpleLongAddTest(unsigned long a, unsigned long b)
{
	printf("simpleLongAddTest\n");
	fflush(stdout);
	return a + b;
}
