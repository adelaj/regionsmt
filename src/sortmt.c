#include "np.h"
#include "common.h"
#include "memory.h"
#include "sort.h"
#include "sortmt.h"
#include "threadpool.h"

#include <string.h>
#include <stdlib.h>

struct sort_context {
    void *arr, *temp;
    size_t cnt, sz, stride;
    cmp_callback cmp;
    void *context;
};

struct sort_args {
    size_t off, len;
};

struct merge_args {
    size_t off, mid, len;
};

/*
struct sort_mt
{
    struct sort_context context;
    struct sort_args *s_args;
    struct merge_args *m_args;
    struct task *tasks;
    uint8_t *sync; // Last two bits in 'res->sync' are not in use!
    size_t *args;
};
*/

static void merge_arrays(void *restrict arr, void *restrict temp, size_t mid, size_t cnt, size_t sz, cmp_callback cmp, void *context)
{
    const size_t tot = cnt * sz, pvt = mid * sz;
    for (size_t i = 0, j = pvt, k = 0; k < tot; k += sz)
    {
        if (i < pvt && (j == tot || cmp((char *) arr + j, (char *) arr + i, context))) memcpy((char *) temp + k, (char *) arr + i, sz), i += sz;
        else memcpy((char *) temp + k, (char *) arr + j, sz), j += sz;
    }
    memcpy(arr, temp, tot);
}

static bool merge_thread_proc(void *Args, void *Context)
{
    struct merge_args *restrict args = Args;
    struct sort_context *restrict context = Context;
    const size_t off = args->off * context->sz;
    merge_arrays((char *) context->arr + off, (char *) context->temp + off, args->mid, args->len, context->sz, context->cmp, context->context);
    return 1;
}

static bool sort_thread_proc(void *Args, void *Context)
{
    struct sort_args *restrict args = Args;
    struct sort_context *restrict context = Context;
    quick_sort((char *) context->arr + args->off * context->sz, args->len, context->sz, context->cmp, context->context, NULL, context->stride);
    return 1;
}

/*
void sort_mt_dispose(struct sort_mt *arg)
{
    if (!arg) return;    
    free(arg->m_args);
    free(arg->s_args);
    free(arg->tasks);
    free(arg->sync);
    free(arg->args);
    free(arg->context.temp);
    free(arg);
}
*/

struct sort_mt {
    void *arr;
    size_t cnt;
    alignas(Dsize_t) volatile Dsize_t ran[];
};

static unsigned sort_thread(void *Ind, void *Data, void *tls)
{
    (void) tls;
    struct sort_mt *data = Data;
    size_t ind = (size_t) Ind, lev = ulog2(ind, 0) + 1, a, b;
    if (ind)
    {
        // Warning! Acquire semantic is provided by 'Dsize_interlocked_compare_exchange' in task condition
        // This operation need not to be atomic
        Dsize_t ran = data->ran[ind - 1];
        a = lo(ran);
        b = hi(ran);
    }
    else
    {
        a = 0;
        b = data->cnt - 1;
    }


    return 1;
}

bool sort_mt_init(struct thread_pool *pool,  struct log *log)
{
    size_t thread_cnt = thread_pool_get_count(pool), dep = ulog2(thread_cnt, 0), task_cnt = (size_t) 1 << dep, cnt = task_cnt - 1;
    struct sort_mt *data;
    // Note, that aligned allocation below is superfluous on all platforms under consideration
    if (!array_assert(log, CODE_METRIC, array_init_impl(&data, NULL, alignof(struct sort_mt), fam_countof(struct sort_mt, ran, cnt), fam_sizeof(struct sort_mt, ran), fam_diffof(struct sort_mt, ran, cnt), ARRAY_STRICT | ARRAY_ALIGN))) return 0;

}

