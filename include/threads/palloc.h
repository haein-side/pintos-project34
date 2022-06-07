#ifndef THREADS_PALLOC_H
#define THREADS_PALLOC_H

#include <stdint.h>
#include <stddef.h>

/* How to allocate pages. */
enum palloc_flags {
	PAL_ASSERT = 001,           /* 실행 권한 */
	PAL_ZERO = 002,             /* 쓰기 권한 */
	PAL_USER = 004              /* 읽기 권한 */
};

/* Maximum number of pages to put in user pool. */
extern size_t user_page_limit;

uint64_t palloc_init (void);
void *palloc_get_page (enum palloc_flags);
void *palloc_get_multiple (enum palloc_flags, size_t page_cnt);
void palloc_free_page (void *);
void palloc_free_multiple (void *, size_t page_cnt);

#endif /* threads/palloc.h */
