/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "vm/uninit.h"

#include "vm/file.h"

#include <string.h>
#include "threads/mmu.h"
#include "threads/thread.h"
#include "userprog/syscall.h"


static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);
static bool lazy_load_mmap_file (struct page *page, void *aux);

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

	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	if (page->frame == NULL)
		return;

	bool dirty = pml4_is_dirty(thread_current()->pml4, page->va);

	if (dirty) {
		file_page = &page->file;
		// lock_acquire(&file_lock);
		file_write_at(file_page->file, page->frame->kva, file_page->pg_read_bytes, file_page->offset);
		// lock_release(&file_lock);
		pml4_set_dirty(thread_current()->pml4, page->va, false);
	}
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable, struct file *file, off_t offset) {
	void *start = pg_round_down(addr);
	size_t page_count = 0;
	struct page *page = NULL;
	struct uninit_aux *file_aux = NULL;
	if (!addr || !start)
    	return;

	while ((start - addr) < length) {
		if (is_kernel_vaddr(start) || spt_find_page(&thread_current()->spt, start))
			return NULL;
		start += PGSIZE;
		page_count ++;
	}
	start = pg_round_down(addr);

	while (length > 0) {
		file_aux = (struct uninit_aux *)malloc(sizeof(struct uninit_aux));
		if (file_aux == NULL)
			return;

		size_t page_read_bytes = length < PGSIZE ? length : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		*file_aux = (struct uninit_aux) {
			.type = UNINIT_AUX_FILE,
			.aux_file = (struct uninit_aux_file) {
				.file = file,
				.offset = offset,
				.pg_read_bytes = page_read_bytes,
				.pg_zero_bytes = page_zero_bytes,
				.writable = writable,
			}
		};

		file_aux->aux_file.file = file_reopen(file);
		if (file_aux->aux_file.file == NULL) {
			free(file_aux);
			return;
		}
		file_aux->aux_file.offset = offset;
		file_aux->aux_file.pg_read_bytes = page_read_bytes;
		file_aux->aux_file.pg_zero_bytes = page_zero_bytes;

		if (!vm_alloc_page_with_initializer(VM_FILE, start, writable, lazy_load_mmap_file, file_aux)) {
			file_close(file_aux->aux_file.file);
			free(file_aux);
			return;
		}
		
		page = spt_find_page(&thread_current()->spt, start);
		page->page_count = page_count;
		// if (page_count == 0)
		// 	page_count = 1;
		offset += page_read_bytes;
		length -= page_read_bytes;
		start += PGSIZE;
	}
	return addr;
}

static bool lazy_load_mmap_file (struct page *page, void *aux) {
    if (!page || !aux) return false;

    struct uninit_aux_file *af = &(((struct uninit_aux *) aux)->aux_file);
    struct file_page *file_page = &page->file;

    struct file *file = af->file;
    off_t offset = af->offset;
    size_t page_read_bytes = af->pg_read_bytes;
    size_t page_zero_bytes = af->pg_zero_bytes;

    void *kpage;
    off_t bytes_read;

    free(aux);

    kpage = page->frame->kva;
    file_page->file = file;
    file_page->offset = offset;
    file_page->pg_read_bytes = page_read_bytes;
    file_page->pg_zero_bytes = page_zero_bytes;

    // lock_acquire(&file_lock);
    bytes_read = file_read_at(file, kpage, page_read_bytes, offset);
    // lock_release(&file_lock);

    memset(kpage + page_read_bytes, 0, page_zero_bytes);

    return true;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	struct thread *cur = thread_current();
	struct supplemental_page_table *spt = &cur->spt;
	struct page *page = NULL;
	size_t page_count;
	void *start = addr;

	if (!addr)
		return;

	page = spt_find_page(spt, addr);
	if (page == NULL)
		return;

	page_count = page->page_count;
	if (page_count == 0)
		page_count = 1;

	for (size_t i = 0; i < page_count; i++) {
		start = addr + i * PGSIZE;
		page = spt_find_page(spt, start);
		if (page == NULL)
			continue;
		struct file_page *file_page;

		spt_remove_page(spt, page);
		// pml4_clear_page(cur->pml4, page->va);
	}
}