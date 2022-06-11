/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"

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

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		/* TODO: Insert the page into the spt. */
	}
err:
	return false;
}

/*** haein ***/
/* Find VA from spt and return page. On error, return NULL. */
/* VA와 상응하는 struct page를 supplemental page table에서 찾아준다. 실패 시, NULL을 리턴한다. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	// struct page *page = NULL; // ?
	struct hash *h = spt->h;

	if (page_lookup(h, va) != NULL){ // va와 상응하는 struct page를 spt에서 찾아줘야 함
		struct page *page = hash_entry(va, struct page, hash_elem);
		return page;
	} else {
		return NULL;
	}

}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */

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

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

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
vm_claim_page (void *va UNUSED) {
	struct page *page = spt_find_page(thread_current()->spt, va);	/*** 수정 필요!!! ***/
	/* TODO: Fill this function */
	if(hash_entry(hash_find()))					/*** 수정 필요!!! ***/
		return vm_do_claim_page (page);
	else
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
	if (pml4_get_page (t->pml4, page->va) == NULL && pml4_set_page(t->pml4, page->va, frame->kva, true)) { /*** 고민 필요!!! - true? ***/
		return swap_in (page, frame->kva); // page fault가 일어났을 때 swap in
	} else {
		return false;
	}

}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}
