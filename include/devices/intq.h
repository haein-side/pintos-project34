#ifndef DEVICES_INTQ_H
#define DEVICES_INTQ_H

#include "threads/interrupt.h"
#include "threads/synch.h"

/* 인터럽트 큐는 커널 스레드와 외부 인터럽트 핸들러 간에 공유되는 circular 버퍼이다.
  인터럽트 큐 함수는 커널 스레드 또는 외부 인터럽트 핸들러에서 호출할 수 있다.
   intq_init()를 제외하고, 인터럽트는 두 경우 모두 해제되어 있어야 합니다.
   인터럽트 큐는 "monitor"의 구조이다.
   threads/synch.h의 잠금 및 조건 변수는 이 경우에 사용할 수 없는데,
   인터럽트 핸들러로부터가 아니라, 커널 스레드로부터만 보호할 수 있기 때문 */

/* Queue buffer size, in bytes. */
#define INTQ_BUFSIZE 64

/* A circular queue of bytes. */
struct intq {
	/* Waiting threads. */
	struct lock lock;           /* Only one thread may wait at once. */
	struct thread *not_full;    /* Thread waiting for not-full condition. */
	struct thread *not_empty;   /* Thread waiting for not-empty condition. */

	/* Queue. */
	uint8_t buf[INTQ_BUFSIZE];  /* Buffer. */
	int head;                   /* New data is written here. */
	int tail;                   /* Old data is read here. */
};

void intq_init (struct intq *);
bool intq_empty (const struct intq *);
bool intq_full (const struct intq *);
uint8_t intq_getc (struct intq *);
void intq_putc (struct intq *, uint8_t);

#endif /* devices/intq.h */
