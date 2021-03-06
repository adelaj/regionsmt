#include "np.h"
#include "ll.h"
#include "cmp.h"
#include "memory.h"
#include "sort.h"

#include <inttypes.h>
#include <string.h>
#include <stdlib.h>

struct generic_cmp_thunk {
    cmp_callback cmp;
    void *context;
};

static bool generic_cmp(const void *A, const void *B, void *Thunk)
{
    struct generic_cmp_thunk *thunk = Thunk;
    return thunk->cmp(*(const void **) A, *(const void **) B, thunk->context);
}

struct array_result pointers(uintptr_t **p_ptr, const void *arr, size_t cnt, size_t stride, cmp_callback cmp, void *context)
{
    uintptr_t *ptr, swp;
    struct array_result res = array_init(&ptr, NULL, cnt, sizeof(*ptr), 0, ARRAY_STRICT);
    if (!res.status) return res;
    for (size_t i = 0, j = 0; i < cnt; ptr[i] = (uintptr_t) arr + j, i++, j += stride);
    quick_sort(ptr, cnt, sizeof(*ptr), generic_cmp, &(struct generic_cmp_thunk) { .cmp = cmp, .context = context }, &swp, sizeof(*ptr));
    *p_ptr = ptr;
    return res;
}

struct generic_cmp_stable_thunk {
    stable_cmp_callback cmp;
    void *context;
};

static bool generic_cmp_stable(const void *A, const void *B, void *Thunk)
{
    struct generic_cmp_stable_thunk *thunk = Thunk;
    const void **a = (void *) A, **b = (void *) B;
    int res = thunk->cmp(*a, *b, thunk->context);
    if (res > 0 || (!res && *a > *b)) return 1;
    return 0;
}

struct array_result pointers_stable(uintptr_t **p_ptr, const void *arr, size_t cnt, size_t stride, stable_cmp_callback cmp, void *context)
{
    uintptr_t *ptr, swp;
    struct array_result res = array_init(&ptr, NULL, cnt, sizeof(*ptr), 0, ARRAY_STRICT);
    if (!res.status) return res;
    for (size_t i = 0, j = 0; i < cnt; ptr[i] = (uintptr_t) arr + j, i++, j += stride);
    quick_sort(ptr, cnt, sizeof(*ptr), generic_cmp_stable, &(struct generic_cmp_stable_thunk) { .cmp = cmp, .context = context }, &swp, sizeof(*ptr));
    *p_ptr = ptr;
    return res;
}

void orders_from_pointers_inplace(uintptr_t *ptr, uintptr_t base, size_t cnt, size_t stride)
{
    for (size_t i = 0; i < cnt; ptr[i] = (ptr[i] - base) / stride, i++);
}

struct array_result orders_stable(uintptr_t **p_ord, const void *arr, size_t cnt, size_t stride, stable_cmp_callback cmp, void *context)
{
    uintptr_t *ord;
    struct array_result res = pointers_stable(&ord, arr, cnt, stride, cmp, context);
    if (!res.status) return res;
    orders_from_pointers_inplace(ord, (uintptr_t) arr, cnt, stride);
    *p_ord = ord;
    return res;
}

struct array_result orders_stable_unique(uintptr_t **p_ord, const void *arr, size_t *p_cnt, size_t stride, stable_cmp_callback cmp, void *context)
{
    size_t cnt = *p_cnt;
    uintptr_t *ord;
    struct array_result res = pointers_stable(&ord, arr, cnt, stride, cmp, context);
    if (!res.status) return res;

    uintptr_t tmp = 0;
    size_t ucnt = 0;    
    if (cnt) tmp = ord[0], ord[ucnt++] = (tmp - (uintptr_t) arr) / stride;
    for (size_t i = 1; i < cnt; i++) if (cmp((const void *) ord[i], (const void *) tmp, context) > 0)
    {
        tmp = ord[i];
        ord[ucnt++] = (tmp - (uintptr_t) arr) / stride;
    }
    
    // Even if the operation fails, the result is still valid
    array_test(&ord, &ucnt, sizeof(*ord), 0, ARRAY_REDUCE, ucnt);
    *p_ord = ord;
    if (*p_cnt) *p_cnt = ucnt;
    return res;
}

