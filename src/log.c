#include "np.h"
#include "ll.h"
#include "log.h"
#include "memory.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

void print(char *buff, size_t *p_cnt, const char *str, size_t len)
{
    size_t cnt = *p_cnt;
    if (cnt && cnt - 1 >= len) memcpy(buff, str, len + 1);
    *p_cnt = len;
}

bool print_fmt_var(char *buff, size_t *p_cnt, va_list arg)
{
    const char *fmt = va_arg(arg, const char *);
    if (fmt)
    {
        size_t cnt = *p_cnt;
        if (cnt)
        {
            int tmp = vsnprintf(buff, cnt, fmt, arg);
            if (tmp < 0) return 0;
            *p_cnt = (size_t) tmp;
        }
        else *p_cnt = 1; // Requesting string of non-zero length
    }
    else *p_cnt = 0; // Zero bytes written
    return 1;
}

enum fmt_flags {
    FMT_FLUSH_LEFT = 1,
    FMT_SIGN_PLACEHOLDER = 2,
    FMT_SIGN_POSITIVE = FMT_SIGN_PLACEHOLDER | 4,
    FMT_FANCY_NUMBERS = 8,
    FMT_ZERO_PADDING = 16,
    FMT_ARG_WIDTH = 32,
    FMT_ARG_PRECISION = 64
};

enum fmt_type {
    FMT_INT8 = 0, // 'bi'
    FMT_INT16, // 'wi'
    FMT_INT32, // 'di'
    FMT_INT64, // 'qi'
    FMT_PTR_DIFF, // 'zi'
    FMT_INTPTR, // 'pi'
    FMT_INT, // 'i'
    FMT_LONG, // 'li'
    FMT_LONGLONG, // 'lli'
    FMT_UINT8, // 'bu'
    FMT_UINT16, // 'wu'
    FMT_UINT32, // 'du'
    FMT_UINT64, // 'qu'
    FMT_SIZE, // 'zu'
    FMT_UINTPTR, // 'pu'
    FMT_CHAR, // 'c'
    FMT_STR, // 's'
    FMT_TIME_DIFF, // 'T'
    FMT_TIME_STAMP, // 'U'
};

struct fmt_context {
    size_t width, precision;
    enum fmt_flags flags;
    enum fmt_type type;
};

bool decode_fmt(struct fmt_context *context, char *fmt, size_t *p_pos)
{
    size_t pos = *p_pos;
    for (;;)
    {

    }
    *p_pos = pos;
    return 1;
}

bool print_fmt2_var(char *buff, size_t *p_cnt, va_list arg)
{
    const char *fmt = va_arg(arg, const char *);
    if (fmt)
    {
        bool halt = 0;
        size_t cnt = *p_cnt;
        for (size_t i = 0, len = 0, tmp = *p_cnt, pos = 0, ctr; !halt && (tmp = len = size_sub_sat(tmp, len)); cnt = size_add_sat(cnt, len), i++) switch (i)
        {
        case 0:
            ctr = Strchrnul(fmt + pos, '%');
            print(buff + cnt, &len, fmt + pos, ctr);
            if (!fmt[pos += ctr]) halt = 1;
            break;
        case 1:
            for (pos++;; pos++)
            {
                switch (fmt[pos])
                {
                case '-':
                case '+':
                case ' ':

                    return 0; // Wrong format string
                }
            }
        default:
            i = 0;
        }
        *p_cnt = cnt;
    }
    else *p_cnt = 0;
    return 1;
}

bool print_fmt(char *buff, size_t *p_cnt, ...)
{
    va_list arg;
    va_start(arg, p_cnt);
    bool res = print_fmt_var(buff, p_cnt, arg);
    va_end(arg);
    return res;
}

bool print_time_diff(char *buff, size_t *p_cnt, uint64_t start, uint64_t stop, struct env env)
{
    uint64_t diff = DIST(stop, start), hdq = diff / 3600000000, hdr = diff % 3600000000, mdq = hdr / 60000000, mdr = hdr % 60000000;
    double sec = 1.e-6 * (double) mdr;
    return
        hdq ? print_fmt(buff, p_cnt, "%s%" PRIu64 "%s hr %s%" PRIu64 "%s min %s%.6f%s sec", ENV(env, hdq), ENV(env, mdq), ENV(env, sec)) :
        mdq ? print_fmt(buff, p_cnt, "%s%" PRIu64 "%s min %s%.6f%s sec", ENV(env, mdq), ENV(env, sec)) :
        sec >= 1.e-6 ? print_fmt(buff, p_cnt, "%s%.6f%s sec", ENV(env, sec)) :
        print_fmt(buff, p_cnt, "less than %s%.6f%s sec", ENV(env, 1.e-6));
}

void print_time_stamp(char *buff, size_t *p_cnt)
{
    size_t cnt = *p_cnt;
    if (cnt)
    {
        time_t t;
        time(&t);
        struct tm ts;
        Localtime_s(&ts, &t);
        size_t len = strftime(buff, cnt, "%Y-%m-%d %H:%M:%S UTC%z", &ts);
        *p_cnt = len ? len : size_add_sat(cnt, cnt);
    }
    else *p_cnt = 1;
}

