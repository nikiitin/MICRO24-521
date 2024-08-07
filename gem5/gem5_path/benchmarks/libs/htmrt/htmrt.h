
#ifndef HTMRT_H
#define HTMRT_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include "thread_context.h"

void initTransactionsGlobals(int num_threads);
void deleteTransactionsGlobals(void);

void beginTransaction(long tag, _tm_thread_context_t *thread_context);
void commitTransaction(long tag, _tm_thread_context_t *thread_context);
void cancelTransactionWithAbortCode(long abort_code);
void cancelTransaction();

#ifdef __cplusplus
}
#endif

#endif
