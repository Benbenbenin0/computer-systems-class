/*
 * mm.c
 * ananyak - Ananya Kumar
 *
 * Malloc/Realloc/Calloc implementation
 * 
 * The core functions in this library are:
 *
 * 1. void *malloc (size_t size) allocates at least
 *    size bytes in the heap and returns a pointer to
 *    the start of the memory segment. Returns NULL on 
 *    failure.
 * 2. void free (void *ptr) frees the segment that begins
 *    at memory location pointed to by ptr. This memory must
 *    have been allocated by a call to malloc/calloc/realloc.
 * 3. see below for a description of calloc and realloc, which
 *    are implemented as normal.
 *
 * Notes: This implementation assumes that the size of the
 * heap will not exceed 2^32 bytes.
 * 
 * Overview: Extends the heap using sbrk when more space is
 * needed. If memory is freed using the function
 * free then subsequent allocation calls attempt to use
 * memory in the heap that has been freed (instead of
 * extending the heap). Malloc searches for the smallest
 * available (ideal) block of memory that satisfies the 
 * requested size.
 *
 * We use lists to keep track of blocks in the heap that are
 * free (because they've been freed). To find the 'ideal'
 * free block, we could search through all the free blocks.
 * To speed up the search, we segregate the lists into bins
 * based on their size. When we free a block we add it into
 * the list in the appropriate bin.
 *
 * The code also maintains one free block at the end of the heap
 * called the wilderness. This section is considered to be
 * infinitely large and is not in any list. It is handled
 * separately.
 *
 * Blocks: We need to keep track of the size of each block
 * (for free), and pointers for the list of free blocks.
 * To do this, each block has a header and a footer. The header
 * starts on byte 8n+4 where n is some integer, and the footer
 * starts on byte 8M where m is some integer.
 *
 * Free blocks:
 *
 *  8n+4                                   8m
 *   v                                     v
 *   *------*------*----------------*------*------*
 *   | size | left |     Buffer     | rite | size |
 *   *------*------*----------------*------*------*

 * Allocated blocks:
 *
 *  8n+4   8n+8                            8m
 *   v      v                              v
 *   *------*------------------------------*------*
 *   | size |           Payload            | size |
 *   *------*------------------------------*------*
 *
 * size refers to the size of the block (in bytes), including
 * the header and footer. left and right are used to maintain
 * the doubly linked free list. The block on the left is
 * 'left' words from the start of the heap, and similarly
 * for the right. Since the size is a multiple of 8, we use
 * the lowest bit of the size in the header to store whether
 * the block is free or not. WARNING: the lowest bit on the
 * footer does not store whether the block is free.
 *
 * Code: The code begins with block functions that help us 
 * extract information from blocks of memory (such as the
 * size of the block, or the left block of the current free
 * block).
 * 
 * mm_checkheap and is_list are debugging functions that
 * check for the heap's validity.
 * 
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "contracts.h"

#include "mm.h"
#include "memlib.h"


// Create aliases for driver tests
// DO NOT CHANGE THE FOLLOWING!
#ifdef DRIVER
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif

 /* Sizes of the bins that we use for the free lists. These can be
    changed to impact performance without changing any other
    part of the code. */
#define SMALLBINS 8 //No. of bins of size 8 (exact bins)
#define MEDBINS 2 //No. of bins of size 64
#define BIGBINS 8 //No. of bins of size 1024

 //Constants
 #define CHUNKSIZE 400 //Extend the heap by at least CHUNKSIZE bytes
 #define MINALLOC 8 //Minimum space that malloc will allocate
 #define HSIZE 8 //Size of the header
 #define ALIGNMENT 8 //We align blocks to multiples of 8 bytes

 /* At the beginning of the heap we store a prologue which
    points to the start of the free lists for each bin. This
    variable is used to determine whether a location in memory
    is part of the prologue. It is the offset to the last
    4-byte chunk in the prologue, measure in words */
 #define LISTZONE (SMALLBINS+MEDBINS+BIGBINS)

 /* Location of the first memory block from heapStart 
    (measured in bytes) */
 #define FIRST (LISTZONE*4+4)

 //Global Variables
 uint32_t *wildPtr; //Start of the wild section
 uint32_t *heapStart; //Start of the heap