/*
bool sort_mt_init(void *arr, size_t cnt, size_t sz, cmp_callback cmp, void *context, struct thread_pool *pool, struct sort_mt_sync *sync)
{
    struct sort_mt_sync *snc = sync ? sync : &(struct sort_mt_sync) { 0 };
    size_t dep = size_bsr(thread_pool_get_count(pool)); // Max value of 'dep' is 63 under x86_64 and 31 under x86 
    const size_t s_cnt = (size_t) 1 << dep, m_cnt = s_cnt - 1;

    struct sort_mt *res;
    if ((res = calloc(1, sizeof(*res))) != NULL &&
        (res->sync = calloc(NIBBLE_CNT(s_cnt), 1)) != NULL && // No overflow happens (cf. previous comment), s_cnt > 0
        array_init(&res->s_args, NULL, s_cnt, sizeof(*res->s_args), 0, ARRAY_STRICT).status &&
        array_init(&res->m_args, NULL, m_cnt, sizeof(*res->m_args), 0, ARRAY_STRICT).status &&
        array_init(&res->context.temp, NULL, cnt, sz, 0, ARRAY_STRICT).status)
    {
        res->context = (struct sort_context) { .arr = arr, .cnt = cnt, .sz = sz, .cmp = cmp, .context = context };
        res->s_args[0] = (struct sort_args) { .off = 0, .len = cnt };

        for (size_t i = 0; i < dep; i++)
        {
            const size_t jm = (size_t) 1 << i, jr = (size_t) 1 << (dep - i - 1), jo = s_cnt - (jm << 1), ji = jr << 1;
            for (size_t j = 0, k = 0; j < jm; j++, k += ji)
            {
                const size_t l = res->s_args[k].len, hl = l >> 1;
                res->s_args[k].len = hl;
                res->s_args[k + jr] = (struct sort_args) { res->s_args[k].off + hl, l - hl };
                res->m_args[jo + j] = (struct merge_args) { res->s_args[k].off, hl, l };
            }
        }

        if (m_cnt)
        {
            if (array_init(&res->tasks, NULL, s_cnt + m_cnt, sizeof(*res->tasks), 0, ARRAY_STRICT).status &&
                array_init(&res->args, NULL, s_cnt + m_cnt - 1, sizeof(*res->args), 0, ARRAY_STRICT).status)
            {
                // Assigning sorting tasks
                for (size_t i = 0; i < s_cnt; i++)
                {
                    res->args[i] = i;
                    res->tasks[i] = (struct task) {
                        .callback = sort_thread_proc,
                        .cond = snc->cond,
                        //.aggr = bs_interlocked_p,
                        .arg = &res->s_args[i],
                        .context = &res->context,
                        //.cond_mem = snc->cond_mem,
                        //.aggr_mem = res->sync,
                        //.cond_arg = snc->cond_arg,
                        //.aggr_arg = &res->args[i]
                    };
                }

                // Assigning merging tasks (except the last task)
                for (size_t i = 0, j = s_cnt; i < m_cnt - 1; i++, j++)
                {
                    res->args[j] = j;
                    res->tasks[j] = (struct task) {
                        .callback = merge_thread_proc,
                        //.cond = bt2_acquire_p,
                        //.aggr = bs_interlocked_p,
                        .arg = &res->m_args[i],
                        .context = &res->context,
                        //.cond_mem = res->sync,
                        //.aggr_mem = res->sync,
                        //.cond_arg = &res->args[i << 1],
                        //.aggr_arg = &res->args[j]
                    };
                }

                // Last merging task requires special handling
                res->tasks[s_cnt + m_cnt - 1] = (struct task) {
                    .callback = merge_thread_proc,
                    //.cond = bt2_acquire_p,
                    .aggr = snc->a_succ,
                    .arg = &res->m_args[m_cnt - 1],
                    .context = &res->context,
                    //.cond_mem = res->sync,
                    //.aggr_mem = snc->a_succ_mem,
                    //.cond_arg = &res->args[(m_cnt - 1) << 1],
                    //.aggr_arg = snc->a_succ_arg
                };

                //if (thread_pool_enqueue_tasks(pool, res->tasks, m_cnt + s_cnt, 1)) return res;
            }            
        }
        else
        {
            if ((res->tasks = malloc(sizeof(*res->tasks))) != NULL)
            {
                *res->tasks = (struct task)
                {
                    .callback = sort_thread_proc,
                    .cond = snc->cond,
                    .aggr = snc->a_succ,
                    .arg = &res->s_args[0],
                    .context = &res->context,
                    //.cond_mem = snc->cond_mem,
                    //.aggr_mem = snc->a_succ_mem,
                    //.cond_arg = snc->cond_arg,
                    //.aggr_arg = snc->a_succ_arg
                };

                //if (thread_pool_enqueue_tasks(pool, res->tasks, 1, 1)) return res;
            }
        }
        sort_mt_dispose(res);
    }   
    return NULL;
}
*/