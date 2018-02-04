#include <type_traits>
#include <memory>
#include <stdio.h>

//https://stackoverflow.com/questions/19532475/casting-a-variadic-parameter-pack-to-void
struct UNUSED_PACK_HELPER { template<typename ...Args> UNUSED_PACK_HELPER(Args const & ... ) {} };
#define UNUSED(x) UNUSED_PACK_HELPER {x}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//https://stackoverflow.com/questions/6512019/can-we-get-the-type-of-a-lambda-argument
template<typename Ret, typename... Rest>
Ret return_argument_helper(Ret(*) (Rest...));

template<typename Ret, typename F, typename... Rest>
Ret return_argument_helper(Ret(F::*) (Rest...));

template<typename Ret, typename F, typename... Rest>
Ret return_argument_helper(Ret(F::*) (Rest...) const);

template <typename F>
decltype(return_argument_helper(&F::operator())) return_argument_helper(F);

template <typename T>
using return_argument = decltype(return_argument_helper(std::declval<T>()));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename Ret, typename... Rest>
std::tuple<Rest...> fn_parameters_helper(Ret(*) (Rest...));

template<typename Ret, typename F, typename... Rest>
std::tuple<Rest...> fn_parameters_helper(Ret(F::*) (Rest...));

template<typename Ret, typename F, typename... Rest>
std::tuple<Rest...> fn_parameters_helper(Ret(F::*) (Rest...) const);

template <typename F>
decltype(fn_parameters_helper(&F::operator())) fn_parameters_helper(F);

