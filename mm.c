/* 
 * Simple, 32-bit clean allocator based on an explicit free list,
 * best fit placement, and boundary tag coalescing, as described in the
 * CS:APP2e text.  Blocks are aligned to double-word boundaries.  This
 * yields 8-byte aligned blocks on a 32-bit processor.  
 * The assignment only requires 8-byte alignment.  The
 * minimum block size is four words.
 *
 * This allocator uses the size of a pointer, e.g., sizeof(void *), to
 * define the size of a word.  This allocator also uses the standard
 * type uintptr_t to define unsigned integers that are the same size
 * as a pointer, i.e., sizeof(uintptr_t) == sizeof(void *).
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Master Minds",
    /* First member's full name */
    "Mayank Jikadara",
    /* First member's (@daiict.ac.in) email address*/
    "201201162@daiict.ac.in",
    /* Second member's full name */
    "Malay Shah",
    /* Second member's (@daiict.ac.in) email address*/
    "201201154@daiict.ac.in"
};

static char *heap_listp = NULL;
int trgt = 0;               
int freeBlk = 0;                         
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void free_add(void *bp);
static void free_remove(void *bp);
static void *find_fit(size_t adjSize);
static void place(void *bp, size_t adjSize);
//static int mm_check(void);

/* Basic constants and macros */
 #define WSIZE 4 /* Word and header/footer size (bytes) */
 #define DSIZE 8 /* Double word size (bytes) */
 #define CHUNKSIZE (1<<12) /* Extend heap by this amount (bytes) */

 #define MAX(x, y) ((x) > (y)? (x) : (y))

 /* Pack a size and allocated bit into a word */
 #define PACK(size, alloc) ((size) | (alloc))

 /* Read and write a word at address p */
 #define GET(p) (*(unsigned int *)(p))
 #define PUT(p, val) (*(unsigned int *)(p) = (val))

 /* Read the size and allocated fields from address p */
 #define GET_SIZE(p) (GET(p) & ~0x7)
 #define GET_ALLOC(p) (GET(p) & 0x1)

 /* Given block ptr bp, compute address of its header and footer */
 #define HDRP(bp) ((char *)(bp) - WSIZE)
 #define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

 /* Given block ptr bp, compute address of next and previous blocks */
 #define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
 #define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Function prototypes for heap consistency checker routines: *//*
static void checkblock(void *bp);
static void checkheap(bool verbose);
static void printblock(void *bp); 
*/

/* 
 * Requires:
 *   None.
 *
 * Effects:
 *   Initialize the memory manager.  Returns 0 if the memory manager was
 *   successfully initialized and -1 otherwise.
 *   
 */
int mm_init(void)
{
	
     trgt = 7; 
    freeBlk = 0; 

    // initialize heap, return -1 if failed
    if ((heap_listp = mem_sbrk(12*WSIZE)) == (void *)-1){
        return -1;
    }
    PUT(heap_listp, 0); //4 byte padding
    PUT(heap_listp + (1*WSIZE), PACK(5*DSIZE, 1)); //prologue header

    int i;
    for(i = 2; i < 10; i++) {
        PUT(heap_listp + (i*WSIZE), 0); 
    }

    // prologue header and footer
    PUT(heap_listp + (11*WSIZE), PACK(0, 1));//epilogue
    PUT(heap_listp + (10*WSIZE), PACK(5*DSIZE, 1));

    // start by pointing to the prologue
    heap_listp += (2*WSIZE);  
	
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL){
        return -1;
    }

    return 0; 
}


/* 
 * Requires:
 *   None.
 *
 * Effects:
 *   Allocate a block with at least "size" bytes of payload, unless "size" is
 *   zero.  Returns the address of this block if the allocation was successful
 *   and NULL otherwise.
 */
void *
mm_malloc(size_t size) 
{
	size_t asize;      /* Adjusted block size */
	size_t extendsize; /* Amount to extend heap if no fit */
	void *bp;

	/* Ignore spurious requests. */
	if (size == 0)
		return (NULL);

	/* Adjust block size to include overhead and alignment reqs. */
	if (size <= DSIZE)
		asize = 2 * DSIZE;
	else
		asize = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);

	/* Search the free list for a fit. */
	if ((bp = find_fit(asize)) != NULL) {
				
		place(bp, asize);
		
		return (bp);
	}

	/* No fit found.  Get more memory and place the block. */
	extendsize = MAX(asize, CHUNKSIZE);
	if ((bp = extend_heap(extendsize / WSIZE)) == NULL)  
	{	return (NULL);}
	place(bp, asize);
	return (bp);
}