void ranks_from_pointers_impl(size_t *rnk, const uintptr_t *ptr, uintptr_t base, size_t cnt, size_t stride)
{
    for (size_t i = 0; i < cnt; rnk[(ptr[i] - base) / stride] = i, i++);
}

struct array_result ranks_from_pointers(size_t **p_rnk, const uintptr_t *ptr, uintptr_t base, size_t cnt, size_t stride)
{
    size_t *rnk;
    struct array_result res = array_init(&rnk, NULL, cnt, sizeof(*rnk), 0, ARRAY_STRICT);
    if (!res.status) return res;
    ranks_from_pointers_impl(rnk, ptr, base, cnt, stride);
    *p_rnk = rnk;
    return res;
}

struct array_result ranks_from_orders(size_t **p_rnk, const uintptr_t *ord, size_t cnt)
{
    return ranks_from_pointers(p_rnk, ord, 0, cnt, 1);
}

struct array_result ranks_unique(size_t **p_rnk, const void *arr, size_t *p_cnt, size_t stride, cmp_callback cmp, void *context)
{
    size_t *rnk;
    uintptr_t *ptr = NULL;
    struct array_result res = pointers(&ptr, arr, *p_cnt, stride, cmp, context);
    if (!res.status) return res;
    res = ranks_unique_from_pointers(&rnk, ptr, (uintptr_t) arr, p_cnt, stride, cmp, context);
    if (res.status) *p_rnk = rnk;
    free(ptr);
    return res;
}

// Warning! 'ptr' may contain pointers to the 'rnk' array (i. e. 'rnk' = 'base').
void ranks_unique_from_pointers_impl(size_t *rnk, const uintptr_t *ptr, uintptr_t base, size_t *p_cnt, size_t stride, cmp_callback cmp, void *context)
{
    size_t cnt = *p_cnt;
    if (!cnt) return;
    size_t ucnt = 0;
    for (size_t i = 0; i < cnt - 1; i++)
    {
        size_t tmp = ucnt;
        if (cmp((void *) ptr[i + 1], (void *) ptr[i], context)) ucnt++;
        rnk[(ptr[i] - base) / stride] = tmp;
    }
    rnk[(ptr[cnt - 1] - base) / stride] = ucnt;
    *p_cnt = ucnt + 1;
}

struct array_result ranks_unique_from_pointers(size_t **p_rnk, const uintptr_t *ptr, uintptr_t base, size_t *p_cnt, size_t stride, cmp_callback cmp, void *context)
{
    size_t *rnk;
    struct array_result res = array_init(&rnk, NULL, *p_cnt, sizeof(*rnk), 0, ARRAY_STRICT);
    if (!res.status) return res;
    ranks_unique_from_pointers_impl(rnk, ptr, base, p_cnt, stride, cmp, context);
    *p_rnk = rnk;
    return res;
}

struct cmp_helper_thunk {
    cmp_callback cmp;
    const void *arr;
    size_t stride;
    void *context;
};

static bool cmp_helper(const void *a, const void *b, void *Context)
{
    struct cmp_helper_thunk *context = Context;
    return context->cmp((char *) context->arr + (uintptr_t) a * context->stride, (char *) context->arr + (uintptr_t) b * context->stride, context->context);
}

struct array_result ranks_unique_from_orders(size_t **p_rnk, const uintptr_t *ord, const void *arr, size_t *p_cnt, size_t stride, cmp_callback cmp, void *context)
{
    return ranks_unique_from_pointers(p_rnk, ord, 0, p_cnt, 1, cmp_helper, &(struct cmp_helper_thunk) { .cmp = cmp, .arr = arr, .stride = stride, .context = context });
}

void ranks_from_pointers_inplace_impl(uintptr_t *restrict ptr, uintptr_t base, size_t cnt, size_t stride, uint8_t *restrict bits)
{
    for (size_t i = 0; i < cnt; i++)
    {
        size_t j = i;
        uintptr_t k = (ptr[i] - base) / stride;
        while (!bt(bits, j))
        {
            uintptr_t l = (ptr[k] - base) / stride;
            bs(bits, j);
            ptr[k] = j;
            j = k;
            k = l;
        }
    }
}