/*
 *  Logging Functions
 *  -----------------
 *  - dbg_printf acts like printf, but will not be run in a release build.
 *  - checkheap acts like mm_checkheap, but prints the line it failed on and
 *    exits if it fails.
 */

#ifndef NDEBUG
#define dbg_printf(...) printf(__VA_ARGS__)
#define checkheap(verbose) do {if (mm_checkheap(verbose)) {  \
                             printf("Checkheap failed on line %d\n", __LINE__);\
                             exit(-1);  \
                        }}while(0)
#else
#define dbg_printf(...)
#define checkheap(...)
#endif

/*
 *  Helper functions
 *  ----------------
 */

// Align p to a multiple of w bytes
static inline void* align(const void const* p, unsigned char w) {
    return (void*)(((uintptr_t)(p) + (w-1)) & ~(w-1));
}

// Check if the given pointer is 8-byte aligned
static inline int aligned(const void const* p) {
    return align(p, 8) == p;
}

// Return whether the pointer is in the heap.
static int in_heap(const void* p) {
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}


/*
 *  Block Functions
 *  ---------------
 *  These functions manipulate blocks and extract information from them
 */

// Return the size of the given block (number of bytes, excluding header).
static inline uint32_t block_size(const uint32_t* block) {
    REQUIRES(block != NULL);
    REQUIRES(in_heap(block));
    return (block[0] & 0xFFFFFFF8);
}

// Return true if the block is free, false otherwise
static inline int block_free(const uint32_t* block) {
    REQUIRES(block != NULL);
    REQUIRES(in_heap(block));
    return (block[0] & 1);
}

// Mark the given block as free(1)/alloced(0) by marking the header and footer.
static inline void block_mark(uint32_t* block, int fr) {
    REQUIRES(block != NULL);
    REQUIRES(in_heap(block));
    block[0] = (block[0] & 0xFFFFFFFE) + fr;
}

// Pack the size and free bit into a block
static inline void block_pack(uint32_t* block, int size, int fr) {
    REQUIRES(block != NULL);
    REQUIRES(in_heap(block));
    REQUIRES(size%8 == 0);
    block[0] = size + fr;
}

// Return a pointer to the memory malloc should return
static inline uint32_t* block_mem(uint32_t* const block) {
    REQUIRES(block != NULL);
    REQUIRES(in_heap(block));
    REQUIRES(aligned(block + 1));
    return block+1;
}

// Return the header to the previous block
static inline uint32_t* block_prev(uint32_t* const block) {
    REQUIRES(block != NULL);
    REQUIRES(in_heap(block));
    return block - (block_size(block-1)/4);
}

// Return the header to the next block
static inline uint32_t* block_next(uint32_t* const block) {
    REQUIRES(block != NULL);
    REQUIRES(in_heap(block));
    return block + (block_size(block)/4);
}

// Return the free block to the left
static inline uint32_t* block_left(uint32_t* const block) {
    REQUIRES(block != NULL);
    REQUIRES(in_heap(block));
    REQUIRES(block_free(block));
    REQUIRES(block != wildPtr);
    return heapStart + block[1];
}

// Return the free block to the right
static inline uint32_t* block_right(uint32_t* const block) {
    REQUIRES(block != NULL);
    REQUIRES(in_heap(block));
    REQUIRES(block_free(block));
    REQUIRES(block != wildPtr);
    return heapStart + block[2];
}

// Makes block's left ptr point to 'left'
static inline void edit_left (uint32_t* const block, uint32_t *left) {
    REQUIRES(block != NULL);
    REQUIRES(in_heap(block));
    REQUIRES(block_free(block));
    REQUIRES(block != wildPtr);
    block[1] = left-heapStart;
}

// Makes block's right ptr point to 'right'
static inline void edit_right (uint32_t* const block, uint32_t *right) {
    REQUIRES(block != NULL);
    REQUIRES(in_heap(block));
    REQUIRES(block_free(block));
    REQUIRES(block != wildPtr);
    block[2] = right-heapStart;
}

