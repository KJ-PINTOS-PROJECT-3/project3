/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/mmu.h"

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
	struct page		*new_page = NULL;
	bool			(*type_initializer)(struct page *, enum vm_type, void *);

	ASSERT (VM_TYPE(type) != VM_UNINIT);

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		new_page = (struct page *)calloc(1, sizeof(struct page));
		if (!new_page)
			goto err;
		
		if (type == VM_ANON)
			type_initializer = anon_initializer;
		else if (type == VM_FILE)
			type_initializer = file_backed_initializer;
		else
			goto err;

		uninit_new(new_page, upage, init, type, aux, type_initializer);
		new_page->writable = writable;
		
		/* TODO: Insert the page into the spt. */
		if (false == spt_insert_page(spt, new_page))
			goto err;
		
		return true;
	}
err:
	if (new_page)
		free (new_page);
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	struct page 		tmp;
	struct page			*page;
	struct hash_elem	*res;
	/* TODO: Fill this function. */

	if (!spt || !va)
		return NULL;
	
	// dummy 구조체 선언
	tmp.va = pg_round_down(va);
	lock_acquire(&(spt->hash_lock));
	res = hash_find(&(spt->hash_table), &(tmp.elem));
	lock_release(&(spt->hash_lock));
	if (!res)
		return NULL;
	page = hash_entry(res, struct page, elem);
	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt,
		struct page *page) {
	struct hash_elem	*e;
	int succ = false;
	/* TODO: Fill this function. */
	lock_acquire(&(spt->hash_lock));
	e = hash_insert(&(spt->hash_table), &(page->elem));
	lock_release(&(spt->hash_lock));
	
	if (!e)
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
static struct frame *
vm_get_frame (void) {
	struct frame	*frame = NULL;
	void			*kva = NULL;
	/* TODO: Fill this function. */

	frame = (struct frame *)calloc(1, sizeof(struct frame));
	if (!frame)	//만약 에러나면 여기 의심해볼 것 
		return NULL;
	
	kva = palloc_get_page(PAL_USER);
	if (!kva)
		kva = vm_evict_frame();	//실패하면 어떻게 처리..? 
	if (!kva) //만약 에러나면 여기 의심해볼 것 
	{
		free (frame);
		return NULL;
	}

	frame->kva = kva;
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

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va) {
	struct page *page = NULL;
	/* TODO: Fill this function */

	if (!va)
		return false;
	
	page = spt_find_page(thread_current()->spt, va);
	if (!page)
		return false;

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame	*frame = vm_get_frame ();
	struct thread	*t = thread_current();
	bool			result;

	if (!page || !frame)
		return false;

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	result = (pml4_get_page(t->pml4, page->va) == NULL) && (pml4_set_page(t->pml4, page->va, frame->kva, page->writable));

	if (result == false)
	{
		palloc_free_page(frame->kva);
		free (frame);
		return false;
	}

	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	if (!spt)
		return ;
	
	hash_init(&(spt->hash_table), page_hash_func, va_less_func, NULL);
	lock_init(&(spt->hash_lock));
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src) {
	// iterator 하나 만들어서 순회 -> 하나 원소 하나 복사 (page 할당 -> hash 선언)
	// hash_init되어 들어옴 
	struct hash_iterator	i;
	struct hash_elem		*e;
	struct page				*src_page;
	struct page				*dst_page;

	lock_acquire(&(src->hash_lock));
	lock_acquire(&(dst->hash_lock));
	hash_first(&i, &(src->hash_table));
	while (hash_next (&i))
	{
		e = hash_cur(&i);
		src_page = hash_entry(e, struct page, elem);
		dst_page = (struct page *)malloc(sizeof(struct page));
		if (!dst_page)
		{
			supplemental_page_table_kill(&(dst->hash_table));
			lock_release(&(src->hash_lock));
			lock_release(&(dst->hash_lock));
			return false;
		}
		memcpy(src_page, dst_page, sizeof(struct page));
		hash_insert(&(dst->hash_table), &(dst_page->elem));
	}
	lock_release(&(src->hash_lock));
	lock_release(&(dst->hash_lock));
	return false;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	lock_acquire(&(spt->hash_lock));
	hash_destroy(&(spt->hash_table), page_destroy_func);
	lock_release(&(spt->hash_lock));
}

/*	HASH FUNCTION : for spt, page 대상	*/
uint64_t page_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
	struct page	*cur_page = hash_entry(e, struct page, elem);
	return hash_bytes(cur_page->va, sizeof(cur_page->va));
}

bool va_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
	struct page	*a_page = hash_entry(a, struct page, elem);
	struct page	*b_page = hash_entry(b, struct page, elem);
	return (uint64_t)a_page->va < (uint64_t)b_page->va;
}

void page_destroy_func (struct hash_elem *e, void *aux UNUSED)
{
	struct page	*tmp;

	if (!e)
		return ;
	
	// page가 동적으로 할당된다고 가정하에 작성
	tmp = hash_entry(e, struct page, elem);

	// 스왑영역에 올라간 경우 해당 스왑 영역 청소
	// file -> write back 허용인 경우 file에 쓰기
	// 해당 영역의 frame이 있는 경우 frame 할당 해제 

	free (tmp);
}