struct array_result ranks_from_pointers_inplace(uintptr_t *restrict ptr, uintptr_t base, size_t cnt, size_t stride)
{
    uint8_t *bits = NULL;
    struct array_result res = array_init(&bits, NULL, UINT8_CNT(cnt), sizeof(*bits), 0, ARRAY_STRICT | ARRAY_CLEAR);
    if (!res.status) return res;
    ranks_from_pointers_inplace_impl(ptr, base, cnt, stride, bits);
    free(bits);
    return (struct array_result) { .status = ARRAY_SUCCESS_UNTOUCHED };
}

struct array_result ranks_from_orders_inplace(uintptr_t *restrict ord, size_t cnt)
{
    return ranks_from_pointers_inplace(ord, 0, cnt, 1);
}

struct array_result ranks_stable(uintptr_t **p_rnk, const void *arr, size_t cnt, size_t stride, stable_cmp_callback cmp, void *context)
{
    uintptr_t *rnk;
    struct array_result res0 = pointers_stable(&rnk, arr, cnt, stride, cmp, context);
    if (!res0.status) return res0;
    struct array_result res1 = ranks_from_pointers_inplace(rnk, (uintptr_t) arr, cnt, stride);
    if (res1.status)
    {
        *p_rnk = rnk;
        return res0;
    }
    free(rnk);
    return res1;
}

void orders_apply_impl(uintptr_t *restrict ord, size_t cnt, size_t sz, void *restrict arr, uint8_t *restrict bits, void *restrict swp, size_t stride)
{
    for (size_t i = 0; i < cnt; i++)
    {
        if (bt(bits, i)) continue;
        size_t k = ord[i];
        if (k == i) continue;
        bs(bits, i);
        memcpy(swp, (char *) arr + i * stride, sz);
        for (size_t j = i;;)
        {
            while (k < cnt && bt(bits, k)) k = ord[k];
            memcpy((char *) arr + j * stride, (char *) arr + k * stride, sz);
            if (k >= cnt)
            {
                memcpy((char *) arr + k * stride, swp, sz);
                break;
            }
            j = k;
            k = ord[j];
            bs(bits, j);
            if (k == i)
            {
                memcpy((char *) arr + j * stride, swp, sz);
                break;
            }
        }
    }
}

// This procedure applies orders to the array. Orders are not assumed to be a surjective map. 
struct array_result orders_apply(uintptr_t *restrict ord, size_t cnt, size_t sz, void *restrict arr, void *restrict swp, size_t stride)
{
    uint8_t *bits = NULL;
    struct array_result res = array_init(&bits, NULL, UINT8_CNT(cnt), sizeof(*bits), 0, ARRAY_STRICT | ARRAY_CLEAR);
    if (!res.status) return res;
    orders_apply_impl(ord, cnt, sz, arr, bits, swp, stride);
    free(bits);
    return (struct array_result) { .status = ARRAY_SUCCESS_UNTOUCHED };
}

static void swap(void *restrict a, void *restrict b, void *restrict swp, size_t sz)
{
    memcpy(swp, a, sz);
    memcpy(a, b, sz);
    memcpy(b, swp, sz);
}

#ifndef QUICK_SORT_CACHED
static void lin_sort_stub(void *restrict arr, size_t tot, size_t sz, cmp_callback cmp, void *context, void *restrict swp, size_t stride, size_t cutoff)
{
    (void) arr, (void) tot, (void) sz, (void) cmp, (void) context, (void) swp, (void) stride, (void) cutoff;
}
#endif

static void insertion_sort_impl(void *restrict arr, size_t tot, size_t sz, cmp_callback cmp, void *context, void *restrict swp, size_t stride, size_t cutoff)
{
    size_t min = 0;
    for (size_t i = stride; i < cutoff; i += stride) if (cmp((char *) arr + min, (char *) arr + i, context)) min = i;
    if (min) swap((char *) arr + min, (char *) arr, swp, sz);
    for (size_t i = stride + stride; i < tot; i += stride)
    {
        size_t j = i;
        if (cmp((char *) arr + j - stride, (char *) arr + j, context)) // First iteration is unrolled
        {
            j -= stride;
            memcpy(swp, (char *) arr + j, sz);
            for (; j > stride && cmp((char *) arr + j - stride, (char *) arr + i, context); j -= stride) memcpy((char *) arr + j, (char *) arr + j - stride, sz);
            memcpy((char *) arr + j, (char *) arr + i, sz);
            memcpy((char *) arr + i, swp, sz);
        }
    }
}