template <typename T>
using fn_parameters = decltype(fn_parameters_helper(std::declval<T>()));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline void sandbox_error(const char* msg)
{
	printf("Sandbox error: %s\n", msg);
	exit(1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
class sandbox_stackarr_helper
{
public:
	T* arr;
};

template <typename T>
inline sandbox_stackarr_helper<T> sandbox_stackarr(T* p_arr) 
{ 
	sandbox_stackarr_helper<T> ret { p_arr }; 
	return ret; 
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
class sandbox_heaparr_helper
{
public:
	NaClSandbox* sandbox;
	T* arr;

	~sandbox_heaparr_helper()
	{
		printf("sandbox_heaparr_helper destructor called\n");
		freeInSandbox(sandbox, (void *)arr);
	}
};

//template <typename T>
inline std::shared_ptr<sandbox_heaparr_helper<const char>> sandbox_heaparr(NaClSandbox* sandbox, const char* str)
{
	char* strInSandbox = (char *) mallocInSandbox(sandbox, strlen(str) + 1);
	strcpy(strInSandbox, str);

	auto ret = std::make_shared<sandbox_heaparr_helper<const char>>();
	ret->sandbox = sandbox;
	ret->arr = strInSandbox;
	return ret; 
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class sandbox_callback_helper
{
public:
	NaClSandbox* sandbox;
	short callbackSlotNum;

	~sandbox_callback_helper()
	{
		printf("sandbox_callback_helper destructor called\n");
		if(!unregisterSandboxCallback(sandbox, callbackSlotNum))
		{
			sandbox_error("Unregister sandbox failed");
		}
	}
};

template <typename T>
inline typename std::enable_if<std::is_function<T>::value,
std::shared_ptr<sandbox_callback_helper>>::type sandbox_callback(NaClSandbox* sandbox, T fnPtr)
{
	unsigned callbackSlotNum;
	auto ret = std::make_shared<sandbox_callback_helper>();
	ret->sandbox = sandbox;

	if(!getFreeSandboxCallbackSlot(sandbox, callbackSlotNum))
	{
		sandbox_error("No free callback slots left in sandbox");
	}

	ret->callbackSlotNum = callbackSlotNum;

	//... Need to figure out callback stuff

	if(!registerSandboxCallback(sandbox, callbackSlotNum, (uintptr_t)fnPtr))
	{
		sandbox_error("Register sandbox failed");
	}

	return ret; 
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T>
T* sandbox_removeWrapper_helper(sandbox_stackarr_helper<T>);

template<typename T>
T* sandbox_removeWrapper_helper(std::shared_ptr<sandbox_heaparr_helper<T>>);

template<typename T>
T sandbox_removeWrapper_helper(T);

template <typename... T>
using sandbox_removeWrapper = std::tuple<decltype(sandbox_removeWrapper_helper(std::declval<T>()))...>;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename... T>
inline size_t sandbox_NaClAddParams(T... arg)
{
	UNUSED(arg...);
	return 0;
}

template <typename T, typename ... Targs>
inline size_t sandbox_NaClAddParams(T arg, Targs... rem)
{
	return sizeof(arg) + sandbox_NaClAddParams(rem...);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
inline void sandbox_handleNaClArg(NaClSandbox_Thread* threadData, T arg)
{
	printf("got a value arg\n");
	PUSH_VAL_TO_STACK(threadData, T, arg);
}

template <>
inline void sandbox_handleNaClArg(NaClSandbox_Thread* threadData, float arg)
{
	printf("got a float arg\n");
	PUSH_FLOAT_TO_STACK(threadData, float, arg);
}

template <>
inline void sandbox_handleNaClArg(NaClSandbox_Thread* threadData, double arg)
{
	printf("got a double arg\n");
	PUSH_FLOAT_TO_STACK(threadData, double, arg);
}

template <typename T>
inline void sandbox_handleNaClArg(NaClSandbox_Thread* threadData, T* arg)
{
	printf("got a pointer arg\n");
	PUSH_PTR_TO_STACK(threadData, T*, arg);
}

template <>
inline void sandbox_handleNaClArg(NaClSandbox_Thread* threadData, sandbox_stackarr_helper<const char> arg)
{
	printf("got a string stack arg\n");
	PUSH_STRING_TO_STACK(threadData, arg.arr);
}

template <typename T>
inline void sandbox_handleNaClArg(NaClSandbox_Thread* threadData, std::shared_ptr<sandbox_heaparr_helper<T>> arg)
{
	printf("got a heap copy ptr arg\n");
	PUSH_PTR_TO_STACK(threadData, T*, arg->arr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename... Targs>
inline void sandbox_dealWithNaClArgs(NaClSandbox_Thread* threadData, Targs... arg)
{
	UNUSED(threadData);
	UNUSED(arg...);
}

template <typename T, typename ... Targs>
inline void sandbox_dealWithNaClArgs(NaClSandbox_Thread* threadData, T arg, Targs... rem)
{
	sandbox_handleNaClArg(threadData, arg);
	sandbox_dealWithNaClArgs(threadData, rem...);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T, typename ... Targs>
inline typename std::enable_if<std::is_void<return_argument<T>>::value,
void>::type sandbox_invokeNaClReturn(NaClSandbox_Thread* threadData)
{
	printf("got a void return\n");
	UNUSED(threadData);
}

template <typename T, typename ... Targs>
inline typename std::enable_if<std::is_pointer<return_argument<T>>::value,
return_argument<T>>::type sandbox_invokeNaClReturn(NaClSandbox_Thread* threadData)
{
	printf("got a pointer return\n");
	return (return_argument<T>)functionCallReturnPtr(threadData);
}

template <typename T, typename ... Targs>
inline typename std::enable_if<std::is_floating_point<return_argument<T>>::value,
return_argument<T>>::type sandbox_invokeNaClReturn(NaClSandbox_Thread* threadData)
{
	printf("got a double return\n");
	return (return_argument<T>)functionCallReturnDouble(threadData);
}

template <typename T, typename ... Targs>
inline typename std::enable_if<!std::is_pointer<return_argument<T>>::value && !std::is_void<return_argument<T>>::value && !std::is_floating_point<return_argument<T>>::value,
return_argument<T>>::type sandbox_invokeNaClReturn(NaClSandbox_Thread* threadData)
{
	printf("got a value return\n");
	return (return_argument<T>)functionCallReturnRawPrimitiveInt(threadData);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////


template <typename T, typename ... Targs>
inline typename std::enable_if<std::is_same<fn_parameters<T>, sandbox_removeWrapper<Targs...>>::value, 
return_argument<T>>::type sandbox_checkSignatureAndCallNaClFn(NaClSandbox_Thread* threadData, void* fnPtr)
{
	invokeFunctionCall(threadData, fnPtr);
	return sandbox_invokeNaClReturn<T>(threadData);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T, typename ... Targs>
__attribute__ ((noinline)) typename std::enable_if<std::is_function<T>::value,
return_argument<T>>::type sandbox_invoker(NaClSandbox* sandbox, void* fnPtr, Targs ... param)
{
	NaClSandbox_Thread* threadData = preFunctionCall(sandbox, sandbox_NaClAddParams(param...), 128 /* size of any arrays being pushed on the stack */);
	sandbox_dealWithNaClArgs(threadData, param...);
	return sandbox_checkSignatureAndCallNaClFn<T, Targs ...>(threadData, fnPtr);
}

#define sandbox_invoke(sandbox, fnName, fnPtr, ...) sandbox_invoker<decltype(fnName)>(sandbox, fnPtr, ##__VA_ARGS__)
