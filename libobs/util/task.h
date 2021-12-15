#pragma once

#include "c99defs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct os_task_thread;
typedef struct os_task_thread os_task_thread_t;

typedef void (*os_task_t)(void *param);

EXPORT os_task_thread_t *os_task_thread_create();
EXPORT bool os_task_thread_queue_task(os_task_thread_t *tt, os_task_t task,
				      void *param);
EXPORT void os_task_thread_destroy(os_task_thread_t *tt);
EXPORT bool os_task_thread_wait(os_task_thread_t *tt);
EXPORT bool os_task_thread_inside(os_task_thread_t *tt);

#ifdef __cplusplus
}
#endif