typedef void (*lin_sort_callback)(void *restrict, size_t, size_t, cmp_callback, void *, void *restrict, size_t, size_t);
typedef void (*sort_callback)(void *restrict, size_t, size_t, cmp_callback, void *, void *restrict, size_t, size_t, lin_sort_callback);

static size_t comb_sort_gap_impl(size_t tot, size_t stride)
{
    size_t gap = (size_t) (.8017118471377937539 * (tot / stride)); // Magic constant suggested by 'ksort.h'. Any positive value < 1.0 is acceptable 
    if (gap == 9 || gap == 10) gap = 11;
    return gap * stride;
}

static void comb_sort_impl(void *restrict arr, size_t tot, size_t sz, cmp_callback cmp, void *context, void *restrict swp, size_t stride, size_t lin_cutoff, lin_sort_callback lin_sort)
{
    size_t gap = comb_sort_gap_impl(tot, stride);
    for (bool flag = 0;; flag = 0, gap = comb_sort_gap_impl(gap, stride))
    {
        for (size_t i = 0, j = gap; i < tot - gap; i += stride, j += stride)
            if (cmp((char *) arr + i, (char *) arr + j, context)) swap((char *) arr + i, (char *) arr + j, swp, sz), flag = 1;
        if (gap == stride) return;
        if (!flag || gap <= lin_cutoff) break;
    }
    lin_sort(arr, tot, sz, cmp, context, swp, stride, gap);
}

static void median_of_three_swap(void *restrict arr, size_t a, size_t pvt, size_t b, size_t sz, cmp_callback cmp, void *context, void *restrict swp)
{
    if (cmp((char *) arr + a, (char *) arr + pvt, context)) swap((char *) arr + a, (char *) arr + pvt, swp, sz);
    if (cmp((char *) arr + pvt, (char *) arr + b, context))
    {
        swap((char *) arr + pvt, (char *) arr + b, swp, sz);
        if (cmp((char *) arr + a, (char *) arr + pvt, context)) swap((char *) arr + a, (char *) arr + pvt, swp, sz);
    }
}

static size_t median_of_three(void *restrict arr, size_t a, size_t pvt, size_t b, cmp_callback cmp, void *context)
{
    return cmp((char *) arr + a, (char *) arr + pvt, context) ?
        cmp((char *) arr + pvt, (char *) arr + b, context) ? pvt : cmp((char *) arr + a, (char *) arr + b, context) ? b : a :
        cmp((char *) arr + pvt, (char *) arr + b, context) ? cmp((char *) arr + a, (char *) arr + b, context) ? a : b : pvt;
}

size_t split_range(size_t *arr, size_t cnt, size_t off, size_t tot, size_t stride, bool inst)
{
    size_t ind = 0;
    for (size_t i = cnt; i; i--)
    {
        size_t step = tot / (i * stride) * stride;
        if (!step)
        {
            if (inst) break;
            else continue;
        }
        arr[ind++] = off;
        off += step;
        tot -= step;
    }
    return ind;
}

Dsize_t quick_sort_partition(void *restrict arr, size_t a, size_t b, size_t sz, cmp_callback cmp, void *context, void *restrict swp, size_t stride)
{
    size_t diff = b - a, pvtl[8], pvt;
    if (split_range(pvtl, countof(pvtl), a, diff, stride, 1) < countof(pvtl))
    {
        pvt = a + (diff / stride >> 1) * stride;
        median_of_three_swap(arr, a, pvt, b, sz, cmp, context, swp);
        a += stride;
        b -= stride;
    }
    else pvt = median_of_three(arr,
        median_of_three(arr, pvtl[0], pvtl[1], pvtl[2], cmp, context),
        median_of_three(arr, pvtl[3], pvtl[4], pvtl[5], cmp, context),
        median_of_three(arr, pvtl[6], pvtl[7], b, cmp, context), cmp, context);
    do {
        while (cmp((char *) arr + pvt, (char *) arr + a, context)) a += stride;
        while (cmp((char *) arr + b, (char *) arr + pvt, context)) b -= stride;
        if (a == b)
        {
            a += stride;
            b -= stride;
            break;
        }
        else if (a > b) break;
        swap((char *) arr + a, (char *) arr + b, swp, sz);
        if (a == pvt) pvt = b;
        else if (pvt == b) pvt = a;
        a += stride;
        b -= stride;
    } while (a <= b);
    return wide(a, b);
}

