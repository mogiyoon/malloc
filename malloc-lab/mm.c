/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/******************************/
/* Basic constants and macros */
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12) // 4096

/* single word (4) or double word (8) alignment */
#define ALIGNMENT DSIZE

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p) (*(unsigned int*)(p))
#define PUT(p, val) (*(unsigned int*)(p) = (val))

/* Read the size and allocated fields from address p*/
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x7)

/* Given blok ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char*)(bp) - WSIZE)
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Address about prev, next free block */
#define NXFRP(bp) ((char*)(bp) + DSIZE)
#define PRFRP(bp) (char*)(bp)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Approach to prev, next free block */
#define NEXT_FBLKP(bp) *(char**)(NXFRP(bp))
#define PREV_FBLKP(bp) *(char**)(PRFRP(bp))

/******************************/

static char* fb_head_bp = NULL;
static char* fb_tail_bp = NULL;
static char* heap_listp;
static int init_count = 0;

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    // printf("init\n");
    init_count++;
    mem_deinit();
    mem_init();
    /* Create the initial empty heap */
    if ((heap_listp = (char*)mem_sbrk(4*WSIZE)) == (void *)-1)
    {
        return -1;
    }
    PUT(heap_listp, 0);  // Alignment padding
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); // Prologue header
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); // Prologue footer
    PUT(heap_listp + (3*WSIZE), PACK(0, 1)); //Epilogue header
    heap_listp += (2*WSIZE); //Payload position

    fb_head_bp = NULL;
    fb_tail_bp = NULL;

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
    {
        return -1;
    }
    return 0;
}

void* extend_heap(size_t words)
{
    // printf("extend\n");
    char *bp;
    size_t size;

    size = words * WSIZE;
    if ((long)(bp = (char*)mem_sbrk(size)) == -1)
    {
        return NULL;
    }

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0)); // Free block header
    PUT(FTRP(bp), PACK(size, 0)); // Free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));  //New epilogue header

    /* If free is not prev block */
    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

