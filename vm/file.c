/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/mmu.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);
static bool lazy_load_file (struct page *page, void *aux);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/*** Dongdongbro ***/
/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct lazy_info *lazy_info = page->uninit.aux;
	struct file_page *file_page = &page->file;
	file_page->file = lazy_info->file;
	file_page->ofs = lazy_info->ofs;
	file_page->read_bytes = lazy_info->read_bytes;
	file_page->remain_cnt = lazy_info->remain_cnt;

	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/*** Dongdongbro ***/
/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	uint64_t current_pml4 = thread_current()->pml4;

	if (*page->file.remain_cnt == 1){
		file_close(page->file.file);
		free(page->file.remain_cnt);
	} else {
		(*page->file.remain_cnt)--;
	}

	if(pml4_get_page(current_pml4, page->va)){
		if(pml4_is_dirty(current_pml4, page->va)){
			file_write_at(page->file.file, page->va, page->file.read_bytes, page->file.ofs);
			pml4_set_dirty(current_pml4, page->va, false);
		}
		palloc_free_page(page->frame->kva);
		pml4_clear_page(current_pml4, page->va);
	}
	free(page->frame);
}

/*** Dongdongbro ***/
/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
			struct file *reopen_file = file_reopen(file);
			if (reopen_file == NULL){
				return NULL;
			}
			off_t now = offset;
			int *remain_cnt = malloc(sizeof(int));
			uint64_t temp_addr = addr;
			uint64_t initial_addr = addr;
			size_t temp_length = length;

			*remain_cnt = 0;

			while (temp_length > 0){
				(*remain_cnt)++;
				if (spt_find_page(&thread_current()->spt, temp_addr) != NULL){
					return NULL;
				}
				temp_addr += PGSIZE;
				temp_length -= PGSIZE;
			}
			
			size_t f_length = file_length(reopen_file);
			
			while (length > 0){
				size_t page_read_bytes = f_length < PGSIZE ? f_length : PGSIZE;
				size_t page_zero_bytes = PGSIZE - page_read_bytes;

				struct lazy_info *lazy_info = malloc(sizeof(struct lazy_info));
				lazy_info->file = reopen_file;
				lazy_info->ofs = now;
				lazy_info->read_bytes = page_read_bytes;
				lazy_info->remain_cnt = remain_cnt;

				if(!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_file, lazy_info)){
					free(lazy_info);
					return NULL;
				}
				
				f_length = f_length > PGSIZE ? f_length - PGSIZE:0;
				length -= PGSIZE;
				addr += PGSIZE;
				now += PGSIZE;
			}

		return initial_addr;
}

/*** haein ***/
/* Do the munmap */
void
do_munmap (void *addr) {
	struct supplemental_page_table *spt = &thread_current() -> spt;
	int remain_cnt = *spt_find_page(spt, addr)->file.remain_cnt;

	while (remain_cnt) {
		struct page *munmap_page = spt_find_page(spt, addr);

		if (munmap_page == NULL) {
			goto err;
		}
		
		spt_remove_page(spt, munmap_page); // vm_dealloc_page (page) -> destroy(page) free(page)
		addr += PGSIZE;
		remain_cnt--;
	}

	err:
		return;
}


/*** Dongdongbro ***/
static bool
lazy_load_file (struct page *page, void *aux){
	struct lazy_info *lazy_info = aux;
	struct file *file = lazy_info->file;

	file_seek(file, lazy_info->ofs);

	if(file_read(file, page->frame->kva, lazy_info->read_bytes) != (int) lazy_info->read_bytes){
		return false;
	}

	memset(page->frame->kva + lazy_info->read_bytes, 0, PGSIZE - lazy_info->read_bytes);
	free(lazy_info);

	return true;
}
