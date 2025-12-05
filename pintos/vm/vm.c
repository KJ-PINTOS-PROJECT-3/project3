/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/vaddr.h"
#include <hash.h>
#include "threads/mmu.h"
#include <string.h>
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

/* Hash table Helpers*/
static uint64_t page_hash(const struct hash_elem *p_, void *aux UNUSED);
static bool page_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);


/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	bool (*type_initializer)(struct page *, enum vm_type, void *);
	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *page = NULL;

	if (spt_find_page (spt, upage) == NULL) {
		page = (struct page *)malloc(sizeof(struct page));
		if(page == NULL) goto err;

		switch (VM_TYPE(type)){
			case VM_ANON:
				type_initializer = anon_initializer;
				break;
			case VM_FILE:
				type_initializer = file_backed_initializer;
				break;
			default:
				goto err;
		}
		uninit_new(page, upage, init, type, aux, type_initializer);
		page->writable = writable;
		if(!(spt_insert_page(spt, page))){
			goto err;
		}
		return true;
	}
err:
	if(page != NULL) free(page);
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	/* TODO: Fill this function. */
	struct page *page = NULL;
	struct page dummy_page; 
	dummy_page.va = pg_round_down(va);

	struct hash_elem *target_elem = hash_find(&spt->hs_table, &dummy_page.hs_elem);
	if(target_elem == NULL){
		return NULL;
	} 

	page = hash_entry(target_elem, struct page, hs_elem);
	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt,
		struct page *page) {
	bool succ = false;
	/* 어떤 상황을 vaildation 해야 할까?*/
	/* 주어진 보충 페이지 테이블에 가상 주소가 존재하는지 확인 -> 즉, 중복 체크*/
	if(page == NULL || spt == NULL) return succ;
	
	struct hash_elem *old = hash_insert(&spt->hs_table, &page->hs_elem);
	if(old == NULL)
		succ = true;

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

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
/**
 * 사용자 풀에서 새 물리적 페이지를 가져오는 함수
 * 
 */
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	void *user_new_page = palloc_get_page(PAL_USER);
	if(user_new_page == NULL){
		PANIC("TODO Implement Frame Table");
	}
	frame = (struct frame *)malloc(sizeof(struct frame));
	if(frame == NULL){
		palloc_free_page(user_new_page);
		return NULL;
	}
	frame->kva = user_new_page;
	frame->page = NULL;

	ASSERT (frame != NULL);
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
vm_try_handle_fault (struct intr_frame *f, void *addr, bool user, 
	bool write, bool not_present) {
	struct supplemental_page_table *spt = &thread_current()->spt;
	struct page *page = NULL;

	if(is_kernel_vaddr(addr) || addr == NULL) return false;

	page = spt_find_page(spt, addr);
	if(page == NULL) return false;

	if(write && !(page->writable)){
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

static void
vm_dealloc_frame(struct frame *frame){
	palloc_free_page(frame -> kva);
	//list_remove(&frame->elem); // TODO: 프레임 리스트 구현 시 주석 해제(아직 구현 안됨)
	free(frame);
}


/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	struct thread *cur = thread_current();
	page = spt_find_page(&cur -> spt, va);
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	struct thread *t = thread_current();
	if(frame == NULL || page == NULL) return false;

	/* Set links */
	frame->page = page;
	page->frame = frame;

	if(!pml4_set_page(t->pml4, page->va, frame->kva, page->writable)){
		vm_dealloc_frame(frame);
		return false;
	}

	/* swap_in(uninit_initializer 실패 시 자원 회수)*/
	if(swap_in(page, frame ->kva) == false){
		pml4_clear_page(t->pml4, page->va);
		vm_dealloc_frame(frame);
		return false; 
	}

	return true;
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	if(spt == NULL) return;
	/* 해시 함수로 다시 구현 */
	if(!hash_init(&spt->hs_table, page_hash, page_less, NULL))
		PANIC("spt initialize failed");
}

/* Copy supplemental page table from src to dst */
// src(부모 프로세스의 spt)의 supplemental page table을 dst(자식 프로세스의 spt)로 복사
bool supplemental_page_table_copy (struct supplemental_page_table *dst, struct supplemental_page_table *src) {
	if (dst == NULL || src == NULL)
		return false;

	struct hash_iterator hi;
	hash_first(&hi, &src->hs_table);

	while(hash_next(&hi)) {
		struct page *parent_page = hash_entry(hash_cur(&hi), struct page, hs_elem);
		void *va = parent_page->va;
		bool writable = parent_page->writable;
		enum vm_type type = parent_page->operations->type;

		if (type == VM_UNINIT) {
			vm_initializer *vi = parent_page->uninit.init;
			struct aux_load *src_aux = parent_page->uninit.aux;
			struct aux_load *dst_aux = malloc(sizeof(struct aux_load));

			struct file *src_file = src_aux->elf_file;
			if (dst_aux == NULL)
				return false;

			memcpy(dst_aux, src_aux, sizeof(struct aux_load));
			dst_aux->elf_file = file_reopen(src_file);

			bool vapwi = vm_alloc_page_with_initializer(parent_page->uninit.type, va, writable, vi, dst_aux);
			if (vapwi == false)
				return false;
		} else {
			bool vap = vm_alloc_page(parent_page->uninit.type, va, writable);
			bool vcp = vm_claim_page(va);

			struct page *child_page = spt_find_page(dst, va);
			if (vap == false || vcp == false || child_page == NULL) {
				return false;
			}

			memcpy(child_page->frame->kva, parent_page->frame->kva, PGSIZE);
		}
	}
	return true;
}

/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill (struct supplemental_page_table *spt) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}


static uint64_t 
page_hash(const struct hash_elem *p_, void *aux UNUSED){
	const struct page *p = hash_entry(p_, struct page, hs_elem);
	return hash_bytes(&p->va, sizeof(p->va));
}


static bool 
page_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED){
	const struct page *a = hash_entry(a_, struct page, hs_elem);
	const struct page *b = hash_entry(b_, struct page, hs_elem);

	return a->va < b->va; 
}