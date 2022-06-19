/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "lib/kernel/hash.h"
#include "threads/malloc.h"
#include "userprog/process.h"
#include "lib/kernel/list.h" /*** haein ***/
#include <string.h>

static struct list frame_table;			/*** GrilledSalmon ***/

struct page *page_lookup (struct hash *h, const void *va); /*** haein ***/

/*** Dongdongbro ***/
unsigned page_hash (const struct hash_elem *h_elem, void *aux UNUSED);
bool page_less (const struct hash_elem *h_elem1, const struct hash_elem *h_elem2, void *aux UNUSED);

/*** GrilledSalmon ***/
void spt_hash_destructor (struct hash_elem *e, void *aux); 	
static void copy_parent_file (struct file *parent_file, int parent_remain_cnt, tid_t child_tid, bool is_uninit, void *aux);

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	list_init(&frame_table);
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/*** GrilledSalmon ***/
/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;
	upage = pg_round_down(upage);

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *page = malloc(sizeof(struct page));
		bool (*initializer)(struct page *, enum vm_type, void *);

		switch (VM_TYPE(type))
		{
		case VM_ANON:
			initializer = anon_initializer;
			break;

		case VM_FILE:
			initializer = file_backed_initializer;
			break;

		default:
			initializer = NULL;
			break;
		}

		uninit_new(page, upage, init, type, aux, initializer);
		page->writable = writable;

		/* TODO: Insert the page into the spt. */
		if (!spt_insert_page(spt, page)) {
			free(page);
			goto err;
		}

		return true;
	}
err:
	return false;
}

/*** haein ***/
/* Find VA from spt and return page. On error, return NULL. */
/* VA와 상응하는 struct page를 supplemental page table에서 찾아준다. 실패 시, NULL을 리턴한다. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	return page_lookup(&spt->h, va);
}

/*** GrilledSalmon ***/
/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt, struct page *page) {
	int succ = false;
	/* TODO: Fill this function. */

	if (hash_insert(&spt->h, &page->hash_elem) == NULL) {
		succ = true;
	}

	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	hash_delete(&spt->h, &page->hash_elem);
	vm_dealloc_page (page);
	return true;
}

/*** GrilledSalmon ***/
/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */
	struct list_elem *elem;

	ASSERT(!list_empty(&frame_table));

	for (elem=list_begin(&frame_table); elem!=list_end(&frame_table); elem=list_next(&elem))
	{
		victim = list_entry(elem, struct frame, frame_elem);
		if (pml4_is_accessed(victim->pml4, victim->page->va)) {
			pml4_set_accessed(victim->pml4, victim->page->va, false);
		} else {
			break;
		}
	}
	/* 만약 리스트를 다 돌았는데 모두 accessed 상태면 자동으로 마지막 frame 리턴*/
	return victim;
}

/*** haein ***/
/* Evict one page and return the corresponding frame.
 * Return NULL on error.
 * 하나의 페이지를 evict 하고 상응하는 frame을 리턴
 * 에러가 났을 때는 NULL을 리턴
 * 프레임 테이블에서 없애고 swap table에서도 없앰 (swap out)
 */
static struct frame *
vm_evict_frame (void) {
	struct frame *victim = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	if (!victim) {
		return NULL;
	}

	if (!list_remove(&victim->frame_elem)) { // frame table에서 삭제
		return NULL;
	}

	if (!swap_out(victim->page)) { // swap_out 호출
		return NULL;
	} 
	
	pml4_clear_page(victim->pml4, victim->page->va); // pml4에서 삭제

	return victim;
}