/* 
 * Requires:
 *   "bp" is either the address of an allocated block or NULL.
 *
 * Effects:
 *   Free a block.
 */
void mm_free(void *bp)
{
        
    size_t size = GET_SIZE(HDRP(bp));

    //set header and footer    
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
        
    coalesce(bp);
}


/*
 * Requires:
 *   "ptr" is either the address of an allocated block or NULL.
 *
 * Effects:
 *   Reallocates the block "ptr" to a block with at least "size" bytes of
 *   payload, unless "size" is zero.  If "size" is zero, frees the block
 *   "ptr" and returns NULL.  If the block "ptr" is already a block with at
 *   least "size" bytes of payload, then "ptr" may optionally be returned.
 *   Otherwise, a new block is allocated and the contents of the old block
 *   "ptr" are copied to that new block.  Returns the address of this new
 *   block if the allocation was successful and NULL otherwise.
 */

void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    size_t prevAlloc = GET_ALLOC(FTRP(PREV_BLKP(oldptr)));
    size_t nextAlloc = GET_ALLOC(HDRP(NEXT_BLKP(oldptr)));
    size_t oldSize = GET_SIZE(HDRP(oldptr));
    int large;

    if(oldSize  < size + DSIZE){ 
        large = 1;
    }
    else{
        large = 0;
    }
    void *newptr;
    // if size is 0, just call mm_free
    if (size == 0){
        mm_free(ptr);
        newptr = 0;
        return NULL;
    }
    //if pointer is NULL, just call mm_malloc
    if (oldptr == NULL){        
        return mm_malloc(size);
    }
    size_t tempSize;
    size_t copySize;
    void *temp;
    // ptr is decreasing in size but there isnt enough after to make a free block
    if(large == 0) {
        return ptr;
    }
    
    // ptr is increasing in size
    else {

        int tempPrev = GET_SIZE(HDRP(PREV_BLKP(oldptr)));
        int tempNext = GET_SIZE(HDRP(NEXT_BLKP(oldptr)));
        // prev is unallocated and will create a large enough block when combined
         if(prevAlloc == 0 && ((tempPrev + oldSize) >= (size + DSIZE))){
            newptr = PREV_BLKP(oldptr);
            tempSize = GET_SIZE(FTRP(newptr));
            free_remove(PREV_BLKP(oldptr));
                size = tempSize + oldSize;

            //set new header and footer            
            PUT(HDRP(newptr), PACK(size, 1));         
            copySize = GET_SIZE(HDRP(oldptr));
            memcpy(newptr, oldptr, copySize);        
            PUT(FTRP(newptr), PACK(size, 1)); 
            return newptr;            
        }             
                
        // next is unallocated and will create a large enough block
        else if(nextAlloc == 0 && (tempNext + oldSize) >= (size + DSIZE)){
            temp = NEXT_BLKP(oldptr);
            tempSize = GET_SIZE(FTRP(temp));
            free_remove(NEXT_BLKP(ptr));
                size = tempSize + oldSize;

            //set header and footer            
            PUT(HDRP(oldptr), PACK(size, 1));
            PUT(FTRP(oldptr), PACK(size, 1)); 
            return oldptr;                            
        }

	// next and prev are unallocated and will create a large enough block 
       else if (nextAlloc == 0 && prevAlloc == 0 && (tempPrev + tempNext + oldSize) >= (size + DSIZE)){
            newptr = PREV_BLKP(oldptr);
            temp = NEXT_BLKP(oldptr);
            tempSize = GET_SIZE(FTRP(newptr)) + GET_SIZE(FTRP(temp));
            //remove from free list since they will combine into 1
            free_remove(PREV_BLKP(oldptr));
            free_remove(NEXT_BLKP(oldptr));
                size = tempSize + oldSize;

                         
            PUT(HDRP(newptr), PACK(size, 1));
            copySize = GET_SIZE(HDRP(oldptr));
            memcpy(newptr, oldptr, copySize);
            PUT(FTRP(newptr), PACK(size, 1));
            return newptr;                                       
        }
        //prev and next are already allocated
        else{         
            newptr = mm_malloc(size);
            copySize = GET_SIZE(HDRP(oldptr));
            if (size < copySize){
                copySize = size;
            }
                
            memcpy(newptr, oldptr, copySize);   
            mm_free(oldptr);
        }
        return newptr;
    }
}
/*
 * Requires:
 *   "bp" is the address of a newly freed block.
 *
 * Effects:
 *   Perform boundary tag coalescing.  Returns the address of the coalesced
 *   block.
 */