static void quick_sort_impl(void *restrict arr, size_t tot, size_t sz, cmp_callback cmp, void *context, void *restrict swp, size_t stride, size_t lin_cutoff, lin_sort_callback lin_sort, size_t log_cutoff, sort_callback log_sort)
{
    Dsize_t *stk = Alloca(log_cutoff * sizeof(*stk));
    size_t frm = 0, a = 0, b = tot - stride;
    for (;;)
    {
        Dsize_t prt = quick_sort_partition(arr, a, b, sz, cmp, context, swp, stride);
        size_t pa = lo(prt), pb = hi(prt);
        if (pb - a < lin_cutoff)
        {
            size_t tmp = pb - a + stride;
            lin_sort((char *) arr + a, tmp, sz, cmp, context, swp, stride, tmp);
            if (b - pa < lin_cutoff)
            {
                tmp = b - pa + stride;
                lin_sort((char *) arr + pa, tmp, sz, cmp, context, swp, stride, tmp);
                if (!frm--) break;
                Dsize_t pop = stk[frm];
                a = lo(pop);
                b = hi(pop);
            }
            else a = pa;
        }
        else if (b - pa < lin_cutoff)
        {
            size_t tmp = b - pa + stride;
            lin_sort((char *) arr + pa, tmp, sz, cmp, context, swp, stride, tmp);
            b = pb;
        }
        else
        {
            if (pb - a > b - pa)
            {
                if (frm < log_cutoff) stk[frm++] = wide(a, pb);
                else log_sort((char *) arr + a, pb - a + stride, sz, cmp, context, swp, stride, lin_cutoff, lin_sort);
                a = pa;
            }
            else
            {
                if (frm < log_cutoff) stk[frm++] = wide(pa, b);
                else log_sort((char *) arr + pa, b - pa + stride, sz, cmp, context, swp, stride, lin_cutoff, lin_sort);
                b = pb;
            }
        }
    }
}

void quick_sort(void *restrict arr, size_t cnt, size_t sz, cmp_callback cmp, void *context, void *restrict swp, size_t stride)
{
    if (cnt < 2) return;
    if (cnt > 2)
    {
        size_t tot = cnt * stride;
        if (cnt > QUICK_SORT_CUTOFF)
        {
            size_t lin_cutoff = QUICK_SORT_CUTOFF * stride, log_cutoff = sub_sat(ulog2(cnt, 1), LOG2(QUICK_SORT_CUTOFF, 0) << 1);
#       ifdef QUICK_SORT_CACHED
            quick_sort_impl(arr, tot, sz, cmp, context, swp, stride, lin_cutoff, insertion_sort_impl, log_cutoff, comb_sort_impl);
#       else
            quick_sort_impl(arr, tot, sz, cmp, context, swp, stride, lin_cutoff, lin_sort_stub, log_cutoff, comb_sort_impl);
            insertion_sort_impl(arr, tot, sz, cmp, context, swp, stride, lin_cutoff);
#       endif 
        }
        else insertion_sort_impl(arr, tot, sz, cmp, context, swp, stride, tot);
    }
    else if (cmp(arr, (char *) arr + stride, context)) swap(arr, (char *) arr + stride, swp, sz);
}

void sort_unique(void *restrict arr, size_t *p_cnt, size_t sz, cmp_callback cmp, void *context, void *restrict swp, size_t stride)
{
    size_t cnt = *p_cnt;
    quick_sort(arr, cnt, sz, cmp, context, swp, stride);
    if (!cnt) return;
    size_t ucnt = 1, tot = cnt * stride;
    for (size_t i = stride, j = i; i < tot; i += stride)
    {
        if (!cmp((char *) arr + i, (char *) arr + i - stride, context)) continue;
        if (i > j) memcpy((char *) arr + j, (char *) arr + i, sz);
        j += stride;
        ucnt++;
    }
    *p_cnt = ucnt;
}