/*
 *  Segregated List Bin Functions
 *  ---------------------
 *  These functions extract information related to the segregated list.
 */

 /*
  * Given the size of a block (in bytes) returns the index of the bin
  * that the block should be in
  */

static inline uint32_t get_bin (uint32_t size)
{
    REQUIRES(size%ALIGNMENT == 0);
    REQUIRES(size >= MINALLOC+HSIZE);

    if (size < 16+SMALLBINS*8)
        return size/8-2;
    else if (size < 16+SMALLBINS*8+MEDBINS*64)
        return (size-(16+SMALLBINS*8))/64+SMALLBINS;
    else if (size < 16+SMALLBINS*8+MEDBINS*64+BIGBINS*3072)
        return (size-(16+SMALLBINS*8+MEDBINS*64))/3072+SMALLBINS+MEDBINS;
    return SMALLBINS+MEDBINS+BIGBINS;
}


/*
 *  Check Functions
 *  ---------------------
 *  The following functions serve to check whether the data structure
 *  invariants are maintained.
 */

/*
 * Returns true if there there is a valid list starting from 'start'
 */
 int is_list (uint32_t *start)
 {
    uint32_t *prev = start;
    uint32_t *cur = heapStart + prev[0];

    while (cur != start)
    {
        if (prev != block_left(cur))
        {
            return 0;
        }

        if (!block_free(cur))
        {
            return 0;
        }

        prev = cur;
        cur = block_right(cur);
    }

    return 1;
 }

/*
 *  Returns 0 if no errors were found, otherwise returns the error
 */
int mm_checkheap(int verbose) 
{
    //Check that wild pointer is in the heap
    if (!in_heap(wildPtr)) 
        return 1;

    //Check the heap
    uint32_t *bPtr = heapStart+(FIRST/4);
    uint32_t freeBlocks = 0;
    int prevFree = 0;

    while (bPtr != wildPtr)
    {
        if (block_size(bPtr) % 8) //Block alignment
        {
            if (verbose)
            {
                printf("Block size not aligned: %d\n", block_size(bPtr));
                fflush(stdout);
            }

            return 1;
        }

        if (block_size(bPtr) < MINALLOC + HSIZE) //Min Block size
        {
            if (verbose)
            {
                printf("Block size is below minimum block size\n");
                fflush(stdout);
            }

            return 1;
        }

        if (!aligned(block_mem(bPtr))) //Size alignment
        {
            if (verbose)
            {
                printf("Block payload is not aligned properly\n");
                fflush(stdout);
            }

            return 1;
        }

        if (block_prev(block_next(bPtr)) != bPtr) //Prev block pointer
        {
            if (verbose)
            {
                printf("Problem with prev pointer in a block\n");
                fflush(stdout);
            }

            return 1;
        }

        if (block_free(bPtr)) //No consecutive free blocks
        {
            if (prevFree)
            {
                printf("There are 2 free blocks next to each other\n");
                fflush(stdout);
                return 1;
            }

            freeBlocks++;
            prevFree = 1;
        }

        else
            prevFree = 0;

        bPtr = block_next(bPtr);
    }

    //Check wilderness
    if (block_size(bPtr) % 8) 
        return 1;
    bPtr = block_next(bPtr);
    bPtr += 1;
    if ((uint64_t)(bPtr) != (uint64_t)(mem_heap_hi())+1) 
        return 1;

    //Check the segregated lists
    uint32_t b;
    uint32_t listBlocks = 0;

    if (LISTZONE%2 != 0)
    {
        printf("Number of bins is even\n");
        return 1;
    }

    for (b = 0; b <= LISTZONE; b++) //Check each free list
    {
        uint32_t *listHead = heapStart+b;
        ASSERT(b <= LISTZONE);
        uint32_t *prev = listHead;
        uint32_t *cur = heapStart + listHead[0];

        while (cur != listHead)
        {
            if (get_bin(block_size(cur)) != b) //block in wrong bin
            {
                if (verbose)
                {
                    printf("Wrong sized block in explicit list %d\n", b);
                    fflush(stdout);
                }

                return 1;
            }

            if (prev != block_left(cur)) //check left block
            {
                if (verbose)
                {
                    printf("Problem with explicit list %d\n", b);
                    fflush(stdout);
                }

                return 1;
            }

            if (!block_free(cur)) //block must be free
            {
                if (verbose) 
                {
                    printf("Block in explicit list %d is not free\n", b);
                    fflush(stdout);
                }

                return 1;
            }

            prev = cur;
            cur = block_right(cur);
            listBlocks++;
        }
    }

    //Number of free blocks in heap and explicit list should be the same
    if (freeBlocks != listBlocks)
    {
        if (verbose)
        {
            printf("Free block counts don't match\n");
            fflush(stdout);
        }

        return 1;
    }

    return 0;
}