bool message_time_diff(char *buff, size_t *p_cnt, void *Context, struct style style)
{
    struct time_diff *context = Context;
    size_t len = *p_cnt, cnt = 0;
    for (unsigned i = 0;; i++)
    {
        size_t tmp = len;
        switch (i)
        {
        case 0:
            if (!print_time_diff(buff + cnt, &tmp, context->start, context->stop, style.time)) return 0;
            break;
        case 1:
            print(buff + cnt, &tmp, STRC(".\n"));
        }
        cnt = size_add_sat(cnt, tmp);
        if (i == 1) break;
        len = size_sub_sat(len, tmp);
    }
    *p_cnt = cnt;
    return 1;
}

bool message_crt(char *buff, size_t *p_cnt, void *Context, struct style style)
{
    (void) style;
    size_t cnt = *p_cnt;
    if (!Strerror_s(buff, cnt, *(Errno_t *) Context))
    {
        size_t len = Strnlen(buff, cnt); // 'len' is not greater than 'cnt' due to the previous condition
        int tmp = snprintf(buff + len, cnt - len, "!\n");
        if (tmp < 0) return 0;
        *p_cnt = size_add_sat(len, (size_t) tmp);
        return 1; 
    }
    *p_cnt = size_add_sat(cnt, cnt);
    return 1;
}

bool message_var(char *buff, size_t *p_cnt, void *context, struct style style, va_list arg)
{
    (void) style;
    (void) context;
    return print_fmt_var(buff, p_cnt, arg);
}

#define STAGED_LOOP(I, LIM_I, LEN, INI_LEN, TMP, CNT) \
    for (size_t (I) = 0, (LEN) = (INI_LEN), (TMP) = (LEN); (I) < (LIM_I) && ((TMP) = (LEN) = size_sub_sat((TMP), (LEN))); (CNT) = size_add_sat((CNT), (LEN)), (I)++) switch (I)

bool message_var_two_stage(char *buff, size_t *p_cnt, void *Thunk, struct style style, va_list arg)
{
    size_t cnt = 0;
    struct message_thunk *thunk = Thunk;
    for (size_t i = 0, len = 0, tmp = *p_cnt; i < 2 && (tmp = len = size_sub_sat(tmp, len)); cnt = size_add_sat(cnt, len), i++) switch (i)
    {
    case 0:
        if (!print_fmt_var(buff + cnt, &len, arg)) return 0;
        break;
    case 1:
        if (!thunk->handler(buff + cnt, &len, thunk->context, style)) return 0;
        break;
    }
    *p_cnt = cnt;
    return 1;
}

// Last argument may be 'NULL'
bool log_init(struct log *restrict log, char *restrict path, size_t lim, enum log_flags flags, struct style style, struct log *restrict log_error)
{
    FILE *f = path ? fopen(path, flags & LOG_APPEND ? "a" : "w") : stderr;
    if (path && !f) log_message_fopen(log_error, CODE_METRIC, MESSAGE_ERROR, path, errno);
    else
    {
        bool tty = file_is_tty(f), bom = !(tty || (flags & (LOG_NO_BOM | LOG_APPEND)));
        size_t cap = bom ? MAX(lim, lengthof(UTF8_BOM)) : lim;
        if (!array_init(&log->buff, &log->cap, cap, sizeof(*log->buff), 0, 0)) log_message_crt(log_error, CODE_METRIC, MESSAGE_ERROR, errno);
        else
        {
            if (bom) strncpy(log->buff, UTF8_BOM, log->cnt = lengthof(UTF8_BOM));
            else log->cnt = 0;
            log->style = tty ? style : (struct style) { 0 };
            log->lim = lim;
            log->file = f;
            log->tot = 0;
            if (log->cnt < log->lim || log_flush(log)) return 1;
            free(log->buff);
        }
        Fclose(f);
    }
    return 0;
}

bool log_flush(struct log *restrict log)
{
    size_t cnt = log->cnt;
    if (cnt)
    {
        size_t wr = fwrite(log->buff, 1, cnt, log->file);
        log->tot += wr;
        log->cnt = 0;
        fflush(log->file);
        return wr == cnt;
    }
    return 1;
}

// May be used for 'log' allocated by 'calloc' (or filled with zeros statically)
void log_close(struct log *restrict log)
{
    log_flush(log);
    Fclose(log->file);
    free(log->buff);
}

bool log_multiple_init(struct log *restrict arr, size_t cnt, char *restrict path, size_t cap, enum log_flags flags, struct style style, struct log *restrict log_error)
{
    size_t i = 0;
    for (; i < cnt && log_init(arr + i, path, cap, flags | LOG_APPEND, style, log_error); i++);
    if (i == cnt) return 1;
    for (; --i; log_close(arr + i));
    return 0;
}

void log_multiple_close(struct log *restrict arr, size_t cnt)
{
    if (cnt) for (size_t i = cnt; --i; log_close(arr + i));
}

