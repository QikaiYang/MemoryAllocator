#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define REPEAT 10000
#define ST_SIZE sizeof(size_t) // 8 tbh
// 1: 1024*138 2: 1024*148 3: 1024*158 4: 1024*168 5: 1024*178 6: 1024*188
#define THRESH 1024 * 133          // 1024*256 works but 1024*128 fails
#define METASIZE sizeof(meta_data) // 24 tbh

typedef struct _meta_data
{
    int free; // Pointer to the next instance of meta_data in the list
    struct _meta_data *next;
    size_t request_size;
} meta_data;

static meta_data *head = NULL;
static size_t last_size;
static int same_freq;
static meta_data *first_free;
// static int is_test2;

/**
 * Allocate space for array in memory
 *
 * Allocates a block of memory for an array of num elements, each of them size
 * bytes long, and initializes all its bits to zero. The effective result is
 * the allocation of an zero-initialized memory block of (num * size) bytes.
 *
 * @param num
 *    Number of elements to be allocated.
 * @param size
 *    Size of elements.
 *
 * @return
 *    A pointer to the memory block allocated by the function.
 *
 *    The type of this pointer is always void*, which can be cast to the
 *    desired type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory, a
 *    NULL pointer is returned.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/calloc/
 */
void *calloc(size_t num, size_t size)
{
    void *before = sbrk(0);
    void *memo = malloc(num * size);

    if (memo == (void *)-1)
    {
        return NULL;
    }
    void *after = sbrk(0);
    if (before != after)
    {
        return memo;
    }

    memo = memset(memo, 0, num * size);
    return memo;
}

void split(meta_data *h, size_t half_size)
{
    // fprintf(stderr, "split happens!\n");
    // fprintf(stderr, "start capacity is %zu\n", half_size);
    void *start = (void *)h;
    // fprintf(stderr, "start ptr is %d, metasize is %zu, half size is %zu\n", (int)start, METASIZE, half_size);
    size_t *tag = (size_t *)(start + METASIZE + half_size); // now tag is at the right place
    *tag = half_size;                                       ///////////////////////////////////////////////////////////////////////////problem is here!
    meta_data *second = (meta_data *)(tag + 1);
    void *temp = (void *)(second + 1);
    second->free = 1;
    second->next = h->next;
    second->request_size = h->request_size - half_size - METASIZE - ST_SIZE;
    h->next = second;
    h->request_size = half_size;
    temp = temp + second->request_size;
    size_t *tag2 = (size_t *)temp;
    *tag2 = second->request_size;

    return;
}

void coal(meta_data *h, meta_data *second)
{
    void *check = (void *)(second + 1);
    size_t *tail_tag = (size_t *)(check + second->request_size);
    *tail_tag = h->request_size + second->request_size + ST_SIZE + METASIZE;
    h->next = second->next;
    h->request_size = (*tail_tag);
    // fprintf(stderr, "tail_tag value is %zu\n", *tail_tag);
    // usleep(100000);
    return;
}

meta_data *cut_half(meta_data *h, size_t num_blk, size_t blk_size, meta_data *prev)
{
    // fprintf(stderr, "sha bi\n");
    size_t total = num_blk * (METASIZE + ST_SIZE + blk_size);

    void *temp = (void *)(h + 1);
    size_t *tail_tag = (size_t *)(temp + blk_size);
    h->free = 0;
    h->next = (meta_data *)(tail_tag + 1);
    h->request_size = blk_size;
    *tail_tag = h->request_size;
    meta_data *worker = (meta_data *)(tail_tag + 1); // point to the meta_data of the next block
    worker->free = 1;
    worker->next = prev->next;
    prev->next = h;
    worker->request_size = total - (blk_size + 2 * (ST_SIZE + METASIZE));
    return h;
}

/**
 * Allocate memory block
 *
 * Allocates a block of size bytes of memory, returning a pointer to the
 * beginning of the block.  The content of the newly allocated block of
 * memory is not initialized, remaining with indeterminate values.
 *
 * @param size
 *    Size of the memory block, in bytes.
 *
 * @return
 *    On success, a pointer to the memory block allocated by the function.
 *
 *    The type of this pointer is always void*, which can be cast to the
 *    desired type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory,
 *    a null pointer is returned.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/malloc/
 */
