#ifndef DYN_LDR_DS_ARRAY_H__
#define DYN_LDR_DS_ARRAY_H__ 1

#include <setjmp.h> 
#include "native_client/src/shared/platform/nacl_log.h"

#define STACK_TYPE jmp_buf
#define STACK_ARR_SIZE 10

typedef struct {
    STACK_TYPE data[STACK_ARR_SIZE];
    unsigned currentTop;
} DS_Stack;

#define Stack_CreateStack(var) DS_Stack var { .currentTop = 0 }


inline STACK_TYPE* Stack_GetTopPtrForPush(DS_Stack* stack)
{
    STACK_TYPE* ret = NULL;

    NaClLog(LOG_INFO, "Entering Stack_GetTopPtrForPush\n");

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

    NaClLog(LOG_INFO, "Entering Stack_GetTopPtrForPop\n");


    if(stack->currentTop == 0) {
        NaClLog(LOG_FATAL, "Jump buffer pop overflow\n");
        return NULL;
    }

    stack->currentTop--;
    ret = &(stack->data[stack->currentTop]);
    return ret;
}

#endif