bool binary_search(size_t *p_ind, const void *key, const void *arr, size_t cnt, size_t sz, stable_cmp_callback cmp, void *context, enum binary_search_flags flags)
{
    if (!cnt) return 0;
    bool rightmost = flags & BINARY_SEARCH_RIGHTMOST;
    size_t left = 0, right = cnt - 1;
    int res = 0;
    while (left < right)
    {
        size_t mid = left + ((right - left + !rightmost) >> 1);
        res = cmp(key, (char *) arr + sz * mid, context);
        if (res > 0) left = mid + rightmost;
        else if (res < 0) right = mid - !rightmost;
        else if (flags & BINARY_SEARCH_CRITICAL)
        {
            if (rightmost) left = mid;
            else right = mid;
            while (left < right)
            {
                mid = left + ((right - left + rightmost) >> 1);
                res = cmp(key, (char *) arr + sz * mid, context);
                if (res > 0) left = mid + 1;
                else if (res < 0) right = mid - 1;
                else if (rightmost) left = mid;
                else right = mid;
            }
            *p_ind = left;
            return 1;
        }
        else
        {
            *p_ind = mid;
            return 1;            
        }
    }
    if (!(flags & BINARY_SEARCH_IMPRECISE) && cmp(key, (char *) arr + sz * left, context)) return 0;
    *p_ind = left;
    return 1;
}

// Hash table
enum {
    FLAG_REMOVED = 1,
    FLAG_NOT_EMPTY = 2
};

struct array_result hash_table_init(struct hash_table *tbl, size_t cnt, size_t szk, size_t szv)
{
    if (!cnt)
    {
        *tbl = (struct hash_table) { .lcap = umax(tbl->lcap), .lhint = umax(tbl->lcap) };
        return (struct array_result) { .status = ARRAY_SUCCESS_UNTOUCHED };
    }
    size_t lcnt = ulog2(cnt, 1);
    if (lcnt == SIZE_MAX) return (struct array_result) { .error = ARRAY_OVERFLOW };
    cnt = (size_t) 1 << lcnt;
    struct array_result res = array_init(&tbl->key, NULL, cnt, szk, 0, ARRAY_STRICT);
    if (res.status)
    {
        size_t tot = res.tot;
        res = array_init(&tbl->val, NULL, cnt, szv, 0, ARRAY_STRICT);
        if (res.status)
        {
            tot = add_sat(tot, res.tot);
            res = array_init(&tbl->bits, NULL, UINT8_CNT(cnt), sizeof(*tbl->bits), 0, ARRAY_CLEAR | ARRAY_STRICT);
            if (res.status)
            {
                tbl->cnt = 0;
                tbl->lcap = tbl->lhint = lcnt;
                return (struct array_result) { .status = ARRAY_SUCCESS, .tot = add_sat(tot, res.tot) };
            }
            free(tbl->val);
        }
        free(tbl->key);
    }
    return res;
}

void hash_table_close(struct hash_table *tbl)
{
    free(tbl->bits);
    free(tbl->val);
    free(tbl->key);
}

bool hash_table_search(struct hash_table *tbl, size_t *p_h, const void *key, size_t szk, cmp_callback eq, void *context)
{
    if (!tbl->cnt) return 0;
    size_t msk = ((size_t) 1 << tbl->lcap) - 1, h = uhash(*p_h) & msk;
    for (size_t i = h;;)
    {
        if (!bt(tbl->bits, h)) return 0;
        if (eq((char *) tbl->key + h * szk, key, context)) break;
        h = (h + 1) & msk;
        if (h == i) return 0;
    }
    *p_h = h;
    return 1;
}

void hash_table_dealloc(struct hash_table *tbl, size_t h, size_t szk, size_t szv, hash_callback hash, void *context)
{
    if (!tbl->cnt) return;
    size_t msk = ((size_t) 1 << tbl->lcap) - 1;
    for (size_t i = (h + 1) & msk; i != h && bt(tbl->bits, i); i = (i + 1) & msk)
    {
        size_t j = uhash(hash((char *) tbl->key + i * szk, context)) & msk;
        if (h < i ? h < j && j <= i : j <= i || h < j) continue;
        memcpy((char *) tbl->key + h * szk, (char *) tbl->key + i * szk, szk);
        memcpy((char *) tbl->val + h * szv, (char *) tbl->val + i * szv, szv);
        h = i;
    }
    br(tbl->bits, h);
    tbl->cnt--;
}