/*
 *  Utility Functions
 *  ---------------
 *  These functions manipulate the data structures and do most of the work
 *  involved.
 */

/*
 *  Insert the block referenced by freePtr to an appropriate free list
 */
static inline void list_insert (uint32_t *freePtr)
{ 
    REQUIRES(freePtr != NULL);

    uint32_t bidx = get_bin(block_size(freePtr)); //which free list?
    uint32_t *listHead = heapStart+bidx;
    uint32_t *listFirst = heapStart + listHead[0];

    if (listFirst == listHead) //List is empty
    {
        //Point listHead and freePtr to each other
        listHead[0] = (uint32_t)(freePtr-heapStart);
        edit_left(freePtr, listHead);
        edit_right(freePtr, listHead);
    }
    
    else
    {
        //Get the block listHead is pointing to and handle links
        listHead[0] = (uint32_t)(freePtr-heapStart);
        edit_left(freePtr, listHead);
        edit_right(freePtr, listFirst);
        edit_left(listFirst, freePtr);
        fflush(stdout);
    }

    ENSURES(freePtr == heapStart + listHead[0]);
    ENSURES(listHead == block_left(freePtr));
    ENSURES(is_list(listHead));
}


/*
 *  Remove the free block referenced by free_ptr from its list
 *  Note the pointer should point to the block, and not to the
 *  memory the user wishes to free.
 */
static inline void list_delete (uint32_t *freePtr)
{
    uint32_t left = freePtr[1];
    uint32_t right = freePtr[2];
    uint32_t *leftPtr = heapStart+left;

    //Get the left block to point to the right block
    if (left <= LISTZONE)
        leftPtr[0] = right;

    else
        leftPtr[2] = right;
    
    //Get the right block to point to the left block
    if (right > LISTZONE)
    {
        uint32_t *rightPtr = heapStart+right;
        rightPtr[1] = left;
    }
}

/*
*   Given a pointer to a free block and memory size, allocates space in
*   the block and returns a pointer to the allocated block. Size must be
*   a multiple of 8, and should include the header
*/
static inline void *place (uint32_t *ptr, uint32_t size)
{
    REQUIRES(size%8 == 0);
    REQUIRES(size >= MINALLOC + HSIZE);
    REQUIRES(ptr != NULL);
    REQUIRES(block_free(ptr));
    REQUIRES(block_size(ptr) >= size);

    list_delete(ptr);
    uint32_t bsize = block_size(ptr);

    if (bsize >= size+MINALLOC+HSIZE)
    {
        //Fragment the free block to allocate memory
        block_pack(ptr, size, 0);
        uint32_t *freePtr = ptr + size/4;
        block_pack(freePtr-1, size, 0);

        //Make a free block with the remainder
        bsize -= size;
        block_pack(freePtr, bsize, 1);
        block_pack(freePtr+(bsize/4-1), bsize, 1);
        list_insert(freePtr);
    }

    else
        block_mark(ptr, 0);

    ENSURES(block_size(ptr) >= size);
    checkheap(1);
    return ptr+1;
}


/*
 *  Given pointer to start of a free list, and memory size, attempts to 
 *  allocate space in list. otherwise returns NULL. Size must be a 
 *  multiple of 8, and includes space for the header. This function is meant
 *  for lists where all blocks have the same size, so it only checks the
 *  first block in the list. It assumes that size fits into a block
 *  in this list.
 */
