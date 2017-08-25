#ifndef DYN_LDR_DS_ARRAY_H__
#define DYN_LDR_DS_ARRAY_H__ 1

#include <setjmp.h> 
#include "native_client/src/shared/platform/nacl_log.h"

#define STACK_TYPE jmp_buf
#define STACK_ARR_SIZE 10

struct _DS_Stack {
    STACK_TYPE data[STACK_ARR_SIZE];
    unsigned currentTop;
};

typedef struct _DS_Stack DS_Stack;

inline void Stack_Init(DS_Stack* stack)
{
    stack->currentTop = 0;
}

inline STACK_TYPE* Stack_GetTopPtrForPush(DS_Stack* stack)
{
    STACK_TYPE* ret = NULL;

    if(stack->currentTop == STACK_ARR_SIZE)
    {
        NaClLog(LOG_FATAL, "Jump buffer push overflow\n");
        return NULL;
    }

    ret = &(stack->data[stack->currentTop]);
    stack->currentTop++;
    return ret;
}

inline STACK_TYPE* Stack_GetTopPtrForPop(DS_Stack *stack)
{
    STACK_TYPE* ret = NULL;

    if(stack->currentTop == 0) {
        NaClLog(LOG_FATAL, "Jump buffer pop overflow\n");
        return NULL;
    }

    stack->currentTop--;
    ret = &(stack->data[stack->currentTop]);
    return ret;
}

#endif