void* coalesce(void* bp)
{
    // printf("coalesce\n");
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) // Case 1
    {
        // printf("coalesce case 1\n");
    }
    else if (prev_alloc && !next_alloc) // Case 2
    {
        // printf("coalesce case 2\n");
        //Free Block pointer
        remove_connection(NEXT_BLKP(bp));
        //Block pointer
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc) // Case 3
    {
        // printf("coalesce case 3\n");
        //Free Block pointer
        remove_connection(PREV_BLKP(bp));
        //Block pointer
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else // Case 4
    {
        // printf("coalesce case 4\n");
        //Free Block pointer
        remove_connection(PREV_BLKP(bp));
        remove_connection(NEXT_BLKP(bp));
        //Block pointer
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    //Free block management
    add_to_free_list(bp);
    // printf("bp position: %p\n", HDRP(bp));
    // printf("coalesce end\n");
    return bp;
}

void remove_connection(void* bp)
{
    // printf("remove connection\n");
    if (fb_head_bp == bp && fb_tail_bp == bp) //case 1
    {
        fb_head_bp = NULL;
        fb_tail_bp = NULL;
    }
    else if (fb_head_bp == bp && fb_tail_bp != bp)
    {
        fb_head_bp = NEXT_FBLKP(bp);
        PREV_FBLKP(NEXT_FBLKP(bp)) = NULL;
    }
    else if (fb_head_bp != bp && fb_tail_bp == bp)
    {
        fb_tail_bp = PREV_FBLKP(bp);
        NEXT_FBLKP(PREV_FBLKP(bp)) = NULL;
    }
    else
    {
        PREV_FBLKP(NEXT_FBLKP(bp)) = PREV_FBLKP(bp);
        NEXT_FBLKP(PREV_FBLKP(bp)) = NEXT_FBLKP(bp);
    }
    // printf("remove end\n");
}

void add_to_free_list(void* bp)
{
    PREV_FBLKP(bp) = NULL;
    if (fb_head_bp != NULL)
    {
        NEXT_FBLKP(bp) = fb_head_bp;
        PREV_FBLKP(fb_head_bp) = bp;
        fb_head_bp = bp;
    }
    else
    {
        NEXT_FBLKP(bp) = NULL;
        fb_head_bp = bp;
        fb_tail_bp = bp;
    }
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    int newsize;
    size_t extendsize;
    char *bp;
    // printf("malloc size: %d ", size);

    /* Ignore spurious requests */
    if (size == 0)
    {
        return NULL;
    }

    /* Adjust block size to include overhead and alignment reqs */
    if (size <= 3*DSIZE)
    {
        newsize = 4*DSIZE;
    }
    else
    {
        newsize = (4 * DSIZE) * ((size + (4 * DSIZE)) /  (4 * DSIZE)) + (4 * DSIZE);
    }

    // printf("-----malloc----\n");
    // printf("new size: %d\n", newsize);
    // printf("init count: %d\n", init_count);
    // if (init_count >= 49)
    // {
    //     print_all_list();
    //     print_free_list();
    // }

    /* Search the free list for a fit */
    if ((bp = (char*)find_fit(newsize)) != NULL) {
        place(bp, newsize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(newsize, CHUNKSIZE);
    if ((bp = (char*)extend_heap(extendsize/WSIZE)) == NULL)
    {
        return NULL;
    }
    place(bp, newsize);
    return bp;
}

void* find_fit(size_t newsize)
{
    // printf("find fit\n");
    //initiate bp to free block list head bp
    char* now_bp = fb_head_bp;
    if (now_bp == NULL)
    {
        return NULL;
    }

    while (NEXT_FBLKP(now_bp) != NULL && GET_SIZE(HDRP(now_bp)) < newsize)
    {
        now_bp = NEXT_FBLKP(now_bp);
    }

    if (GET_SIZE(HDRP(now_bp)) >= newsize)
    {
        return now_bp;
    }
    else
    {
        return NULL;
    }
}

void place(void* bp, size_t newsize)
{
    // printf("place\n");
    size_t before_place_size = GET_SIZE(HDRP(bp));
    //Allocated Header & Footer
    PUT(HDRP(bp), PACK(newsize, 1));
    PUT(FTRP(bp), PACK(newsize, 1));
    //Make Next Block Header & Footer
    remove_connection(bp);

    //Change free block point of next block
    if (before_place_size > newsize)
    {
        PUT(HDRP(NEXT_BLKP(bp)), PACK(before_place_size - newsize, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(before_place_size - newsize, 0));

        //Free block management
        char* newBp = NEXT_BLKP(bp);
        add_to_free_list(newBp);
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    // printf("---------------freee-----------------\n");
    // print_free_list();
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    // printf("realloc\n");
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

void print_free_list() {
    char* now_bp = fb_head_bp;

    printf("-----print free-----\n");

    if (fb_head_bp == NULL) {
        printf("No list here\n");
    }
    printf("head: %p\n", HDRP(fb_head_bp));
    printf("tail: %p\n", HDRP(fb_tail_bp));
    while (now_bp != NULL) 
    {
        printf("prev bp position: %p ", HDRP(PREV_FBLKP(now_bp)));
        printf("now header: %p ", HDRP(now_bp));
        printf("now alloc: %d ", GET_ALLOC(HDRP(now_bp)));
        printf("now bp size: %d ", GET_SIZE(HDRP(now_bp)));
        printf("next bp position: %p ||", HDRP(NEXT_FBLKP(now_bp)));

        now_bp = NEXT_FBLKP(now_bp);
    }
    printf("\n");
}

void print_all_list() {
        //initiate bp to epilogue bp
        char* now_bp = heap_listp;

        printf("------print all------\n");
    
        //if the header does not meet condition, update it
        while(GET_SIZE(HDRP(now_bp)) != 0)
        {
            printf("now header %p ", HDRP(now_bp));
            printf("now size %d ", GET_SIZE(HDRP(now_bp)));
            printf("now allocated %d ||", GET_ALLOC(HDRP(now_bp)));
            // printf("now footer %p ", FTRP(now_bp));
            // printf("now size %d ", GET_SIZE(FTRP(now_bp)));
            // printf("now allocated %d ", GET_ALLOC(FTRP(now_bp)));
            // printf("prev bp: %p ", PREV_BLKP(now_bp));
            // printf("next bp: %p ||", NEXT_BLKP(now_bp));
            now_bp = NEXT_BLKP(now_bp);
        }
        printf("now header %p ", HDRP(now_bp));
        printf("now size %d ", GET_SIZE(HDRP(now_bp)));
        printf("now allocated %d ||", GET_ALLOC(HDRP(now_bp)));
        printf("next size %d ", GET_SIZE(HDRP(NEXT_BLKP(now_bp))));
        printf("next allocated %d ||\n", GET_ALLOC(HDRP(NEXT_BLKP(now_bp))));
}