#ifndef VM_UNINIT_H
#define VM_UNINIT_H
#include <stdbool.h>
#include "filesys/off_t.h"

struct page;
struct supplemental_page_table;
enum vm_type;

typedef bool vm_initializer (struct page *, void *aux);

enum uninit_aux_type {
	UNINIT_AUX_LOAD,
	UNINIT_AUX_FILE,
	UNINIT_AUX_ANON
};

struct uninit_aux_load {
    struct file *elf_file;
    off_t page_pos;
    size_t page_read_bytes;
    size_t page_zero_bytes;
};

struct uninit_aux_file {
	struct file *file;
	off_t offset;
	size_t pg_read_bytes;
	size_t pg_zero_bytes;
	bool writable;
};

struct uninit_aux_anon {

};

struct 							uninit_aux {
	enum uninit_aux_type		type;
	union {
		struct uninit_aux_load	aux_load;
		struct uninit_aux_file	aux_file;
		struct uninit_aux_anon	aux_anon;
	};
};

/* Uninitlialized page. The type for implementing the
 * "Lazy loading". */
struct uninit_page {
	/* Initiate the contets of the page */
	vm_initializer *init;
	enum vm_type type;
	void *aux;
	/* Initiate the struct page and maps the pa to the va */
	bool (*page_initializer) (struct page *, enum vm_type, void *kva);
};

void uninit_new (struct page *page, void *va, vm_initializer *init,
		enum vm_type type, void *aux,
		bool (*initializer)(struct page *, enum vm_type, void *kva));

bool uninit_copy(struct supplemental_page_table *dst, struct page *src_page);

#endif