void *malloc(size_t size)
{
    // fprintf(stderr, "same _frequency is %d\n", same_freq);
    // usleep(1000);
    if (head == NULL)
    { // construct linked list
        void *all = sbrk(size + METASIZE + ST_SIZE);
        if (all == (void *)-1)
        { // heap overflow
            return NULL;
        }
        meta_data *info = (meta_data *)all;
        head = info;
        void *ret = (void *)(info + 1);
        info->free = 0;
        info->next = NULL;
        info->request_size = size;
        size_t *tag = (size_t *)(ret + size);
        *tag = info->request_size;
        // fprintf(stderr, "tag value is %zu\n", *tag);
        return (void *)ret;
    }
    // fprintf(stderr, "sha bi\n");
    meta_data *cur = NULL;
    meta_data *prev = NULL;
    if (first_free == NULL)
    { // no free blocks, search!
        // fprintf(stderr, "sha bi\n");
        cur = head;
    }
    else
    {
        // fprintf(stderr, "sha bi\n");
        cur = first_free;
    }
    while ((cur->free == 0) || (cur->request_size < (size_t)size))
    { // walk through the linked list
        // fprintf(stderr, "head address is %d, cur address is %d\n", (int)head, (int)cur);
        // usleep(200000);
        prev = cur;
        cur = cur->next;
        if (cur == NULL)
        {
            break;
        }
    }
    if (cur == NULL)
    { // no available block, sbrk new memory
        if (size == last_size)
        {
            same_freq++;
        }
        else
        {
            same_freq = 0;
            last_size = size;
        }
        if (same_freq >= 5)
        { // ask for more block
            // is_test2 = 1;
            // fprintf(stderr, "is test2!\n");
            cur = (meta_data *)sbrk(REPEAT * (METASIZE + ST_SIZE + size));
            meta_data *after = cut_half(cur, REPEAT, size, prev);
            first_free = after->next;
            same_freq = 0;
            void *ret = (void *)(after + 1);
            return ret;
        }
        else
        { //  ask for one block
          // void *pos = sbrk(0);
          // fprintf(stderr, "address of heap is %d, needed size is %zu, meta size is %zu, llint size is %zu\n", (int)pos, size, METASIZE, ST_SIZE);
          // usleep(200000);
            // fprintf(stderr, "ask for new size\n");
            cur = (meta_data *)sbrk(size + METASIZE + ST_SIZE);
        }
        if (cur == (void *)-1)
        { // heap overflow
            return NULL;
        }
        // fprintf(stderr, "cur is null? %d, prev is null ? %d\n", (int)(cur == NULL), (int)(prev == NULL));
        cur->next = prev->next;
        cur->free = 0;
        cur->request_size = size;
        prev->next = cur;
        void *temp = (void *)(cur + 1);
        size_t *tag = (size_t *)(temp + cur->request_size);
        *tag = cur->request_size;
        // fprintf(stderr, "tag value is %zu\n", *tag);
        //  usleep(100000);
        // fprintf(stderr, "heyhey\n");
        return (void *)temp;
    }
    else
    { // found an available free block
        // fprintf(stderr, "sha bi\n");
        void *temp = (void *)(cur + 1);
        cur->free = 0;
        // fprintf(stderr, "temp is pointing to %d\n", (int)temp);
        if ((cur->request_size - size) >= THRESH)
        { // size is smaller, should spliting
            // fprintf(stderr, "split happens\n");
            split(cur, size);
            if (first_free == cur)
            {
                first_free = cur->next;
            }
        }
        return (void *)temp;
    }
}

/**
 * Deallocate space in memory
 *
 * A block of memory previously allocated using a call to malloc(),
 * calloc() or realloc() is deallocated, making it available again for
 * further allocations.
 *
 * Notice that this function leaves the value of ptr unchanged, hence
 * it still points to the same (now invalid) location, and not to the
 * null pointer.
 *
 * @param ptr
 *    Pointer to a memory block previously allocated with malloc(),
 *    calloc() or realloc() to be deallocated.  If a null pointer is
 *    passed as argument, no action occurs.
 */
