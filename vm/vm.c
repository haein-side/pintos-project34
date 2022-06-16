/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "lib/kernel/hash.h"
#include "threads/malloc.h"
#include "userprog/process.h"
#include <string.h>

struct page *page_lookup (struct hash *h, const void *va); /*** haein ***/

/*** Dongdongbro ***/
unsigned page_hash (const struct hash_elem *h_elem, void *aux UNUSED);
bool page_less (const struct hash_elem *h_elem1, const struct hash_elem *h_elem2, void *aux UNUSED);

void spt_hash_destructor (struct hash_elem *e, void *aux); 	/*** GrilledSalmon ***/

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
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}


/*** GrilledSalmon ***/
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

	if (kva == NULL) {		/***I'm Your Father***/
		PANIC("Swap out should be implemented!!!\n");
	}

	frame->kva = kva;

	ASSERT (frame->page == NULL);
	return frame;
}

/*** Dongdongbro ***/
/* Growing the stack. */
static void
vm_stack_growth (void *addr) {
	struct thread *t = thread_current();
	addr = pg_round_down(addr);

	if (addr < USER_STACK_LIMIT){
		goto err;
	}
	if (vm_alloc_page(VM_STACK, addr, true) && vm_claim_page(addr)) {
		memset(addr, 0, PGSIZE);
		return;
	}
		
err:
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
		if (addr == rsp - 8 || (rsp <= addr && addr < USER_STACK)) { // stack growth
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

/*** Dongdongbro ***/
/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst, struct supplemental_page_table *src) {
	struct hash_iterator i;
	hash_first (&i, &src->h);
	while (hash_next(&i)){
		struct page *src_page = hash_entry(hash_cur(&i), struct page, hash_elem);
		enum vm_type type = VM_TYPE (src_page->operations->type);
		struct seg_info *dst_seg_info;
		struct page *dst_page;

		switch (type)
		{
		case VM_UNINIT :
			dst_seg_info = malloc(sizeof(struct seg_info));
			memcpy(dst_seg_info, src_page->uninit.aux, sizeof(struct seg_info));
			if(!vm_alloc_page_with_initializer(src_page->uninit.type, src_page->va, src_page->writable, src_page->uninit.init, dst_seg_info)){
				return false;
			};
			break;

		case VM_ANON :
			if(!vm_alloc_page(type | src_page->anon.aux_type, src_page->va, src_page->writable) || !vm_claim_page(src_page->va)){
				return false;
			};
			dst_page = spt_find_page(dst, src_page->va);
			memcpy(dst_page->frame->kva, src_page->frame->kva, PGSIZE);
			break;

		case VM_FILE :		/*** 수정 필요!!! ***/
			if(!vm_alloc_page(type, src_page->va, src_page->writable) || !vm_claim_page(src_page->va)){
				return false;
			};
			dst_page = spt_find_page(dst, src_page->va);
			memcpy(dst_page->frame->kva, src_page->frame->kva, PGSIZE);
			break;

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