bool hash_table_remove(struct hash_table *tbl, size_t h, const void *key, size_t szk, size_t szv, hash_callback hash, cmp_callback eq, void *context)
{
    if (!(hash_table_search(tbl, &h, key, szk, eq, context))) return 0;
    hash_table_dealloc(tbl, h, szk, szv, hash, context);
    return 1;
}

void *hash_table_fetch_key(struct hash_table *tbl, size_t h, size_t szk)
{
    return (char *) tbl->key + h * szk;
}

void *hash_table_fetch_val(struct hash_table *tbl, size_t h, size_t szv)
{
    return (char *) tbl->val + h * szv;
}

static struct array_result hash_table_rehash(struct hash_table *tbl, size_t lcnt, size_t szk, size_t szv, hash_callback hash, void *context, void *restrict swpk, void *restrict swpv)
{
    size_t cnt = (size_t) 1 << lcnt, msk = cnt - 1, lcap = tbl->lcap, cap = lcap < bitsof(lcap) ? (size_t) 1 << lcap : 0;
    uint8_t *bits;
    struct array_result res = array_init(&bits, NULL, UINT8_CNT(cnt), sizeof(*bits), 0, ARRAY_CLEAR | ARRAY_STRICT);
    if (!res.status) return res;
    for (size_t i = 0; i < cap; i++)
    {
        if (!btr(tbl->bits, i)) continue;
        for (;;)
        {
            size_t h = uhash(hash((char *) tbl->key + i * szk, context)) & msk;
            for (; bts(bits, h); h = (h + 1) & msk);
            if (h >= cap || !btr(tbl->bits, h))
            {
                memcpy((char *) tbl->key + h * szk, (char *) tbl->key + i * szk, szk);
                memcpy((char *) tbl->val + h * szv, (char *) tbl->val + i * szv, szv);
                break;
            }
            swap((char *) tbl->key + i * szk, (char *) tbl->key + h * szk, swpk, szk);
            swap((char *) tbl->val + i * szv, (char *) tbl->val + h * szv, swpv, szv);
        }
    }
    free(tbl->bits);
    tbl->bits = bits;
    tbl->lcap = lcnt;
    return res;
}

#define HASH_LOAD_FACTOR(X) (((X) >> 1) + ((X) >> 2)) // .75 * (X)

struct array_result hash_table_alloc(struct hash_table *tbl, size_t *p_h, const void *key, size_t szk, size_t szv, hash_callback hash, cmp_callback eq, void *context, void *restrict swpk, void *restrict swpv)
{
    struct array_result res = { .status = 1 };
    size_t cnt = tbl->cnt, lcap = tbl->lcap, cap = lcap < bitsof(lcap) ? (size_t) 1 << lcap : 0; // 'lcap' may be equal to SIZE_MAX
    if (cnt >= HASH_LOAD_FACTOR(cap)) // Extend when the load factor of .75 is reached
    {
        size_t lcap1 = lcap + 1;
        if (lcap1 >= bitsof(lcap1)) return (struct array_result) { .error = ARRAY_OVERFLOW };
        size_t cap1 = (size_t) 1 << lcap1;
        
        res = array_init(&tbl->key, NULL, cap1, szk, 0, ARRAY_STRICT | ARRAY_REALLOC);
        if (!res.status) return res;

        size_t tot = res.tot;
        res = array_init(&tbl->val, NULL, cap1, szv, 0, ARRAY_STRICT | ARRAY_REALLOC);
        res.tot = add_sat(tot, res.tot);
        if (!res.status) return res;

        tot = res.tot;
        res = hash_table_rehash(tbl, lcap1, szk, szv, hash, context, swpk, swpv);
        res.tot = add_sat(tot, res.tot);
        if (!res.status) return res;
        cap = cap1;
    }
    else if ((cap >> 1) > cnt) // Shrink when at least half of the space remains unused
    {
        size_t lcap1 = ulog2(cnt + 1, 1), cap1 = (size_t) 1 << lcap1;
        if (cnt >= HASH_LOAD_FACTOR(cap1)) lcap1++, cap1 <<= 1;
        if ((tbl->lhint >= bitsof(tbl->lhint) || tbl->lhint <= lcap1) && cap1 < cap)
        {
            res = hash_table_rehash(tbl, lcap1, szk, szv, hash, context, swpk, swpv);
            if (!res.status) return res;

            size_t tot = res.tot;
            res = array_init(&tbl->key, NULL, cap1, szk, 0, ARRAY_STRICT | ARRAY_REALLOC | ARRAY_FAILSAFE);
            res.tot = add_sat(tot, res.tot);
            if (!res.status) return res;

            tot = res.tot;
            res = array_init(&tbl->val, NULL, cap1, szv, 0, ARRAY_STRICT | ARRAY_REALLOC | ARRAY_FAILSAFE);
            res.tot = add_sat(tot, res.tot);
            if (!res.status) return res;
            cap = cap1;
        }
        else res.status |= HASH_UNTOUCHED;
    }
    else res.status |= HASH_UNTOUCHED;
    size_t msk = cap - 1, h = uhash(*p_h) & msk;
    for (;; h = (h + 1) & msk)
    {
        if (!bts(tbl->bits, h)) tbl->cnt++;
        else if (eq((char *) tbl->key + h * szk, key, context)) res.status |= HASH_PRESENT;
        else continue;
        break;
    }
    *p_h = h;
    return res;
}