/*** GrilledSalmon & haein ***/
/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
/* palloc()과 프레임을 얻어옵니다. 만약 이용가능한 페이지가 없으면, 페이지를 지우고 이를 리턴합니다.
항상 유효한 주소값을 반환해야합니다. 만약 유저풀 메모리가 가득 찼다면 이 함수는 사용가능한 메모리 공간을 얻기 위해
기존에 있던 프레임을 지워야합니다. */
static struct frame *
vm_get_frame (void) {
	struct frame *frame = calloc(1, sizeof (struct frame));
	ASSERT (frame != NULL);

	/* TODO: Fill this function. */
	uint64_t *kva = palloc_get_page(PAL_USER);

	if (kva == NULL) {		
		struct frame *evicted_frame = vm_evict_frame(); //  evict 시킨 페이지에 상응하는 frame 리턴
		ASSERT (evicted_frame != NULL);
		kva = evicted_frame->kva; // evict 시킨 페이지의 kva에 공간 할당 받을 수 있음
	}

	frame->kva = kva;
	ASSERT (frame->page == NULL);

	list_push_back(&frame_table, &frame->frame_elem); // frame_table에 추가
	frame->pml4 = thread_current()->pml4; // frame의 pml4에 현재 스레드의 pml4 초기화

	return frame;
}

/*** Dongdongbro ***/
/* Growing the stack. */
static void
vm_stack_growth (void *addr) {
	addr = pg_round_down(addr);

	if (addr < USER_STACK_LIMIT){
		goto err;
	}
	if (vm_alloc_page(VM_STACK, addr, true) && vm_claim_page(addr)) {
		memset(addr, 0, PGSIZE);
		return;
	}

err :
	PANIC("Stack growth failed!");
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}


/*** GrilledSalmon ***/
/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct thread *t = thread_current();
	struct page *page = spt_find_page(&t->spt, addr);
	void *rsp;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	if (is_user_vaddr(f->rsp)) {
		rsp = f->rsp;
		t->rsp = rsp;
	} else {
		rsp = t->rsp;
	}

	if(page == NULL){
		if ((addr == rsp - 8 || (rsp<=addr && addr<USER_STACK) && rsp != NULL)) { // stack growth
			vm_stack_growth(addr);
			return true;
		}
		return false;
	}
	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/*** Dongdongbro ***/
/* Claim the page that allocate on VA. */
// va를 할당하기 위해 페이지를 선언한다.
bool
vm_claim_page (void *va) {
	struct page *page = spt_find_page(&thread_current()->spt, va);
	/* TODO: Fill this function */

	if (page != NULL) {
		return vm_do_claim_page (page);
	}
	return false;
}

/*** haein ***/
/* Claim the PAGE and set up the mmu. */
/* 페이지 값을 넘겨 받고, 그 페이지와 get frame에서 물리 메모리 공간을 페이지와 연결 시켜준다. */
static bool
vm_do_claim_page (struct page *page) { // 이미 만들어진 page => 매핑
	struct frame *frame = vm_get_frame ();
	struct thread *t = thread_current();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	if (pml4_get_page (t->pml4, page->va) == NULL && pml4_set_page(t->pml4, page->va, frame->kva, page->writable)) { /*** 고민 필요!!! - true? ***/
		return swap_in (page, frame->kva); // page fault가 일어났을 때 swap in
	} else { // 만약 page fault에서 호출했는데 실패했으면 바로 프로세스 종료
		// 나중에 vm_dealloc_page 써야 할듯? /*** GriiledSalmon ***/
		return false;
	}
}

/*** Dongdongbro ***/
/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	/*** 고민 필요!!!(init이 두번 불릴 경우를 고려) ***/
	/*
	if(spt->h != NULL){
		free(spt->h);
	}
	*/


	if (!hash_init(&spt->h, page_hash, page_less, NULL)){
		PANIC("There are no memory in Kernel pool(malloc fail)");
	}
}

/*** GrilledSalmon ***/
/* Checking the condition, copy file struct from parent to child. */
static void copy_parent_file (struct file *parent_file, int parent_remain_cnt, tid_t child_tid, bool is_uninit, void *aux) {
	if (is_uninit) {
		struct lazy_info *lazy_info = aux;
		if (parent_file->copying_child != child_tid) { 	/* First file copy condition */
			lazy_info->file = file_reopen(parent_file);
			lazy_info->remain_cnt = malloc(sizeof(int));

			memcpy(lazy_info->file, parent_file, sizeof(struct file));
			*lazy_info->remain_cnt = parent_remain_cnt;

			parent_file->copying_child = child_tid;
			parent_file->child_file = lazy_info->file;
			parent_file->child_remain_cnt = lazy_info->remain_cnt;
		} else {
			lazy_info->file = parent_file->child_file;
			lazy_info->remain_cnt = parent_file->child_remain_cnt;
		}
	} else {
		struct file_page *file_page = aux;
		if (parent_file->copying_child != child_tid) { 	/* First file copy condition */
			file_page->file = file_reopen(parent_file);
			file_page->remain_cnt = malloc(sizeof(int));

			memcpy(file_page->file, parent_file, sizeof(struct file));
			*file_page->remain_cnt = parent_remain_cnt;

			parent_file->copying_child = child_tid;
			parent_file->child_file = file_page->file;
			parent_file->child_remain_cnt = file_page->remain_cnt;
		} else {
			file_page->file = parent_file->child_file;
			file_page->remain_cnt = parent_file->child_remain_cnt;
		}
	}
}

