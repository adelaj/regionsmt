#include "np.h"
#include "ll.h"
#include "log.h"
#include "memory.h"
#include "sort.h"
#include "strproc.h"

#include <string.h>
#include <stdlib.h>

bool empty_handler(const char *str, size_t len, void *Ptr, void *Context)
{
    (void) str;
    (void) len;
    struct handler_context *context = Context;
    if (context) bs((uint8_t *) Ptr + context->offset, context->bit_pos);
    return 1;
}

bool p_str_handler(const char *str, size_t len, void *Ptr, void *Context)
{
    (void) len;
    *(const char **) Ptr = str;
    return empty_handler(str, len, Ptr, Context);
}

// Deprecated!
bool str_handler(const char *str, size_t len, void *Ptr, void *context)
{
    (void) context;
    char **ptr = Ptr;
    char *tmp = malloc(len + 1);
    if (len + 1 && !tmp) return 0;
    memcpy(tmp, str, len + 1);
    *ptr = tmp;
    return 1;
}

// Warning! 'buff' may be not null-terminated


#define DECL_STR_TO_UINT(TYPE, SUFFIX, LIMIT, BACKEND_RETURN, BACKEND, RADIX) \
    unsigned str_to_ ## SUFFIX(const char *str, const char **ptr, TYPE *p_res) \
    { \
        errno = 0; \
        BACKEND_RETURN res = BACKEND(str, (char **) ptr, (RADIX)); \
        Errno_t err = errno; \
        if (res > (LIMIT)) \
        { \
            *p_res = (LIMIT); \
            return err && err != ERANGE ? 0 : CVT_OUT_OF_RANGE; \
        } \
        *p_res = (TYPE) res; \
        return err ? err == ERANGE ? CVT_OUT_OF_RANGE : 0 : 1; \
    }

DECL_STR_TO_UINT(uint64_t, uint64, UINT64_MAX, unsigned long long, strtoull, 10)
DECL_STR_TO_UINT(uint32_t, uint32, UINT32_MAX, unsigned long, strtoul, 10)
DECL_STR_TO_UINT(uint16_t, uint16, UINT16_MAX, unsigned long, strtoul, 10)
DECL_STR_TO_UINT(uint8_t, uint8, UINT8_MAX, unsigned long, strtoul, 10)
DECL_STR_TO_UINT(uint64_t, uint64_hex, UINT64_MAX, unsigned long long, strtoull, 16)
DECL_STR_TO_UINT(uint32_t, uint32_hex, UINT32_MAX, unsigned long, strtoul, 16)
DECL_STR_TO_UINT(uint16_t, uint16_hex, UINT16_MAX, unsigned long, strtoul, 16)
DECL_STR_TO_UINT(uint8_t, uint8_hex, UINT8_MAX, unsigned long, strtoul, 16)

#if defined _M_X64 || defined __x86_64__
DECL_STR_TO_UINT(size_t, size, SIZE_MAX, unsigned long long, strtoull, 10)
DECL_STR_TO_UINT(size_t, size_hex, SIZE_MAX, unsigned long long, strtoull, 16)
#elif defined _M_IX86 || defined __i386__
DECL_STR_TO_UINT(size_t, size, SIZE_MAX, unsigned long, strtoul, 10)
DECL_STR_TO_UINT(size_t, size_hex, SIZE_MAX, unsigned long, strtoul, 16)
#endif


unsigned str_to_fp64(const char *str, const char **ptr, double *p_res)
{
    errno = 0;
    *p_res = strtod(str, (char **) ptr);
    Errno_t err = errno;
    return err ? err == ERANGE ? CVT_OUT_OF_RANGE : 0 : 1;
}

bool bool_handler(const char *str, size_t len, void *Ptr, void *Context)
{
    (void) len;
    uint8_t res;
    const char *test;
    if (!str_to_uint8(str, &test, &res)) return 0;
    if (*test)
    {
        if (!Stricmp(str, "false")) res = 0;
        else if (!Stricmp(str, "true")) res = 1;
        else return 0;
    }
    if (res > 1) return 0;
    struct bool_handler_context *context = Context;
    if (!context) return 1;
    if (res) bs((uint8_t *) Ptr, context->bit_pos);
    else br((uint8_t *) Ptr, context->bit_pos);
    return empty_handler(str, len, Ptr, context->context);
}

bool bool_handler2(const char *str, size_t len, void *Ptr, void *Context)
{
    return str && len ? bool_handler(str, len, Ptr, Context) : 
        empty_handler(str, len, Ptr, &(struct handler_context) { .bit_pos = ((struct bool_handler_context *) Context)->bit_pos });
}

#define DECL_INT_HANDLER(TYPE, PREFIX, CONV) \
    bool PREFIX ## _handler(const char *str, size_t len, void *Ptr, void *Context) \
    { \
        (void) len; \
        TYPE *ptr = Ptr; \
        const char *test; \
        if (!CONV(str, &test, ptr) || *test) return 0; \
        return empty_handler(str, len, Ptr, Context); \
    }

DECL_INT_HANDLER(uint64_t, uint64, str_to_uint64)
DECL_INT_HANDLER(uint32_t, uint32, str_to_uint32)
DECL_INT_HANDLER(uint16_t, uint16, str_to_uint16)
DECL_INT_HANDLER(uint8_t, uint8, str_to_uint8)
DECL_INT_HANDLER(size_t, size, str_to_size)
DECL_INT_HANDLER(double, fp64, str_to_fp64)

bool str_tbl_handler(const char *str, size_t len, void *p_Off, void *Context)
{
    size_t *p_off = p_Off;
    struct str_tbl_handler_context *context = Context;
    if (!len && context->str_cnt) // Last null-terminator is used to store zero-length strings
    {
        *p_off = context->str_cnt - 1;
        return 1;
    }
    if (!array_test(&context->str, &context->str_cap, sizeof(*context->str), 0, 0, context->str_cnt, len, 1).status) return 0;
    *p_off = context->str_cnt;
    memcpy(context->str + context->str_cnt, str, (len + 1) * sizeof(*context->str));
    context->str_cnt += len + 1;
    return 1;
}