void free(void *ptr)
{
    /*
    1   check if coalescing left or right is doable
    2   update infomation in meta_data
    */
    // fprintf(stderr, "free is called!\n");
    // fprintf(stderr, "ptr points to %d\n", *(int*)ptr);
    if (ptr == NULL)
    {
        return;
    }
    meta_data *info = (meta_data *)(ptr - METASIZE);
    info->free = 1;
    // first_free = info;
    //  check the previous memory tag
    if (head != info)
    { // not the first block, safe to decrease pointer
        size_t *prev_tag = (size_t *)(ptr - METASIZE - ST_SIZE);
        size_t step = *prev_tag;
        void *track = (void *)prev_tag;
        meta_data *prev = (meta_data *)(track - step - METASIZE);
        meta_data *next = info->next;
        if (prev->free == 1)
        { // check the previous pointer
            // if (first_free == info) { /////////////thank you sosososOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO much!
            first_free = prev;
            //}
            coal(prev, info);
            // fprintf(stderr, "sha bi\n");
            if ((next != NULL) && (next->free == 1))
            { // not the end, do coal!
                coal(prev, next);
                // fprintf(stderr, "sha bi\n");
                //  now next is nothing valid
            }
        }
        else
        { // check the next pointer
            if ((next != NULL) && (next->free == 1))
            { // not the end, probably could do coal
                // if (first_free == next) {
                first_free = info;
                //}
                coal(info, next);
                // fprintf(stderr, "sha bi\n");
                //  now next is nothing valid
            }
        }
    }
    else
    { // now this is the very first block, only check the next block
        meta_data *next = info->next;
        if ((next != NULL) && (next->free == 1))
        { // not the end, probably could do coal
            // if (first_free == next) {
            first_free = info;
            //}
            coal(info, next);
            // fprintf(stderr, "sha bi\n");
            //  now next is nothing valid
        }
    }
    return;
}

/**
 * Reallocate memory block
 *
 * The size of the memory block pointed to by the ptr parameter is changed
 * to the size bytes, expanding or reducing the amount of memory available
 * in the block.
 *
 * The function may move the memory block to a new location, in which case
 * the new location is returned. The content of the memory block is preserved
 * up to the lesser of the new and old sizes, even if the block is moved. If
 * the new size is larger, the value of the newly allocated portion is
 * indeterminate.
 *
 * In case that ptr is NULL, the function behaves exactly as malloc, assigning
 * a new block of size bytes and returning a pointer to the beginning of it.
 *
 * In case that the size is 0, the memory previously allocated in ptr is
 * deallocated as if a call to free was made, and a NULL pointer is returned.
 *
 * @param ptr
 *    Pointer to a memory block previously allocated with malloc(), calloc()
 *    or realloc() to be reallocated.
 *
 *    If this is NULL, a new block is allocated and a pointer to it is
 *    returned by the function.
 *
 * @param size
 *    New size for the memory block, in bytes.
 *
 *    If it is 0 and ptr points to an existing block of memory, the memory
 *    block pointed by ptr is deallocated and a NULL pointer is returned.
 *
 * @return
 *    A pointer to the reallocated memory block, which may be either the
 *    same as the ptr argument or a new location.
 *
 *    The type of this pointer is void*, which can be cast to the desired
 *    type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory,
 *    a NULL pointer is returned, and the memory block pointed to by
 *    argument ptr is left unchanged.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/realloc/
 */
void *realloc(void *ptr, size_t size)
{
    // implement realloc!
    // fprintf(stderr, "sha bi\n");
    if (ptr == NULL)
    {
        return malloc(size);
    }
    if (size == 0)
    {
        free(ptr);
        return NULL;
    }
    // fprintf(stderr, "ptr points to %d\n", *(int*)ptr);
    // usleep(100000);
    meta_data *info = (meta_data *)(ptr - METASIZE);
    if (info->request_size >= size)
    { // should shrink the memory, split happens here
        // fprintf(stderr, "sha bi\n");
        if ((info->request_size - size) >= THRESH)
        {
            // fprintf(stderr, "sha bi\n");
            split(info, size);
            // if (first_free == info) {
            //     fprintf(stderr, "sha bi\n");
            //     first_free = info ->next;
            // }
        }
        // fprintf(stderr, "sha bi\n");
        return ptr;
    }
    else
    { // should malloc new memory and copy
        // fprintf(stderr, "sha bi\n");
        void *fresh = malloc(size);
        // meta_data * temp = (meta_data*)(fresh - METASIZE);
        //  fprintf(stderr, "new block size is %zu\n", temp ->request_size);
        //  fprintf(stderr, "size required is %zu\n", size);
        fresh = memcpy(fresh, ptr, info->request_size);
        free(ptr);
        return fresh;
    }
}