static void *coalesce(void *bp)
 {
    size_t prevAlloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t nextAlloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if(!prevAlloc && !nextAlloc) { 		/* Case 1 */
	free_remove(PREV_BLKP(bp));
        free_remove(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        free_add(bp);
    }

    else if (prevAlloc && !nextAlloc) {		/* Case 2 */
	free_remove(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));
        free_add(bp);
    }
    else if (!prevAlloc && nextAlloc) { 	/* Case 3 */
	free_remove(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        free_add(bp);
    }

    else {					/* Case 4 */
        free_add(bp);
    }
    
    return bp;
 }


/* 
 * Requires:
 *   None.
 *
 * Effects:
 *   Extend the heap with a free block and return that block's address.
 */

static void *
extend_heap(size_t words) 
{
	void *bp;
	size_t size;

	/* Allocate an even number of words to maintain alignment. */
	size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
	if ((bp = mem_sbrk(size)) == (void *)-1)  
		return (NULL);

	/* Initialize free block header/footer and the epilogue header. */
	PUT(HDRP(bp), PACK(size, 0));         /* Free block header */
	PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

	/* Coalesce if the previous block was free. */
	return (coalesce(bp));
} 


/*
 * Requires:
 *   None.
 *
 * Effects:
 *   Find a fit for a block with "asize" bytes.  Returns that block's address
 *   or NULL if no suitable block was found. 
 */
static void *find_fit(size_t size)
 {
    if(freeBlk == 0){
        return NULL;
    }
 
    int temp1 = size / 3264;
      
    if(temp1 < trgt){
        temp1 = trgt;
    }   
    else if(temp1 > 7){
        temp1 = 7;
    } 
  
    for(; temp1 < 8; temp1++)
    {
        int i = 0;
        char *bp = (char *)GET(heap_listp + (temp1 * WSIZE));
	for (;  i < 8 && (int)bp != 0 && GET_SIZE(HDRP(bp)) > 0; bp = (char *)GET(bp+WSIZE)) {
            if (!GET_ALLOC(HDRP(bp)) && (size <= GET_SIZE(HDRP(bp)))) {
                return bp;
            }
            i++;
        }
   }
    return NULL;
}

/* 
 * Requires:
 *   "bp" is the address of a free block that is at least "asize" bytes.
 *
 * Effects:
 *   Place a block of "asize" bytes at the start of the free block "bp" and
 *   split that block if the remainder would be at least the minimum block
 *   size. 
 */

static void place(void *bp, size_t size)
 {
    size_t currentSize = GET_SIZE(HDRP(bp));

    if ((currentSize - size) >= (2*DSIZE)) {
        free_remove(bp);
        PUT(HDRP(bp), PACK(size, 1));
        PUT(FTRP(bp), PACK(size, 1));
        void * nextBP = NEXT_BLKP(bp);
        PUT(HDRP(nextBP), PACK(currentSize-size, 0));
        PUT(FTRP(nextBP), PACK(currentSize-size, 0));
        free_add(nextBP);
    }
    else {
        PUT(HDRP(bp), PACK(currentSize, 1));
        PUT(FTRP(bp), PACK(currentSize, 1));
        free_remove(bp);
    }
 }


/* 
 * Remove a pointer to a free block from the list of free blocks.
 * update global values to reflect changes
 */  
 static void free_remove(void *bp){              

    size_t prev = GET(bp);
    size_t next = GET((char *)bp + WSIZE);
 
    int size = GET_SIZE(HDRP(bp));
    freeBlk--;      
    int temp1 = size / 3264;
    if(temp1 > 7){
        temp1 = 7;
    } 
 
    if(prev !=0 && next != 0) {
       PUT(((char *)prev + WSIZE), next);        
        PUT((next), prev); 
    }
    else if (prev != 0 && next == 0){
        PUT(((char *)prev + WSIZE), 0);
    }         
  
    else if (prev == 0 && next != 0)
   {
        PUT(heap_listp+(temp1 * WSIZE), next);
        PUT(next, 0);
    }

    else
    {    
        PUT(heap_listp+(temp1 * WSIZE), 0);  
    }
 }

/* 
 * Add a pointer to the free list and update globals to reflect changes
 * We iterate over the heap to find a point which has still to be associated with a block
 */  
 static void free_add(void *bp)
 {     
    void *tempNext;
    void *tempPrev; 
    int size = GET_SIZE(HDRP(bp));
    int temp1 = size / 3264;
    freeBlk++;

    if(temp1 > 7){
        temp1 = 7;
    }
    
    if(trgt > temp1 || trgt == 7){
        trgt = temp1;
    }
  
    
    char *tempCurrent = (char *)GET(heap_listp + (temp1 * WSIZE));

   if(tempCurrent !=0)  {
        tempPrev = (char *)GET(heap_listp + (temp1 * WSIZE));
        for (; (int)tempCurrent != 0 && GET_SIZE(HDRP(tempCurrent)) < (unsigned)size; tempCurrent = (char *)GET(tempCurrent+WSIZE))
	{
            tempPrev = tempCurrent;
        }
        tempCurrent = tempPrev;
        tempNext = (char *)GET(tempCurrent + WSIZE); 
        PUT(tempCurrent + WSIZE, (int)bp); 
       if((int)tempNext != 0){
            PUT(tempNext, (int)bp);
        }
	PUT(bp+WSIZE, (int)tempNext);
        PUT(bp, (int)tempCurrent); 
           
    }
	

    else{
	
        PUT(heap_listp + (temp1 * WSIZE), (int)bp); 
        tempCurrent = (char *)GET(heap_listp + (temp1 * WSIZE));
	  
        PUT(tempCurrent, 0); 
	
    	 PUT(tempCurrent+WSIZE, 0);
    }
        
}

/*
 * Requires:
 *   "bp" is the address of a block.
 *
 * Effects:
 *   Perform a minimal check on the block "bp".
 *//*
static void
checkblock(void *bp) 
{

	if ((uintptr_t)bp % DSIZE)
		printf("Error: %p is not doubleword aligned\n", bp);
	if (GET(HDRP(bp)) != GET(FTRP(bp)))
		printf("Error: header does not match footer\n");
}*/

/* 
 * Requires:
 *   None.
 *
 * Effects:
 *   Perform a minimal check of the heap for consistency. 
 *//*
void
checkheap(bool verbose) 
{
	void *bp;

	if (verbose)
		printf("Heap (%p):\n", heap_listp);

	if (GET_SIZE(HDRP(heap_listp)) != DSIZE ||
	    !GET_ALLOC(HDRP(heap_listp)))
		printf("Bad prologue header\n");
	checkblock(heap_listp);

	for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
		if (verbose)
			printblock(bp);
		checkblock(bp);
	}

	if (verbose)
		printblock(bp);
	if (GET_SIZE(HDRP(bp)) != 0 || !GET_ALLOC(HDRP(bp)))
		printf("Bad epilogue header\n");
}
*/
/*
 * Requires:
 *   "bp" is the address of a block.
 *
 * Effects:
 *   Print the block "bp".
 *//*
static void
printblock(void *bp) 
{
	bool halloc, falloc;
	size_t hsize, fsize;

	checkheap(false);
	hsize = GET_SIZE(HDRP(bp));
	halloc = GET_ALLOC(HDRP(bp));  
	fsize = GET_SIZE(FTRP(bp));
	falloc = GET_ALLOC(FTRP(bp));  

	if (hsize == 0) {
		printf("%p: end of heap\n", bp);
		return;
	}

	printf("%p: header: [%zu:%c] footer: [%zu:%c]\n", bp, 
	    hsize, (halloc ? 'a' : 'f'), 
	    fsize, (falloc ? 'a' : 'f'));
}
*/
/*
 * The last lines of this file configures the behavior of the "Tab" key in
 * emacs.  Emacs has a rudimentary understanding of C syntax and style.  In
 * particular, depressing the "Tab" key once at the start of a new line will
 * insert as many tabs and/or spaces as are needed for proper indentation.
 */

/* Local Variables: */
/* mode: c */
/* c-default-style: "bsd" */
/* c-basic-offset: 8 */
/* c-continued-statement-offset: 4 */
/* indent-tabs-mode: t */
/* End: */