// String pool
size_t stro_hash(const void *Off, void *Str)
{
    return str_hash((const char *) Str + *(size_t *) Off);
}

struct array_result str_pool_init(struct str_pool *pool, size_t cnt, size_t len, size_t szv)
{
    struct array_result res0 = array_init(&pool->buff.str, &pool->buff.cap, len, sizeof(*pool->buff.str), 0, 0);
    if (!res0.status) return res0;
    pool->buff.len = 0;
    struct array_result res1 = hash_table_init(&pool->tbl, cnt, sizeof(size_t), szv);
    if (res1.status) return (struct array_result) { .status = ARRAY_SUCCESS, .tot = add_sat(res0.tot, res1.tot) };
    free(pool->buff.str);
    return res1;
}

void str_pool_close(struct str_pool *pool)
{
    free(pool->buff.str);
    hash_table_close(&pool->tbl);
}

// Warning! String 'str' should be null-terminated
struct array_result str_pool_insert(struct str_pool *pool, const char *key, size_t len, size_t *p_off, size_t szv, void *p_val, void *restrict swpv)
{
    size_t h = str_hash(key), cnt = pool->tbl.cnt, swpk;
    struct array_result res0 = hash_table_alloc(&pool->tbl, &h, key, sizeof(size_t), szv, stro_hash, stro_str_eq_unsafe, pool->buff.str, &swpk, swpv);
    if (!res0.status) return res0;
    if (res0.status & HASH_PRESENT)
    {
        if (p_off) *p_off = *(size_t *) hash_table_fetch_key(&pool->tbl, h, sizeof(size_t));
        if (p_val) *(void **) p_val = hash_table_fetch_val(&pool->tbl, h, szv);
        return res0;
    }
    // Position should be saved before the buffer update
    size_t off = pool->buff.len + (len && cnt);
    struct array_result res1 = buff_append(&pool->buff, key, len, (cnt ? BUFFER_INIT | BUFFER_TERM : BUFFER_TERM) | BUFFER_UNSAFE_AWARE);
    if (!res1.status)
    {
        hash_table_dealloc(&pool->tbl, h, sizeof(size_t), szv, stro_hash, pool->buff.str);
        return res1;
    }
    *(size_t *) hash_table_fetch_key(&pool->tbl, h, sizeof(size_t)) = off;
    if (p_off) *p_off = off;
    if (p_val) *(void **) p_val = hash_table_fetch_val(&pool->tbl, h, szv);
    return (struct array_result) { .status = res0.status & res1.status, .tot = add_sat(res0.tot, res1.tot) };
}

bool str_pool_fetch(struct str_pool *pool, const char *str, size_t szv, void *p_val)
{
    size_t h = str_hash(str);
    if (!hash_table_search(&pool->tbl, &h, str, sizeof(size_t), stro_str_eq_unsafe, pool->buff.str)) return 0;
    if (p_val) *(void **) p_val = hash_table_fetch_val(&pool->tbl, h, szv);
    return 1;
}
