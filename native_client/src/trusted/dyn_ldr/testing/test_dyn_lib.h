typedef int (*CallbackType)(unsigned, char*);

int simpleAddTest(int a, int b);
size_t simpleStrLenTest(const char* str);
int simpleCallbackTest(unsigned a, char* b, CallbackType callback);
int simpleWriteToFileTest(FILE* file, char* str);
char* simpleEchoTest(char * str);
double simpleDoubleAddTest(double a, double b);
unsigned long simpleLongAddTest(unsigned long a, unsigned long b);