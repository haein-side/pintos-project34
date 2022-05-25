#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#include "threads/fixed_point.h" // project1_advanced_scheduler
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* Random value for basic thread
   Do not modify this value. */
#define THREAD_BASIC 0xd42df210

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* sleep 상태의 스레드들을 저장하기 위한 리스트 */
static struct list sleep_list;
static int64_t next_tick_to_awake;	// sleep list에서 맨 처음으로 awake할 스레드의 tick의 값

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Thread destruction requests */
static struct list destruction_req;

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;	/* 1: mlfqs, 0: rr*/

/* project1_advanced_scheduler */
#define NICE_DEFAULT 0
#define RECENT_CPU_DEFAULT 0
#define LOAD_AVG_DEFAULT 0
int load_avg;


static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static void do_schedule(int status);
static void schedule (void);
static tid_t allocate_tid (void);
void test_max_priority(void);
bool cmp_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);

/* Returns true if T appears to point to a valid thread. */
#define is_thread(t) ((t) != NULL && (t)->magic == THREAD_MAGIC)

/* Returns the running thread.
 * Read the CPU's stack pointer `rsp', and then round that
 * down to the start of a page.  Since `struct thread' is
 * always at the beginning of a page and the stack pointer is
 * somewhere in the middle, this locates the curent thread. */
#define running_thread() ((struct thread *) (pg_round_down (rrsp ())))


// Global descriptor table for the thread_start.
// Because the gdt will be setup after the thread_init, we should
// setup temporal gdt first.
static uint64_t gdt[3] = { 0, 0x00af9a000000ffff, 0x00cf92000000ffff };

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) {
	ASSERT (intr_get_level () == INTR_OFF);

	/* Reload the temporal gdt for the kernel
	 * This gdt does not include the user context.
	 * The kernel will rebuild the gdt with user context, in gdt_init (). */
	struct desc_ptr gdt_ds = {
		.size = sizeof (gdt) - 1,
		.address = (uint64_t) gdt
	};
	lgdt (&gdt_ds);

	/* Init the globla thread context */
	lock_init (&tid_lock);
	list_init (&ready_list);
	list_init (&destruction_req);
	list_init (&sleep_list);	// sleep 스레드들을 연결해놓은 리스트를 초기화 한다.
	next_tick_to_awake = INT64_MAX;

	/* Set up a thread structure for the running thread. */
	initial_thread = running_thread ();
	init_thread (initial_thread, "main", PRI_DEFAULT);
	initial_thread->status = THREAD_RUNNING;
	initial_thread->tid = allocate_tid ();
}

// readylist에서 가장 먼저 나올 스레드의 틱 값을 업데이트 시켜주기 위한 함수이다.
void update_next_tick_to_awake(int64_t ticks){
	// thread_sleep에서 호출된 함수로, tick으로 받아온 변수의 값이 이전 next_tick보다 작으면 그 tick값을 next_tick값으로 바꿔주고 원래의 값이 더 작다면 기존 값을 유지한다.
	next_tick_to_awake = (next_tick_to_awake>ticks) ? ticks : next_tick_to_awake;

}

int64_t get_next_tick_to_awake(void){
	return next_tick_to_awake;
}

// timer sleep에서 인자로 받은 (start + ticks) 으로 sleep을 실행한다.
void thread_sleep(int64_t ticks){
	// 현재 실행되고 있는 스레드에 대한 작업이기 떄문에 불러온다
	struct thread*  cur = thread_current();

	enum intr_level old_level;
	ASSERT(!intr_context());
	old_level = intr_disable();

	ASSERT(cur != idle_thread);
	

	cur->wakeup_tick = ticks;	// 현재 running중인 쓰레드의 wakeup 틱을 timer sleep값으로 업데이트 시켜놓고,
	update_next_tick_to_awake(cur->wakeup_tick);	// next_tick_to_awake을 업데이트 시켜준다.
	list_push_back(&sleep_list, &cur->elem);	// sleep_list에 추가

	// 스레드를 sleep 시킨다.
	thread_block();

	// 인터럽트 원복
	intr_set_level(old_level);
}

