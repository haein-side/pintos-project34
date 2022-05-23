#ifndef THREADS_SYNCH_H
#define THREADS_SYNCH_H

#include <list.h>
#include <stdbool.h>

/* A counting semaphore. */
struct semaphore {
	unsigned value;             /* 공유자원의 갯수를 의미한다. */
	struct list waiters;        /* 공유자원을 사용하기 위해 대기하는 waiters의 리스트 */
};

void sema_init (struct semaphore *, unsigned value);
void sema_down (struct semaphore *);
bool sema_try_down (struct semaphore *);
void sema_up (struct semaphore *);
void sema_self_test (void);
bool cmp_sem_priority (const struct list_elem *a, const struct list_elem *b, void *aux);

/* Lock. */
struct lock {
	struct thread *holder;      /* 현재 lock을 가지고 있는 thread 정보 */
	struct semaphore semaphore; /* 0과 1로 이루어진 세마포어 */
};

void lock_init (struct lock *);
void lock_acquire (struct lock *);
bool lock_try_acquire (struct lock *);
void lock_release (struct lock *);
bool lock_held_by_current_thread (const struct lock *);

/* Condition variable. */
struct condition {
	struct list waiters;        /* condition variabels가 만족하기를 기다리는 waiters의 리스트를 가진다. */
};

void cond_init (struct condition *);
void cond_wait (struct condition *, struct lock *);
void cond_signal (struct condition *, struct lock *);
void cond_broadcast (struct condition *, struct lock *);

/* Optimization barrier.
 *
 * The compiler will not reorder operations across an
 * optimization barrier.  See "Optimization Barriers" in the
 * reference guide for more information.*/
#define barrier() asm volatile ("" : : : "memory")

#endif /* threads/synch.h */
