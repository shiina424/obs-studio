#include "task.h"
#include "bmem.h"
#include "threading.h"
#include "circlebuf.h"

struct os_task_thread {
	pthread_t thread;
	os_sem_t *sem;
	long id;
	bool valid;

	pthread_mutex_t mutex;
	struct circlebuf tasks;
};

struct os_task_info {
	os_task_t task;
	void *param;
};

static THREAD_LOCAL bool exit_thread = false;
static THREAD_LOCAL long thread_id = 0;
static volatile long thread_id_counter = 1;

static void *tiny_tubular_task_thread(void *param);

os_task_thread_t *os_task_thread_create()
{
	struct os_task_thread *tt = bzalloc(sizeof(*tt));
	tt->valid = true;
	tt->id = os_atomic_inc_long(&thread_id_counter);

	if (pthread_mutex_init(&tt->mutex, NULL) != 0)
		goto fail1;
	if (os_sem_init(&tt->sem, 0) != 0)
		goto fail2;
	if (pthread_create(&tt->thread, NULL, tiny_tubular_task_thread, tt) !=
	    0)
		goto fail3;

	return tt;

fail3:
	os_sem_destroy(tt->sem);
fail2:
	pthread_mutex_destroy(&tt->mutex);
fail1:
	bfree(tt);
	return NULL;
}

bool os_task_thread_queue_task(os_task_thread_t *tt, os_task_t task,
			       void *param)
{
	struct os_task_info ti = {
		task,
		param,
	};

	if (!tt || !tt->valid)
		return false;

	pthread_mutex_lock(&tt->mutex);
	circlebuf_push_back(&tt->tasks, &ti, sizeof(ti));
	pthread_mutex_unlock(&tt->mutex);
	os_sem_post(tt->sem);
	return true;
}

static void stop_thread(void *unused)
{
	exit_thread = true;
	UNUSED_PARAMETER(unused);
}

void os_task_thread_destroy(os_task_thread_t *tt)
{
	if (!tt)
		return;

	os_task_thread_queue_task(tt, stop_thread, NULL);
	if (tt->valid)
		pthread_join(tt->thread, NULL);
	os_sem_destroy(tt->sem);
	pthread_mutex_destroy(&tt->mutex);
	circlebuf_free(&tt->tasks);
	bfree(tt);
}

bool os_task_thread_wait(os_task_thread_t *tt)
{
	if (tt->valid) {
		os_task_thread_queue_task(tt, stop_thread, NULL);
		pthread_join(tt->thread, NULL);
	}

	tt->valid = pthread_create(&tt->thread, NULL, tiny_tubular_task_thread,
				   tt) == 0;
	return tt->valid;
}

bool os_task_thread_inside(os_task_thread_t *tt)
{
	return tt->id == thread_id;
}

static void *tiny_tubular_task_thread(void *param)
{
	struct os_task_thread *tt = param;
	thread_id = tt->id;

	os_set_thread_name(__FUNCTION__);

	while (!exit_thread && os_sem_wait(tt->sem) == 0) {
		struct os_task_info ti;

		pthread_mutex_lock(&tt->mutex);
		circlebuf_pop_front(&tt->tasks, &ti, sizeof(ti));
		if (tt->tasks.size && ti.task == stop_thread) {
			circlebuf_push_back(&tt->tasks, &ti, sizeof(ti));
			circlebuf_pop_front(&tt->tasks, &ti, sizeof(ti));
		}
		pthread_mutex_unlock(&tt->mutex);

		ti.task(ti.param);
	}

	return NULL;
}
