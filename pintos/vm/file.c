/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include <round.h>
#include "threads/vaddr.h"
#include "userprog/syscall.h"
#include <string.h>
#include "threads/mmu.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);


/* Helper Function */
static bool valid_vma_range(uintptr_t vaild_addr_ptr, size_t valid_length);
static bool file_load(struct page* page, void* aux);
static void write_back(struct page *page);


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

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;
	struct file_page *file_page = &page->file;
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

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page = &page->file;

	if(pml4_is_dirty(thread_current()->pml4, page->va))
		write_back(page);

	if(file_page->mapped_file == NULL) return;
	file_close(file_page->mapped_file);
}

/* When evicted from physical memory */
static void write_back(struct page *page){
	struct file *target_file = page->file.mapped_file;
	size_t read_bytes = page->file.read_bytes;
	off_t pos = page->file.pos;

	file_write_at(target_file, page->frame->kva, read_bytes, pos);
}


static void *get_group_number(struct page *page){
	if(page->operations->type == VM_FILE){
		return page->file.mmap_base;
	} else if (page->operations->type == VM_UNINIT){
		struct uninit_aux *aux = page->uninit.aux;
		return aux->aux_file.mmap_base;
	}
	return NULL;
}

/* 요청한 vma가 연속된 free인지 확인(SPT CHECK) */
static bool valid_vma_range(uintptr_t vaild_addr_ptr, size_t valid_length){
	struct thread *cur = thread_current();
	while(valid_length > 0){
		if(spt_find_page(&cur->spt, vaild_addr_ptr)) return NULL;
		size_t move_bytes = (PGSIZE < valid_length) ? PGSIZE : valid_length; 
		vaild_addr_ptr += move_bytes;
		valid_length -= move_bytes;
	}
	return true;
}

static bool file_load(struct page* page, void* aux){
	void *kpage = page->frame->kva;
	struct uninit_aux_file *aux_file = &(((struct uninit_aux *) aux)->aux_file);
	struct file *mapped_file = aux_file->file;
	off_t pos = aux_file->page_pos;
	size_t read_bytes = aux_file->page_read_bytes;
	size_t zero_bytes = aux_file->page_zero_bytes;
	//free(aux);

	struct file_page *file_page = &page->file;
	*file_page = (struct file_page){
		.mapped_file = mapped_file,
		.mmap_base = aux_file->mmap_base,
		.pos = pos,
		.read_bytes = read_bytes,
		.zero_bytes = zero_bytes,
	};

	lock_acquire(&file_lock);
	file_read_at(mapped_file, kpage, read_bytes, pos);
	lock_release(&file_lock);

	memset(kpage + read_bytes, 0, zero_bytes);
	return true;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	uint8_t* upage = (uint8_t *)addr;
	off_t ofs = offset;
	struct uninit_aux *aux_file = NULL;
	void *aux = NULL;

	if(!valid_vma_range(addr, length)) return NULL;

	size_t read_bytes = length;
	size_t page_cnt = DIV_ROUND_UP(length, PGSIZE);

	for(size_t i = 0; i < page_cnt; i++){
		size_t page_read_bytes = (read_bytes < PGSIZE) ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		struct file *d_file = file_reopen(file);
		if(!d_file) return NULL;


		/* aux 생성 */
		aux_file = (struct uninit_aux *)malloc(sizeof(struct uninit_aux));
		*aux_file = (struct uninit_aux) {
			.type = UNINIT_AUX_FILE,
			.aux_file = (struct uninit_aux_file) {
				.file = d_file,
				.mmap_base = addr, 
				.page_pos = ofs,
				.page_read_bytes = page_read_bytes,
				.page_zero_bytes = page_zero_bytes,
			}
		};

		aux = aux_file;
		if(!vm_alloc_page_with_initializer(VM_FILE, upage, writable, file_load, aux)){
			free(aux_file);
			/* TODO : 중간에 실패시 지금까지 해온거 FREE 해야 함  */
			return NULL;
		}

		/* Advance. */
		read_bytes -= page_read_bytes;
		upage += PGSIZE;
		ofs += PGSIZE;
	}

	/* 포인터 할당 */
	return addr;

}

/* Do the munmap */
void
do_munmap (void *addr) {
	struct supplemental_page_table *spt = &thread_current()->spt;
	struct page *pivot_page = spt_find_page(spt, addr);
	struct page *page = NULL;
	void *cur_addr = addr;
	if(pivot_page == NULL || page_get_type(pivot_page) != VM_FILE) return;
	void *group_number = get_group_number(pivot_page);

	/* spt 찾고 file_backed인지 확인 */
	while(true){
		page = spt_find_page(spt, addr);
		if(page == NULL || page_get_type(page) != VM_FILE) break;

		if(get_group_number(page) != group_number) break;
		spt_remove_page(spt, page);
		addr += PGSIZE;
	}
	return;
}