/*** Dongdongbro & GrilledSalmon ***/
/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst, struct supplemental_page_table *src) {
	tid_t tid = thread_current()->tid;
	struct hash_iterator i;
	hash_first (&i, &src->h);
	while (hash_next(&i)){
		struct page *src_page = hash_entry(hash_cur(&i), struct page, hash_elem);
		enum vm_type type = VM_TYPE (src_page->operations->type);
		struct lazy_info *dst_lazy_info;
		struct page *dst_page;

		switch (type)
		{
		case VM_UNINIT :
		{
			struct lazy_info *src_lazy_info = src_page->uninit.aux;
			dst_lazy_info = malloc(sizeof(struct lazy_info));
			memcpy(dst_lazy_info, src_lazy_info, sizeof(struct lazy_info));
			if (type == VM_FILE) {
				copy_parent_file(src_lazy_info->file, *src_lazy_info->remain_cnt, tid, true, dst_lazy_info);
			}

			if(!vm_alloc_page_with_initializer(src_page->uninit.type, src_page->va, src_page->writable, src_page->uninit.init, dst_lazy_info)){
				return false;
			};
		}
			break;

		case VM_ANON :
		{	
			if(!vm_alloc_page(type | src_page->anon.aux_type, src_page->va, src_page->writable) || !vm_claim_page(src_page->va)){
				return false;
			};
			dst_page = spt_find_page(dst, src_page->va);
			memcpy(dst_page->frame->kva, src_page->frame->kva, PGSIZE);
		}
			break;

		case VM_FILE :
		{
			if(!vm_alloc_page_with_initializer(type, src_page->va, src_page->writable, NULL, &src_page->file) || !vm_claim_page(src_page->va)){
				return false;
			};
			dst_page = spt_find_page(dst, src_page->va);
			memcpy(dst_page->frame->kva, src_page->frame->kva, PGSIZE);
			/*** 부모의 dirty bit를 복사해줘야 할까? 고민 필요!!!!! ***/
			copy_parent_file(src_page->file.file, *src_page->file.remain_cnt, tid, false, &dst_page->file);
			break;
		}

		default :
			PANIC("Cached type");
			break;
		}
	}
	return true;
}

/*** GrilledSalmon ***/
void spt_hash_destructor (struct hash_elem *e, void *aux) {
	struct page *page = hash_entry(e, struct page, hash_elem);
	/* filebacked할 때 수정 필요!!!(writeback) */

	return vm_dealloc_page(page);
}

/*** GrilledSalmon ***/
/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */

	hash_destroy(&spt->h, spt_hash_destructor);
}


/*** haein ***/
/* Returns the page containing the given virtual address, or a null pointer if no such page exists. */
struct page *
page_lookup (struct hash *h, const void *va) {
  struct page p;
  struct hash_elem *e;

  p.va = pg_round_down(va); // offset을 0으로 만들고 페이지 주소를 받아옴

  e = hash_find (h, &p.hash_elem);
  return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}

/*** Dongdongbro ***/
/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *h_elem, void *aux UNUSED) {
  const struct page *p = hash_entry (h_elem, struct page, hash_elem);
  return hash_bytes (&p->va, sizeof p->va);
}

/*** Dongdongbro ***/
/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *h_elem1,
           const struct hash_elem *h_elem2, void *aux UNUSED) {
  const struct page *a = hash_entry (h_elem1, struct page, hash_elem);
  const struct page *b = hash_entry (h_elem2, struct page, hash_elem);

  return a->va < b->va;
}
