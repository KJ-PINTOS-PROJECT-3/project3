/* uninit.c: Implementation of uninitialized page.
 *
 * All of the pages are born as uninit page. When the first page fault occurs,
 * the handler chain calls uninit_initialize (page->operations.swap_in).
 * The uninit_initialize function transmutes the page into the specific page
 * object (anon, file, page_cache), by initializing the page object,and calls
 * initialization callback that passed from vm_alloc_page_with_initializer
 * function.
 * */

#include "vm/vm.h"
#include <stdbool.h>
#include "vm/uninit.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/syscall.h"

static bool uninit_initialize (struct page *page, void *kva);
static void uninit_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations uninit_ops = {
	.swap_in = uninit_initialize,
	.swap_out = NULL,
	.destroy = uninit_destroy,
	.type = VM_UNINIT,
};

/* DO NOT MODIFY this function */
void
uninit_new (struct page *page, void *va, vm_initializer *init,
		enum vm_type type, void *aux,
		bool (*initializer)(struct page *, enum vm_type, void *)) {
	ASSERT (page != NULL);

	*page = (struct page) {
		.operations = &uninit_ops,
		.va = va,
		.frame = NULL, /* no frame for now */
		.uninit = (struct uninit_page) {
			.init = init,
			.type = type,
			.aux = aux,
			.page_initializer = initializer,
		}
	};
}

/* Initalize the page on first fault */
static bool
uninit_initialize (struct page *page, void *kva) {
	struct uninit_page *uninit = &page->uninit;

	/* Fetch first, page_initialize may overwrite the values */
	vm_initializer *init = uninit->init;
	void *aux = uninit->aux;

	return uninit->page_initializer (page, uninit->type, kva) &&
		(init ? init (page, aux) : true);
}

/* Free the resources hold by uninit_page. Although most of pages are transmuted
 * to other page objects, it is possible to have uninit pages when the process
 * exit, which are never referenced during the execution.
 * PAGE will be freed by the caller. */
static void
uninit_destroy (struct page *page) {
	struct uninit_page *uninit = &page->uninit;
	struct uninit_aux *aux_uninit = page->uninit.aux;
	if (aux_uninit){		
		if(aux_uninit->type == UNINIT_AUX_FILE){
			file_close(aux_uninit->aux_file.file);
		} 
		free (page->uninit.aux);
		page->uninit.aux = NULL;
	}
}

bool 
uninit_aux_load_copy(struct supplemental_page_table *dst, struct page *src_page) {
	struct file				*current_file_copy = NULL;
	struct uninit_aux		*aux = NULL;


	current_file_copy = thread_current()->current_file;

	aux = (struct uninit_aux *)calloc(1, sizeof(struct uninit_aux));
	if (!aux) return false;

	memcpy(aux, src_page->uninit.aux, sizeof(struct uninit_aux));
	aux->aux_load.elf_file = current_file_copy;

	if (!vm_alloc_page_with_initializer(
		src_page->uninit.type, src_page->va, src_page->writable,
		src_page->uninit.init, aux
	)) return false;

	return true;
}

bool 
uninit_aux_file_copy(struct supplemental_page_table *dst, struct page *src_page) {
	return true;
}

bool 
uninit_aux_anon_copy(struct supplemental_page_table *dst, struct page *src_page) {
	struct page	*dst_page;

	if (!vm_alloc_page_with_initializer(src_page->uninit.type, src_page->va, src_page->writable, src_page->uninit.init, src_page->uninit.aux))
		return false;

	return true;
}

bool uninit_copy(struct supplemental_page_table *dst, struct page *src_page) {
	enum uninit_aux_type	aux_type;

	if (!dst || !src_page) return false;  

	aux_type = ((struct uninit_aux *)(src_page->uninit.aux))->type;

	switch (aux_type)
	{
		case UNINIT_AUX_LOAD:
			if (false == uninit_aux_load_copy(dst, src_page)) return false;
			break ;
		case UNINIT_AUX_FILE:
			if (false == uninit_aux_file_copy(dst, src_page)) return false;
			break ;
		case UNINIT_AUX_ANON:
			if (false == uninit_aux_anon_copy(dst, src_page)) return false;
			break ;	
	}

	return true;
}