static inline uint32_t *list_alloc_exact (uint32_t *start, uint32_t size)
{
    REQUIRES(size%8 == 0);
    REQUIRES(size >= MINALLOC + HSIZE);
    REQUIRES(get_bin(size) <= (start-heapStart));
    checkheap(1);
    REQUIRES(is_list(start));
    
    uint32_t *curPtr = heapStart + start[0];

    if (curPtr != start) //If list non-empty
    {
        return place(curPtr, size);
    }

    checkheap(1);
    return NULL;
}


/*
 *  Given pointer to start of a free list, and memory size, attempts to 
 *  allocate space in list. otherwise returns NULL. Size must be a 
 *  multiple of 8, and includes space for the header. Allocates the
 *  smallest block in the list whose size is >= specified size.
 */
static inline uint32_t *list_alloc_best (uint32_t *start, uint32_t size)
{
    REQUIRES(size%8 == 0);
    REQUIRES(size >= MINALLOC + HSIZE);
    checkheap(1);
    REQUIRES(is_list(start));
    
    uint32_t *curPtr = heapStart + start[0];
    uint32_t best = 0xFFFFFFFF;
    uint32_t *bestPtr = NULL;
    int ctr = 0;


    while (curPtr != start && ctr <= 5) //Best fit search
    {
        if (block_size(curPtr) >= size && block_size(curPtr) <= best)
        {
            best = block_size(curPtr);
            bestPtr = curPtr;
        }

        curPtr = block_right(curPtr);
        ctr++;
    }

    if (bestPtr != NULL)
    {
        return place(bestPtr, size);
    }
        
    checkheap(1);
    return NULL;
}

/*
 *  Expands the wilderness by at least chunk size to accomodate new
 *  memory request
 */
static inline uint32_t wild_expand (uint32_t size)
{
    REQUIRES(size%8 == 0);
    if (size < CHUNKSIZE) 
        size = CHUNKSIZE;
    if ((long long)mem_sbrk(size) < 0) return 0;
    return size;
}

/*
 *  Insert a given size into the wilderness, expanding the 
 *  wilderness if necessary
 */
static inline uint32_t *wild_alloc (uint32_t size)
{
    REQUIRES(size%8 == 0);
    REQUIRES(size >= MINALLOC + HSIZE);
    checkheap(1);

    uint32_t wpsize = block_size(wildPtr);
    ASSERT(wpsize%8 == 0);
    
    //If not enough space, ask for more
    if (wpsize < size+MINALLOC+HSIZE)
    {
        uint32_t inc = wild_expand(size-wpsize+MINALLOC+HSIZE);
        if (inc == 0) return NULL;
        wpsize += inc;
    }

    ASSERT(wpsize >= size+MINALLOC+HSIZE);

    //Insert new block into wild region
    uint32_t *allocPtr = wildPtr+1;
    wildPtr += size/4;
    wpsize -= size;

    ASSERT(in_heap(allocPtr));
    ASSERT(aligned(allocPtr));
    ASSERT(in_heap(wildPtr));

    //Set header for allocated block
    block_pack(allocPtr-1, size, 0);
    block_pack(wildPtr-1, size, 0);
    
    //Modify header for wild block
    ASSERT(wpsize >= MINALLOC+HSIZE);
    block_pack(wildPtr, wpsize, 1);
    block_pack(wildPtr+(wpsize/4-1), wpsize, 1);

    checkheap(1);
    return allocPtr;
}

/*
 *  Malloc Implementation
 *  ---------------------
 *  The following functions deal with the user-facing malloc implementation.
 */

/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) 
{
    uint32_t size = wild_expand(4+FIRST+HSIZE+MINALLOC);
    if (size == 0) return -1;
    heapStart = mem_heap_lo();
    ASSERT(aligned(heapStart));

    int b;

    for(b = 0; b <= LISTZONE; b++) //Seg lists
    {
        *(heapStart+b) = b;
    }

    uint32_t freeSize = size-(4+FIRST); //Wilderness
    wildPtr = heapStart+(FIRST/4);
    block_pack(wildPtr, freeSize, 1);
    block_pack(wildPtr+(freeSize/4-1), freeSize, 1);
    checkheap(1);
    return 0;
}

