#pragma once
#include <cstring>

typedef enum : int {
    PWR_SUCCESS = 0,
    PWR_FAIL    = 1
} PwrErrCode;

#define PWR_ERROR(...) printf("\033[0;31m" "[ ERROR    ] " "\033[0m" __VA_ARGS__)
#define PWR_INFO(...)  printf("[ INFO     ] " __VA_ARGS__)
#define PWR_WARN(...)  printf("[ WARNING  ] " __VA_ARGS__)

#define PWR_RETURN_RESULT(result) return (result)

#define CHECK_PWR_CALL_BASE(func, RESULT, RESULT_ARGS...) \
    do {                                                  \
        int status = func;                                \
        if (status != PWR_RET_SUCCESS)                    \
        {                                                 \
            PWR_ERROR("Call %s\n", #func);                \
            RESULT(PWR_FAIL, ##RESULT_ARGS);              \
        }                                                 \
    } while (false)

#define CHECK_PWR_CALL(func) CHECK_PWR_CALL_BASE(func, PWR_RETURN_RESULT)

#define CHECK_PWR_FUNC_CALL(func) \
    do {                                                       \
        PwrErrCode status = func;                              \
        if (status != PWR_SUCCESS)                             \
        {                                                      \
            PWR_ERROR("Function call: %s\n", #func);           \
        }                                                      \
    } while (false)
