#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "threads/init.h"
#include "filesys/filesys.h"
#include "threads/synch.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
void check_address (void *addr);
void halt(void);
void exit (int status);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);

struct lock *lock;
/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	check_address(f->rsp);
	struct thread *p1 = thread_current();
	switch (f->R.rax) {
		case SYS_HALT :
			halt();
			break;                   /* Halt the operating system. */
		case SYS_EXIT :
			exit(f->R.rdi);
			break;                   /* Terminate this process. */
		case SYS_FORK :
			break;                   /* Clone current process. */
		case SYS_EXEC :
			break;                   /* Switch current process. */
		case SYS_WAIT :
			break;                   /* Wait for a child process to die. */
		case SYS_CREATE :
			printf ("system call success\n");
			f->R.rax = create(f->R.rdi, f->R.rsi);
			break;                 /* Create a file. */
		case SYS_REMOVE :
			f->R.rax = remove(f->R.rdi);
			break;                 /* Delete a file. */
		case SYS_OPEN :
			break;                   /* Open a file. */
		case SYS_FILESIZE :
			break;               /* Obtain a file's size. */
		case SYS_READ :
			break;                   /* Read from a file. */
		case SYS_WRITE :
			break;                  /* Write to a file. */
		case SYS_SEEK :
			break;                   /* Change position in a file. */
		case SYS_TELL :
			break;                   /* Report current position in a file. */
		case SYS_CLOSE :
			break;                  /* Close a file. */
		default :
			thread_exit ();			// 영천이의 생각노트 추가예정
	}
	printf ("system call!\n");
}

void check_address (void *addr){
	struct thread *p1 = thread_current();
	// 커널 영역을 참조하고, 주소가 유효하지 않고, 페이지퐅트가 뜨면 종료한다.
	if(!is_user_vaddr(addr) || addr==NULL || pml4_get_page(p1->pml4, addr)==NULL){
		exit(-1);
	}
}

void halt (void){
	power_off();
}

void exit (int status){
	struct thread *p1 = thread_current();
	printf("%s : exit(%d)\n", p1->name, status);
	thread_exit();
}

bool create (const char *file, unsigned initial_size){
	check_address(file);
	return filesys_create(file, initial_size);
}

bool remove (const char *file){
	check_address(file);
	return filesys_remove(file);
}

// int write (int fd, void *buffer unsigned size){
// 	lock_acquire(lock);
// }