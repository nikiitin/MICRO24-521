// Abort handlers
#include "thread_context.h"

void initGlobals(int num_threads);
void deleteGlobals ();

void beginTransaction(long tag, _tm_thread_context_t *thread_context);
void commitTransaction(long tag, _tm_thread_context_t *thread_context);
void cancelTransactionWithAbortCode(long abort_code);
void cancelTransaction();

long inTransaction(void);