// awake는 timet interrupt가 발생할 때마다 tick과 next tick to awake와 비교하여 실행한다.
void thread_awake(int64_t ticks){
	// sleeplist의 head를 가져온다
	struct list_elem* cur = list_begin(&sleep_list);
	struct thread* t;
	next_tick_to_awake = INT64_MAX;

	// sleep list의 끝까지 순회한다.
	while(cur != list_end(&sleep_list)){
		// list_entry 해당 구조체의 시작 주소
		t = list_entry(cur, struct thread, elem);
		// 리스트 안에 구조체의 깨울 시간이 지났다면,
		if(ticks >= t->wakeup_tick){
			cur = list_remove(&t->elem);
			thread_unblock(t);
		}
		else{
			cur = list_next(cur);
			update_next_tick_to_awake(t->wakeup_tick);
		}
		// next_tick이 바뀌었을 수 있으므로 업데이트
	}
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
// Idle 스레드를 만들고, 선점 스레드 스케쥴링을 시작한다.
void
thread_start (void) {
	/* Create the idle thread. */
	struct semaphore idle_started;
	sema_init (&idle_started, 0);

	// idle 스레드를 만들고 맨 처음 ready queue에 들어간다.
	// semaphore를 1로 up 시켜 공유 자원의 접근을 가능하게 한 다음 바로 block 된다.
	thread_create ("idle", PRI_MIN, idle, &idle_started);
	load_avg= LOAD_AVG_DEFAULT;		/* project1_advanced_scheduler */

	/* Start preemptive thread scheduling. */
	// thread_create(idle) 에서 disable했던 인터럽트 상태를 enable로 만든다.
	// 이제 스레드에서 스케쥴링이 가능하다. 인터럽트가 가능하므로
	intr_enable ();

	/* Wait for the idle thread to initialize idle_thread. */
	sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) {
	struct thread *t = thread_current ();

	/* Update statistics. */
	if (t == idle_thread)
		idle_ticks++;
#ifdef USERPROG
	else if (t->pml4 != NULL)
		user_ticks++;
#endif
	else
		kernel_ticks++;

	/* Enforce preemption. */
	if (++thread_ticks >= TIME_SLICE)	// thread_ticks는 맨처음 schedule()에서 0으로 만들어준다.
		intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void) {
	printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
			idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
   // 새 커널 스레드를 만들고 바로 ready queue에 넣어준다.
tid_t
thread_create (const char *name, int priority,
		thread_func *function, void *aux) {
	struct thread *t;
	tid_t tid;

	ASSERT (function != NULL);

	/* Allocate thread. */
	t = palloc_get_page (PAL_ZERO);
	if (t == NULL)
		return TID_ERROR;

	/* Initialize thread. */
	init_thread (t, name, priority);
	tid = t->tid = allocate_tid ();

	/* Call the kernel_thread if it scheduled.
	 * Note) rdi is 1st argument, and rsi is 2nd argument. */
	t->tf.rip = (uintptr_t) kernel_thread;
	t->tf.R.rdi = (uint64_t) function;
	t->tf.R.rsi = (uint64_t) aux;
	t->tf.ds = SEL_KDSEG;
	t->tf.es = SEL_KDSEG;
	t->tf.ss = SEL_KDSEG;
	t->tf.cs = SEL_KCSEG;
	t->tf.eflags = FLAG_IF;

	/* Add to run queue. */
	thread_unblock (t);
	// 추가한 부분
	test_max_priority();

	return tid;
}

void test_max_priority(void) {
	int pri = thread_get_priority();
	struct list_elem *cur = list_begin(&ready_list);
	struct thread *t1;
	t1 = list_entry(cur, struct thread, elem);

	if (pri < t1->priority)
		thread_yield();
}

bool cmp_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
	struct thread *p1;
	struct thread *p2;
	p1 = list_entry(a, struct thread, elem);
	p2 = list_entry(b, struct thread, elem);

	if (p1->priority > p2->priority)
		return 1;
	else
		return 0;
}




/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) {
	ASSERT (!intr_context ());
	ASSERT (intr_get_level () == INTR_OFF);
	thread_current ()->status = THREAD_BLOCKED;
	schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) {
	enum intr_level old_level;

	ASSERT (is_thread (t));

	old_level = intr_disable ();
	ASSERT (t->status == THREAD_BLOCKED);
	list_insert_ordered(&ready_list, &t->elem, cmp_priority, NULL);
	t->status = THREAD_READY;
	intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void) {
	return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
// 현재 실행 중인 스레드가 문제가 없는지를 확인한 다음 그 스레드를 가리키는 포인터를 반환한다.
struct thread *
thread_current (void) {
	struct thread *t = running_thread ();

	/* Make sure T is really a thread.
	   If either of these assertions fire, then your thread may
	   have overflowed its stack.  Each thread has less than 4 kB
	   of stack, so a few big automatic arrays or moderate
	   recursion can cause stack overflow. */
	ASSERT (is_thread (t));
	ASSERT (t->status == THREAD_RUNNING);

	return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) {
	return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) {
	ASSERT (!intr_context ());

#ifdef USERPROG
	process_exit ();
#endif

	/* Just set our status to dying and schedule another process.
	   We will be destroyed during the call to schedule_tail(). */
	intr_disable ();
	do_schedule (THREAD_DYING);
	NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) {
	struct thread *curr = thread_current ();
	enum intr_level old_level;

	// 외부 인터럽트를 수행중이라면 종료. 외부 인터럽트는 인터럽트 당하면 안된다.
	ASSERT (!intr_context ());

	old_level = intr_disable ();	// timer인터럽트나 i/o 인터럽트같은 것들를 disable한다.
	// 만약 현재 스레드가 Idle 스레드가 아니라면 ready queue에 다시 담는다.
	if (curr != idle_thread)
		list_insert_ordered(&ready_list, &curr->elem, cmp_priority, NULL);
	// 현재 스레드가 idle이라면 ready queue에 담을필요가 없다. 어차피 static으로 선언되어 있어, 필요할 때 불러올 수 있다.
	do_schedule (THREAD_READY);
	intr_set_level (old_level);
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) {	
	if (!thread_mlfqs) {
		thread_current ()->init_priority = new_priority;   // 새로운 우선순위로 조정
		refresh_priority();      // priority-donation 관련 //donation으로 priority가 변경될 수 있으므로 확인
		test_max_priority();   // priority-Synchronization 관련 // ready_list에서 우선순위가 가장 높은 스레드와 현재 스레드의 우선순위 비교하여, 현재 스레드가 낮다면 thread_yield
	}
}
/* Returns the current thread's priority. */

int
thread_get_priority (void) {
	return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice UNUSED) {
	enum intr_level old_level = intr_disable ();
	if (nice) 
		thread_current()->nice = nice;
	mlfqs_priority(thread_current());
	intr_set_level (old_level);
	test_max_priority();
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) {
	enum intr_level old_level = intr_disable ();
	int nice_value = thread_current()->nice;
	intr_set_level (old_level);
	return nice_value;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) {
	enum intr_level old_level = intr_disable ();
	int load_avg_value = fp_to_int_round(load_avg * 100); //계산식
	intr_set_level (old_level);
	return load_avg_value;
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) {
	enum intr_level old_level = intr_disable ();
	struct thread *cur = thread_current();
	int recent_cpu_value = fp_to_int_round(cur->recent_cpu * 100);
	intr_set_level (old_level);
	return recent_cpu_value;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
   // 어떤 스레드들도 실행되고 있지 않을 때 실행되는 쓰레드. 맨 처음 thread_start()가 호출될 때 ready queue에 먼저 들어가 있는다.
static void
idle (void *idle_started_ UNUSED) {
	struct semaphore *idle_started = idle_started_;

	idle_thread = thread_current ();	// 현재 들고 있는 스레드가 idle밖에 없다
	sema_up (idle_started);		// semaphore의 값을 1로 만들어 줘서 공유 자원의 공유(인터럽트) 가능

	for (;;) {
		/* Let someone else run. */
		intr_disable ();	// 자기 자신(idle)을 block해주기 전까지 인터럽트 당하면 안되므로 먼저 disable 한다.
		thread_block ();	// 자기 자신을 block 한다.

		/* Re-enable interrupts and wait for the next one.

		   The `sti' instruction disables interrupts until the
		   completion of the next instruction, so these two
		   instructions are executed atomically.  This atomicity is
		   important; otherwise, an interrupt could be handled
		   between re-enabling interrupts and waiting for the next
		   one to occur, wasting as much as one clock tick worth of
		   time.

		   See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
		   7.11.1 "HLT Instruction". */
		asm volatile ("sti; hlt" : : : "memory");
	}
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) {
	ASSERT (function != NULL);

	intr_enable ();       /* The scheduler runs with interrupts off. */
	function (aux);       /* Execute the thread function. */
	thread_exit ();       /* If function() returns, kill the thread. */
}


/* Does basic initialization of T as a blocked thread named
   NAME. */
// 만든 스레드를 초기화 한다. 맨처음 스레드의 상태는 blocked이다.
// 커널 스택 포인터 rsp의 위치도 같이 정해준다. rsp의 값은 커널이 함수나 변수를 쌓을수록 점점 작아질 것.
static void
init_thread (struct thread *t, const char *name, int priority) {
	ASSERT (t != NULL);	// 가리키는 공간이 비어있지는 않고 (Null이 아니고)
	ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);	// priority의 값은 제대로 박혀 있고
	ASSERT (name != NULL);	// 이름이 들어갈 공간은 있는지(디버그 용도)

	memset (t, 0, sizeof *t);	// 해당 메모리를 모두 0으로 초기화하고
	t->status = THREAD_BLOCKED;	// 맨처음 스레드 상태는 blocked
	strlcpy (t->name, name, sizeof t->name);	// 이름 써주는 공간
	t->tf.rsp = (uint64_t) t + PGSIZE - sizeof (void *);	// 커널 스택 포인터의 위치를 정해준다. 원래 스레드의 위치 t + 4kb(1<<12) - 포인터 변수 크기
	t->priority = priority;		// 우선순위 정해준다.
	t->magic = THREAD_MAGIC;	// 스레드 공간의 끝주소 값은 동일하기 때문에 기본 값으로 넣어준다.
	t->init_priority = priority;
	t->wait_on_lock = NULL;
	list_init(&t->list_donation);

	t->nice = NICE_DEFAULT;
	t->recent_cpu = RECENT_CPU_DEFAULT;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) {
	if (list_empty (&ready_list))
		return idle_thread;
	else
		return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Use iretq to launch the thread */
void
do_iret (struct intr_frame *tf) {
	__asm __volatile(
			"movq %0, %%rsp\n"
			"movq 0(%%rsp),%%r15\n"
			"movq 8(%%rsp),%%r14\n"
			"movq 16(%%rsp),%%r13\n"
			"movq 24(%%rsp),%%r12\n"
			"movq 32(%%rsp),%%r11\n"
			"movq 40(%%rsp),%%r10\n"
			"movq 48(%%rsp),%%r9\n"
			"movq 56(%%rsp),%%r8\n"
			"movq 64(%%rsp),%%rsi\n"
			"movq 72(%%rsp),%%rdi\n"
			"movq 80(%%rsp),%%rbp\n"
			"movq 88(%%rsp),%%rdx\n"
			"movq 96(%%rsp),%%rcx\n"
			"movq 104(%%rsp),%%rbx\n"
			"movq 112(%%rsp),%%rax\n"
			"addq $120,%%rsp\n"
			"movw 8(%%rsp),%%ds\n"
			"movw (%%rsp),%%es\n"
			"addq $32, %%rsp\n"
			"iretq"
			: : "g" ((uint64_t) tf) : "memory");
}

/* Switching the thread by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function. */
static void
thread_launch (struct thread *th) {
	uint64_t tf_cur = (uint64_t) &running_thread ()->tf;
	uint64_t tf = (uint64_t) &th->tf;
	ASSERT (intr_get_level () == INTR_OFF);

	/* The main switching logic.
	 * We first restore the whole execution context into the intr_frame
	 * and then switching to the next thread by calling do_iret.
	 * Note that, we SHOULD NOT use any stack from here
	 * until switching is done. */
	__asm __volatile (
			/* Store registers that will be used. */
			"push %%rax\n"
			"push %%rbx\n"
			"push %%rcx\n"
			/* Fetch input once */
			"movq %0, %%rax\n"
			"movq %1, %%rcx\n"
			"movq %%r15, 0(%%rax)\n"
			"movq %%r14, 8(%%rax)\n"
			"movq %%r13, 16(%%rax)\n"
			"movq %%r12, 24(%%rax)\n"
			"movq %%r11, 32(%%rax)\n"
			"movq %%r10, 40(%%rax)\n"
			"movq %%r9, 48(%%rax)\n"
			"movq %%r8, 56(%%rax)\n"
			"movq %%rsi, 64(%%rax)\n"
			"movq %%rdi, 72(%%rax)\n"
			"movq %%rbp, 80(%%rax)\n"
			"movq %%rdx, 88(%%rax)\n"
			"pop %%rbx\n"              // Saved rcx
			"movq %%rbx, 96(%%rax)\n"
			"pop %%rbx\n"              // Saved rbx
			"movq %%rbx, 104(%%rax)\n"
			"pop %%rbx\n"              // Saved rax
			"movq %%rbx, 112(%%rax)\n"
			"addq $120, %%rax\n"
			"movw %%es, (%%rax)\n"
			"movw %%ds, 8(%%rax)\n"
			"addq $32, %%rax\n"
			"call __next\n"         // read the current rip.
			"__next:\n"
			"pop %%rbx\n"
			"addq $(out_iret -  __next), %%rbx\n"
			"movq %%rbx, 0(%%rax)\n" // rip
			"movw %%cs, 8(%%rax)\n"  // cs
			"pushfq\n"
			"popq %%rbx\n"
			"mov %%rbx, 16(%%rax)\n" // eflags
			"mov %%rsp, 24(%%rax)\n" // rsp
			"movw %%ss, 32(%%rax)\n"
			"mov %%rcx, %%rdi\n"
			"call do_iret\n"
			"out_iret:\n"
			: : "g"(tf_cur), "g" (tf) : "memory"
			);
}

/* Schedules a new process. At entry, interrupts must be off.
 * This function modify current thread's status to status and then
 * finds another thread to run and switches to it.
 * It's not safe to call printf() in the schedule(). */
static void
do_schedule(int status) {
	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT (thread_current()->status == THREAD_RUNNING);
	while (!list_empty (&destruction_req)) {
		struct thread *victim =
			list_entry (list_pop_front (&destruction_req), struct thread, elem);
		palloc_free_page(victim);
	}
	thread_current ()->status = status;
	schedule ();
}

static void
schedule (void) {
	struct thread *curr = running_thread ();
	struct thread *next = next_thread_to_run ();

	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT (curr->status != THREAD_RUNNING);
	ASSERT (is_thread (next));
	/* Mark us as running. */
	next->status = THREAD_RUNNING;

	/* Start new time slice. */
	thread_ticks = 0;

#ifdef USERPROG
	/* Activate the new address space. */
	process_activate (next);
#endif

	if (curr != next) {
		/* If the thread we switched from is dying, destroy its struct
		   thread. This must happen late so that thread_exit() doesn't
		   pull out the rug under itself.
		   We just queuing the page free reqeust here because the page is
		   currently used bye the stack.
		   The real destruction logic will be called at the beginning of the
		   schedule(). */
		if (curr && curr->status == THREAD_DYING && curr != initial_thread) {
			ASSERT (curr != next);
			list_push_back (&destruction_req, &curr->elem);
		}

		/* Before switching the thread, we first save the information
		 * of current running. */
		thread_launch (next);
	}
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) {
	static tid_t next_tid = 1;
	tid_t tid;

	lock_acquire (&tid_lock);
	tid = next_tid++;
	lock_release (&tid_lock);

	return tid;
}


void mlfqs_priority(struct thread *t)
{
	if (t == idle_thread) return;
	t->priority = fp_to_int (add_mixed (div_mixed (t->recent_cpu, -4), PRI_MAX - t->nice * 2));
	// PRI_MAX –(recent_cpu/ 4) – (nice * 2)
}

void mlfqs_recent_cpu(struct thread *t)
{
	if (t == idle_thread) return;
	t->recent_cpu = add_mixed (mult_fp (div_fp (mult_mixed (load_avg, 2), add_mixed (mult_mixed (load_avg, 2), 1)), t->recent_cpu), t->nice);
	// (2 * load_avg) / (2 * load_avg +1) * t->recent_cpu + t->nice;
}

void mlfqs_load_avg(void)
{
	int cnt = 0;
	if (thread_current() != idle_thread) {
		cnt++;
	}
	load_avg =  add_fp (mult_fp (div_fp (int_to_fp (59), int_to_fp (60)), load_avg), 
                     mult_mixed (div_fp (int_to_fp (1), int_to_fp (60)), list_size(&ready_list) + cnt));
	// (59/60) * load_avg + (1/60) * (list_size(&ready_list) + cnt);
	if (load_avg < 0) {
		load_avg = LOAD_AVG_DEFAULT; 
	}
// add_fp (mult_fp (div_fp (int_to_fp (59), int_to_fp (60)), load_avg), 
// mult_mixed (div_fp (int_to_fp (1), int_to_fp (60)), ready_threads))
}

void mlfqs_increment(void)
{
	if (thread_current() == idle_thread) return;
	thread_current()->recent_cpu += 1;
}

void mlfqs_recalc(void)
{
	struct list_elem *e;
	for(e = list_begin(&ready_list); e != list_end(&ready_list); e = list_next(e)){
		mlfqs_recent_cpu(list_entry(e, struct thread, elem));
		mlfqs_priority(list_entry(e, struct thread, elem));
	}
}