static bool log_prefix(char *buff, size_t *p_cnt, struct code_metric code_metric, enum message_type type, struct style style)
{
    const char *ttl[] = { "MESSAGE", "ERROR", "WARNING", "NOTE", "INFO", "DAMNATION" };
    size_t cnt = 0;
    for (size_t i = 0, len = 0, tmp = *p_cnt; i < 3 && (tmp = len = size_sub_sat(tmp, len)); cnt = size_add_sat(cnt, len), i++) switch (i)
    {
    case 0:
        if (!print_fmt(buff + cnt, &len, "%s[", ENV_BEGIN(style.ts))) return 0;
        break;
    case 1:
        print_time_stamp(buff + cnt, &len);
        break;
    case 2:
        if (!print_fmt(buff + cnt, &len, "]%s %s%s%s %s(%s%s%s @ %s%s%s:%zu):%s ", ENV_END(style.ts), ENV(style.ttl[type], ttl[type]), ENV(style.src, DQUO(style.dquo, code_metric.func), DQUO(style.dquo, code_metric.path), code_metric.line))) return 0;
    }
    *p_cnt = cnt;
    return 1;
}

static bool log_message_impl(struct log *restrict log, struct code_metric code_metric, enum message_type type, message_callback handler, message_callback_var handler_var, void *context, va_list arg)
{
    if (!log) return 1;
    size_t cnt = log->cnt;
    for (unsigned i = 0; i < 2; i++) for (;;)
    {
        size_t len = log->cap - cnt;
        switch (i)
        {
        case 0:
            if (!log_prefix(log->buff + cnt, &len, code_metric, type, log->style)) return 0;
            break;
        case 1:
            if (handler_var)
            {
                va_list arg2;
                va_copy(arg2, arg);
                bool res = handler_var(log->buff + cnt, &len, context, log->style, arg2);
                va_end(arg2);
                if (!res) return 0;
            }
            else if (handler && !handler(log->buff + cnt, &len, context, log->style)) return 0;
        }
        unsigned res = array_test(&log->buff, &log->cap, sizeof(*log->buff), 0, 0, cnt, len, 1); // All messages are NULL-terminated
        if (!res) return 0;
        if (!(res & ARRAY_UNTOUCHED)) continue;
        cnt += len;
        break;
    }
    log->cnt = cnt;
    return cnt >= log->lim ? log_flush(log) : 1;
}

// Emulation of passing '(va_list) 0'
static bool log_message_supp(struct log *restrict log, struct code_metric code_metric, enum message_type type, message_callback handler, void *context, ...)
{
    va_list arg;
    va_start(arg, context);
    bool res = log_message_impl(log, code_metric, type, handler, NULL, context, arg);
    va_end(arg);
    return res;
}

bool log_message(struct log *restrict log, struct code_metric code_metric, enum message_type type, message_callback handler, void *context)
{
    return log_message_supp(log, code_metric, type, handler, context);
}

bool log_message_var(struct log *restrict log, struct code_metric code_metric, enum message_type type, message_callback_var handler_var, void *context, ...)
{
    va_list arg;
    va_start(arg, context);
    bool res = log_message_impl(log, code_metric, type, NULL, handler_var, context, arg);
    va_end(arg);
    return res;
}

bool log_message_fmt(struct log *restrict log, struct code_metric code_metric, enum message_type type, ...)
{
    va_list arg;
    va_start(arg, type);
    bool res = log_message_impl(log, code_metric, type, NULL, message_var, NULL, arg);
    va_end(arg);
    return res;
}

bool log_message_time_diff(struct log *restrict log, struct code_metric code_metric, enum message_type type, uint64_t start, uint64_t stop, ...)
{
    va_list arg;
    va_start(arg, stop);
    bool res = log_message_impl(log, code_metric, type, NULL, message_var_two_stage, &(struct message_thunk) { .handler = message_time_diff, .context = &(struct time_diff) { .start = start, .stop = stop } }, arg);
    va_end(arg);
    return res;
}

bool log_message_crt(struct log *restrict log, struct code_metric code_metric, enum message_type type, Errno_t err)
{
    return log_message(log, code_metric, type, message_crt, &err);
}

bool log_message_fopen(struct log *restrict log, struct code_metric code_metric, enum message_type type, const char *restrict path, Errno_t err)
{
    return log_message_var(log, code_metric, type, message_var_two_stage, &(struct message_thunk) { .handler = message_crt, .context = &err }, "Unable to open the file %s%s%s%s%s: ", ENV(log->style.path, ENV(log->style.dquo, path)));
}

bool log_message_fseek(struct log *restrict log, struct code_metric code_metric, enum message_type type, int64_t offset, const char *restrict path)
{
    return log_message_fmt(log, code_metric, type, "Unable to seek into the position %s%" PRId64 "%s while reading the file %s%s%s%s%s!\n", ENV(log->style.num, offset), ENV(log->style.path, ENV(log->style.dquo, path)));
}
