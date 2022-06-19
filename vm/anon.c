/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/*** haein ***/
/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = NULL;
}

/*** haein ***/
/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva UNUSED) {
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;

	anon_page->aux_type = VM_AUXTYPE(type); // anon_page의 aux_type은 anon_type이므로 1을 빼줌 
	anon_page->slot_number = -1;            // 아직 swap out된 적 없으므로 slot number -1로 줌
	
	/*** 고민 필요!!! (bool형 리턴값 false인 경우?) ***/
	return true;
}

/*** haein ***/
/* Swap in the page by read contents from the swap disk. */
/* 프로세스가 swap out된 페이지에 접근하려고 하는 경우,
 * swap disk에서 contents를 읽어 page를 swap in	함
 */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
	
	int slot_number = anon_page->slot_number;
	int sec_no = anon_page->slot_number * PG_PER_SEC;
	void *kva = page->frame->kva;

	for (int i=0; i<PG_PER_SEC; i++) {
		disk_read(swap_disk, sec_no, kva);
		kva += DISK_SECTOR_SIZE;
		sec_no++;
	}
	
	bitmap_set(swap_table, slot_number, false);
	anon_page->slot_number = -1;

	return true;

}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	/*** 고민 필요!!! - swap out 시 달라질 수 있음 ***/
	free(page->frame);
}