/*
 * malloc
 */
void *malloc (size_t size) 
{
    checkheap(1);  // Let's make sure the heap is ok!

    if (size == 0) //Ignore empty requests
        return NULL;

    uint32_t newsize;

    if (size <= MINALLOC) //Minimum size to store headers
        newsize = MINALLOC+HSIZE;
    else //Align size to ALIGNMENT
        newsize = ((size+HSIZE+ALIGNMENT-1)/ALIGNMENT)*ALIGNMENT;

    ASSERT(newsize%8 == 0);
    ASSERT(newsize >= size+HSIZE);
    ASSERT(newsize >= MINALLOC+HSIZE);

    //Try allocating from a freed block
    uint32_t b;
    void *p;

    for (b = get_bin(newsize); b <= LISTZONE; b++)
    {
        if (*(heapStart+b) != b) //If the explicit list is non-empty
        {
            if (b < SMALLBINS) 
                p = list_alloc_exact(heapStart+b, newsize);
            else
                p = list_alloc_best(heapStart+b, newsize);
            if (p != NULL)
                return p;
        }
    }

    //Otherwise allocate from the wilderness (end of the heap)
    p = wild_alloc(newsize);

    return p;
}

/*
 * free
 */
void free (void *ptr) 
{
    if (ptr == NULL) {
        return;
    }

    uint32_t *bPtr = ptr;
    bPtr--;
    block_mark(bPtr, 1);

    //Coalesce before inserting into free list data structure

    if (bPtr != (heapStart + FIRST/4) && block_free(block_prev(bPtr)))
    {
        //Coalesce with block to the left
        uint32_t *prev = block_prev(bPtr);
        list_delete(prev);
        uint32_t newSize = block_size(prev) + block_size(bPtr);
        bPtr = prev;
        block_pack(bPtr, newSize, 1);
        block_pack(bPtr+(newSize/4-1), newSize, 1);
    }

    if (block_next(bPtr) == wildPtr)
    {
        //Coalesce with wilderness
        uint32_t wildSize = block_size(wildPtr) + block_size(bPtr);
        wildPtr = bPtr;
        block_pack(wildPtr, wildSize, 1);
        block_pack(wildPtr+(wildSize/4-1), wildSize, 1);
    }

    else
    {
        if (block_free(block_next(bPtr)))
        {
            //Coalesce with the block to the right
            uint32_t *next = block_next(bPtr);
            list_delete(next);
            uint32_t newSize = block_size(bPtr) + block_size(next);
            block_pack(bPtr, newSize, 1);
            block_pack(bPtr+(newSize/4-1), newSize, 1);
        }

        list_insert(bPtr);
    }
}

/*
 * realloc
 */
void *realloc(void *oldptr, size_t size) 
{
    size_t oldsize;
    void *newptr;

    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0) {
        free(oldptr);
        return 0;
    }

    /* If oldptr is NULL, then this is just malloc. */
    if (oldptr == NULL) {
        return malloc(size);
    }

    /* If the old block is large enough, then we don't have to allocate 
    more space */
    uint32_t *bPtr = oldptr;
    bPtr--;
    if (block_size(bPtr) >= size+HSIZE) return oldptr;

    newptr = malloc(size);

    /* If realloc() fails the original block is left untouched  */
    if(!newptr) {
        return 0;
    }

    /* Copy the old data. */
    oldsize = block_size((uint32_t *)oldptr - 1)-HSIZE;
    if(size < oldsize) oldsize = size;
    memcpy(newptr, oldptr, oldsize);

    /* Free the old block. */
    free(oldptr);

    return newptr;
}

/*
 * calloc - you may want to look at mm-naive.c
 */
void *calloc (size_t nmemb, size_t size) 
{
    size_t bytes = nmemb * size;
    void *newptr;

    newptr = malloc(bytes);
    memset(newptr, 0, bytes);

    return newptr;
}