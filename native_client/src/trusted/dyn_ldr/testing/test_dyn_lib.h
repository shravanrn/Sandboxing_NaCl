typedef int (*CallbackType)(unsigned, const char*);

struct testStruct
{
	unsigned long fieldLong;
	const char* fieldString;
	unsigned int fieldBool; 
};

int simpleAddTest(int a, int b);
size_t simpleStrLenTest(const char* str);
int simpleCallbackTest(unsigned a, const char* b, CallbackType callback);
int simpleWriteToFileTest(FILE* file, const char* str);
char* simpleEchoTest(char * str);
double simpleDoubleAddTest(const double a, const double b);
unsigned long simpleLongAddTest(unsigned long a, unsigned long b);
struct testStruct simpleTestStructVal();
struct testStruct* simpleTestStructPtr();