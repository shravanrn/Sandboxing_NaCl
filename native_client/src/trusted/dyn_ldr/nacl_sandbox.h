#include <type_traits>
#include <memory>
#include <functional>
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
	size_t size;
};

template <typename T>
inline sandbox_stackarr_helper<T> sandbox_stackarr(T* p_arr, size_t size) 
{ 
	sandbox_stackarr_helper<T> ret;
	ret.arr = p_arr;
	ret.size = size; 
	return ret; 
}

inline sandbox_stackarr_helper<const char> sandbox_stackarr(const char* p_arr) 
{ 
	return sandbox_stackarr(p_arr, strlen(p_arr) + 1); 
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

template <typename T>
inline sandbox_heaparr_helper<T>* sandbox_heaparr(NaClSandbox* sandbox, T* arg, size_t size)
{
	T* argInSandbox = (T *) mallocInSandbox(sandbox, size);
	memcpy((void*) argInSandbox, (void*) arg, size);

	auto ret = new sandbox_heaparr_helper<T>();
	ret->sandbox = sandbox;
	ret->arr = argInSandbox;
	return ret; 
}

inline sandbox_heaparr_helper<const char>* sandbox_heaparr(NaClSandbox* sandbox, const char* str)
{
	return sandbox_heaparr(sandbox, str, strlen(str) + 1);
}

template <typename T>
inline std::shared_ptr<sandbox_heaparr_helper<T>> sandbox_heaparr_sharedptr(NaClSandbox* sandbox, T* str, size_t size)
{
	return std::shared_ptr<sandbox_heaparr_helper<T>> (sandbox_heaparr(sandbox, str, size));
}


inline std::shared_ptr<sandbox_heaparr_helper<const char>> sandbox_heaparr_sharedptr(NaClSandbox* sandbox, const char* str)
{
	return sandbox_heaparr_sharedptr(sandbox, str, strlen(str) + 1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//https://stackoverflow.com/questions/10766112/c11-i-can-go-from-multiple-args-to-tuple-but-can-i-go-from-tuple-to-multiple
template <typename F, typename Tuple, bool Done, int Total, int... N>
struct call_impl
{
	static inline return_argument<F> call(F f, Tuple && t)
	{
		return call_impl<F, Tuple, Total == 1 + sizeof...(N), Total, N..., sizeof...(N)>::call(f, std::forward<Tuple>(t));
	}
};

template <typename F, typename Tuple, int Total, int... N>
struct call_impl<F, Tuple, true, Total, N...>
{
	static inline return_argument<F> call(F f, Tuple && t)
	{
		return f(std::get<N>(std::forward<Tuple>(t))...);
	}
};

template <typename F, typename Tuple>
inline return_argument<F> call_func(F f, Tuple && t)
{
	typedef typename std::decay<Tuple>::type ttype;
	return call_impl<F, Tuple, 0 == std::tuple_size<ttype>::value, std::tuple_size<ttype>::value>::call(f, std::forward<Tuple>(t));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename TArg>
inline typename std::enable_if<std::is_pointer<TArg>::value,
TArg>::type sandbox_get_callback_param(NaClSandbox_Thread* threadData)
{
	printf("callback pointer arg\n");
	return COMPLETELY_UNTRUSTED_CALLBACK_PTR_PARAM(threadData, TArg);
}

template<typename TArg>
inline typename std::enable_if<!std::is_pointer<TArg>::value,
TArg>::type sandbox_get_callback_param(NaClSandbox_Thread* threadData)
{
	printf("callback value arg\n");
	return COMPLETELY_UNTRUSTED_CALLBACK_STACK_PARAM(threadData, TArg);
}

template<typename... TArgs>
inline std::tuple<TArgs...> sandbox_get_callback_params_helper(NaClSandbox_Thread* threadData)
{
	//Note - we can't use make tuple here as this would(may?) iterate through the parameter right to left
	//So we use an initializer list which guarantees order of eval as left to right
	return std::tuple<TArgs...> {sandbox_get_callback_param<TArgs>(threadData)...};
}

template<typename Ret, typename... Rest>
inline std::tuple<Rest...> sandbox_get_callback_params(Ret(*f) (Rest...), NaClSandbox_Thread* threadData)
{
	UNUSED(f);
	return sandbox_get_callback_params_helper<Rest...>(threadData);
}

template<typename Ret, typename F, typename... Rest>
inline std::tuple<Rest...> sandbox_get_callback_params(Ret(F::*f) (Rest...), NaClSandbox_Thread* threadData)
{
	UNUSED(f);
	return sandbox_get_callback_params_helper<Rest...>(threadData);
}

template<typename Ret, typename F, typename... Rest>
inline std::tuple<Rest...> sandbox_get_callback_params(Ret(F::*f) (Rest...) const, NaClSandbox_Thread* threadData)
{
	UNUSED(f);
	return sandbox_get_callback_params_helper<Rest...>(threadData);
}

template <typename F>
decltype(sandbox_get_callback_params(&F::operator())) sandbox_get_callback_params(F);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
inline T sandbox_callback_return(NaClSandbox_Thread* threadData, T arg)
{
	UNUSED(threadData);
	return arg;
}

template <typename T>
inline T* sandbox_callback_return(NaClSandbox_Thread* threadData, T* arg)
{
	return CALLBACK_RETURN_PTR(threadData, T*, arg);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename TFunc>
class sandbox_callback_helper
{
public:
	NaClSandbox* sandbox;
	short callbackSlotNum;
	uintptr_t callbackRegisteredAddress;

	~sandbox_callback_helper()
	{
		printf("sandbox_callback_helper destructor called\n");
		if(!unregisterSandboxCallback(sandbox, callbackSlotNum))
		{
			sandbox_error("Unregister sandbox failed");
		}
	}
};

template<typename TFunc>
SANDBOX_CALLBACK return_argument<TFunc> sandbox_callback_receiver(uintptr_t sandboxPtr, void* state)
{
	NaClSandbox* sandbox = (NaClSandbox*) sandboxPtr;
	NaClSandbox_Thread* threadData = callbackParamsBegin(sandbox);

	TFunc* fnPtr = (TFunc*)(uintptr_t) state;
	fn_parameters<TFunc> params = sandbox_get_callback_params(fnPtr, threadData);
	printf("Calling callback function\n");
	auto ret = call_func(fnPtr, params);
	return sandbox_callback_return(threadData, ret);
}

template <typename T>
__attribute__ ((noinline)) typename std::enable_if<std::is_function<T>::value,
sandbox_callback_helper<T>*>::type sandbox_callback(NaClSandbox* sandbox, T* fnPtr)
{
	unsigned callbackSlotNum;

	if(!getFreeSandboxCallbackSlot(sandbox, &callbackSlotNum))
	{
		sandbox_error("No free callback slots left in sandbox");
	}

	uintptr_t callbackReceiver = (uintptr_t) sandbox_callback_receiver<T>;
	void* callbackState = (void*)(uintptr_t)fnPtr;

	auto callbackRegisteredAddress = registerSandboxCallbackWithState(sandbox, callbackSlotNum, callbackReceiver, callbackState);

	if(!callbackRegisteredAddress)
	{
		sandbox_error("Register sandbox failed");
	}

	auto ret = new sandbox_callback_helper<T>();
	ret->sandbox = sandbox;
	ret->callbackSlotNum = callbackSlotNum;
	ret->callbackRegisteredAddress = callbackRegisteredAddress;
	return ret; 
}

template <typename T>
std::shared_ptr<sandbox_callback_helper<T>> sandbox_callback_sharedptr(NaClSandbox* sandbox, T* fnPtr)
{
	return std::shared_ptr<sandbox_callback_helper<T>> (sandbox_callback(sandbox, fnPtr));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T>
T* sandbox_removeWrapper_helper(sandbox_stackarr_helper<T>);

template<typename T>
T* sandbox_removeWrapper_helper(sandbox_heaparr_helper<T>);
template<typename T>
T* sandbox_removeWrapper_helper(std::shared_ptr<sandbox_heaparr_helper<T>>);

template<typename T>
T* sandbox_removeWrapper_helper(sandbox_callback_helper<T>);
template<typename T>
T* sandbox_removeWrapper_helper(std::shared_ptr<sandbox_callback_helper<T>>);

template<typename T>
T sandbox_removeWrapper_helper(T);

template <typename T>
using sandbox_removeWrapper = decltype(sandbox_removeWrapper_helper(std::declval<T>()));

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
inline size_t sandbox_NaClStackArraySizeOf(T arg)
{
	UNUSED(arg);
	return 0;
}

template <typename T>
inline size_t sandbox_NaClStackArraySizeOf(sandbox_stackarr_helper<T> arg)
{
	return arg.size;
}

template<typename... T>
inline size_t sandbox_NaClAddStackArrParams(T... arg)
{
	UNUSED(arg...);
	return 0;
}

template <typename T, typename ... Targs>
inline size_t sandbox_NaClAddStackArrParams(T arg, Targs... rem)
{
	return sandbox_NaClStackArraySizeOf(arg) + sandbox_NaClAddStackArrParams(rem...);
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

template <typename T>
inline void sandbox_handleNaClArg(NaClSandbox_Thread* threadData, sandbox_stackarr_helper<T> arg)
{
	printf("got a stack array arg\n");
	PUSH_GEN_ARRAY_TO_STACK(threadData, arg.arr, arg.size);
}

template <typename T>
inline void sandbox_handleNaClArg(NaClSandbox_Thread* threadData, std::shared_ptr<sandbox_heaparr_helper<T>> arg)
{
	printf("got a heap copy ptr arg\n");
	PUSH_PTR_TO_STACK(threadData, T*, arg->arr);
}

template <typename T>
inline void sandbox_handleNaClArg(NaClSandbox_Thread* threadData, std::shared_ptr<sandbox_callback_helper<T>> arg)
{
	printf("got a callback arg\n");
	PUSH_VAL_TO_STACK(threadData, uintptr_t, arg->callbackRegisteredAddress);
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

//https://github.com/facebook/folly/blob/master/folly/functional/Invoke.h
//mimic C++17's std::invocable
namespace invoke_detail {

	template< class... >
	using void_t = void;

	template <typename F, typename... Args>
	constexpr auto invoke(F&& f, Args&&... args) noexcept(
	    noexcept(static_cast<F&&>(f)(static_cast<Args&&>(args)...)))
	    -> decltype(static_cast<F&&>(f)(static_cast<Args&&>(args)...)) {
	  return static_cast<F&&>(f)(static_cast<Args&&>(args)...);
	}
	template <typename M, typename C, typename... Args>
	constexpr auto invoke(M(C::*d), Args&&... args)
	    -> decltype(std::mem_fn(d)(static_cast<Args&&>(args)...)) {
	  return std::mem_fn(d)(static_cast<Args&&>(args)...);
	}

	template <typename F, typename... Args>
	using invoke_result_ =
	decltype(invoke(std::declval<F>(), std::declval<Args>()...));

	template <typename Void, typename F, typename... Args>
	struct is_invocable : std::false_type {};

	template <typename F, typename... Args>
	struct is_invocable<void_t<invoke_result_<F, Args...>>, F, Args...>
	: std::true_type {};
}

template <typename F, typename... Args>
struct is_invocable_port : invoke_detail::is_invocable<void, F, Args...> {};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T, typename ... Targs>
inline typename std::enable_if<is_invocable_port<T, sandbox_removeWrapper<Targs>...>::value, 
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
	NaClSandbox_Thread* threadData = preFunctionCall(sandbox, sandbox_NaClAddParams(param...), sandbox_NaClAddStackArrParams(param...) /* size of any arrays being pushed on the stack */);
	printf("StackArr Size: %u\n", (unsigned)sandbox_NaClAddStackArrParams(param...));
	sandbox_dealWithNaClArgs(threadData, param...);
	return sandbox_checkSignatureAndCallNaClFn<T, Targs ...>(threadData, fnPtr);
}

#define sandbox_invoke(sandbox, fnName, fnPtr, ...) sandbox_invoker<decltype(fnName)>(sandbox, fnPtr, ##__VA_ARGS__)
