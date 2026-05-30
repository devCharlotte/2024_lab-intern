// Implicit Linked List - Best fit
//
// 블록 = [header 4B | payload | footer 4B], header/footer에 크기 | 할당 비트
// 힙 앞에 prologue(8B, 할당됨), 끝에 epilogue header(크기 0, 할당됨)를 둬서
// coalesce 할 때 힙 경계를 따로 검사하지 않아도 되게 함
// free 하면 바로 앞뒤 프리 블록이랑 합치고, realloc은 되도록 제자리에서 처리

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

/////// 워드, 헤더, 푸터 / 더블 워드 / 힙 확장 기본 바이트
#define WSIZE       4
#define DSIZE       8
#define CHUNKSIZE   (1 << 12)
#define MIN_BLOCK   (2 * DSIZE)     // header + footer + payload 8B

#define MAX(x, y)   ((x) > (y) ? (x) : (y))

// 크기와 할당 여부를 한 워드로
#define PACK(size, alloc)   ((size) | (alloc))

#define GETTER(p)       (*(unsigned int *)(p))
#define PUTTER(p, val)  (*(unsigned int *)(p) = (unsigned int)(val))

#define GET_SIZE(p)       (GETTER(p) & ~0x7)   // 블록 크기
#define IS_ALLOCATED(p)   (GETTER(p) & 0x1)    // 블록 할당 여부

#define HDPT(bp)    ((char *)(bp) - WSIZE)                         // 헤더 포인터
#define FTPT(bp)    ((char *)(bp) + GET_SIZE(HDPT(bp)) - DSIZE)    // 풋터 포인터

#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE))   // 다음 블록
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))   // 이전 블록

static char* heap_list = NULL;  // prologue 블록 (탐색 시작점)

static void* extend_heap(size_t words);
static void* coalesce(void* bp);
static void* best_fit(size_t a_size);
static void place(void* bp, size_t a_size);
static size_t adjust_size(size_t size);

// mm_init
int mm_init(void)
{
    if ((heap_list = mem_sbrk(4 * WSIZE)) == (void*)-1)
        return -1;

    PUTTER(heap_list, 0);                               // 정렬용 패딩
    PUTTER(heap_list + (1 * WSIZE), PACK(DSIZE, 1));    // prologue header
    PUTTER(heap_list + (2 * WSIZE), PACK(DSIZE, 1));    // prologue footer
    PUTTER(heap_list + (3 * WSIZE), PACK(0, 1));        // epilogue header
    heap_list += (2 * WSIZE);

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}

// extend_heap - words 만큼 힙을 늘리고 새로 생긴 프리 블록을 리턴
static void* extend_heap(size_t words)
{
    char* bp;
    size_t size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;   // 8의 배수 유지

    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    // 원래 epilogue 자리가 새 블록의 header가 됨
    PUTTER(HDPT(bp), PACK(size, 0));
    PUTTER(FTPT(bp), PACK(size, 0));
    PUTTER(HDPT(NEXT_BLKP(bp)), PACK(0, 1));    // 새 epilogue

    return coalesce(bp);    // 힙 끝 블록이 프리였으면 합쳐짐
}

// coalesce - 앞뒤 프리 블록과 합치고 합쳐진 블록 리턴
static void* coalesce(void* bp)
{
    size_t prev_alloc = IS_ALLOCATED(FTPT(PREV_BLKP(bp)));
    size_t next_alloc = IS_ALLOCATED(HDPT(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDPT(bp));

    if (prev_alloc && next_alloc) {
        return bp;
    }
    else if (prev_alloc && !next_alloc) {       // 뒤만 프리
        size += GET_SIZE(HDPT(NEXT_BLKP(bp)));
        PUTTER(HDPT(bp), PACK(size, 0));
        PUTTER(FTPT(bp), PACK(size, 0));       // header를 먼저 바꿨으니 FTPT는 합친 블록 끝
    }
    else if (!prev_alloc && next_alloc) {       // 앞만 프리
        size += GET_SIZE(HDPT(PREV_BLKP(bp)));
        PUTTER(FTPT(bp), PACK(size, 0));
        PUTTER(HDPT(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else {                                      // 둘 다 프리
        size += GET_SIZE(HDPT(PREV_BLKP(bp))) + GET_SIZE(FTPT(NEXT_BLKP(bp)));
        PUTTER(HDPT(PREV_BLKP(bp)), PACK(size, 0));
        PUTTER(FTPT(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

// 요청 크기 -> 실제 블록 크기 (header/footer 포함, 8의 배수, 최소 16)
static size_t adjust_size(size_t size)
{
    return MAX(ALIGN(size + DSIZE), MIN_BLOCK);
}

// mm_malloc
void* mm_malloc(size_t size)
{
    size_t a_size;
    size_t extend_size;
    char* bp;

    if (size == 0)
        return NULL;

    a_size = adjust_size(size);

    if ((bp = best_fit(a_size)) != NULL) {
        place(bp, a_size);
        return bp;
    }

    // 맞는 블록이 없으면 힙을 늘림
    extend_size = MAX(a_size, CHUNKSIZE);
    if ((bp = extend_heap(extend_size / WSIZE)) == NULL)
        return NULL;
    place(bp, a_size);
    return bp;
}

// best fit - 들어가는 블록 중 가장 작은 것
static void* best_fit(size_t a_size)
{
    char* bp;
    char* best = NULL;
    size_t b_size = 0;

    for (bp = heap_list; GET_SIZE(HDPT(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (IS_ALLOCATED(HDPT(bp)) || GET_SIZE(HDPT(bp)) < a_size)
            continue;
        if (best == NULL || GET_SIZE(HDPT(bp)) < b_size) {
            best = bp;
            b_size = GET_SIZE(HDPT(bp));
            if (b_size == a_size)   // 딱 맞으면 더 볼 필요 없음
                break;
        }
    }
    return best;
}

// place - bp를 a_size 만큼 할당 상태로, 남는 공간이 최소 블록 이상이면 쪼갬
static void place(void* bp, size_t a_size)
{
    size_t c_size = GET_SIZE(HDPT(bp));
    char* rest;

    if ((c_size - a_size) >= MIN_BLOCK) {
        PUTTER(HDPT(bp), PACK(a_size, 1));
        PUTTER(FTPT(bp), PACK(a_size, 1));

        rest = NEXT_BLKP(bp);
        PUTTER(HDPT(rest), PACK(c_size - a_size, 0));
        PUTTER(FTPT(rest), PACK(c_size - a_size, 0));
        coalesce(rest);     // realloc으로 줄일 때는 뒤에 프리 블록이 붙어 있을 수 있음
    }
    else {
        PUTTER(HDPT(bp), PACK(c_size, 1));
        PUTTER(FTPT(bp), PACK(c_size, 1));
    }
}

// mm_free
void mm_free(void* bp)
{
    size_t size;

    if (bp == NULL)
        return;

    size = GET_SIZE(HDPT(bp));
    PUTTER(HDPT(bp), PACK(size, 0));
    PUTTER(FTPT(bp), PACK(size, 0));
    coalesce(bp);
}

// mm_realloc - 줄이거나 뒤 블록을 붙여서 늘릴 수 있으면 제자리, 안 되면 새로 할당 후 복사
void* mm_realloc(void* bp, size_t size)
{
    size_t a_size, c_size;
    char* next;
    void* new_bp;

    if (bp == NULL)
        return mm_malloc(size);
    if (size == 0) {
        mm_free(bp);
        return NULL;
    }

    a_size = adjust_size(size);
    c_size = GET_SIZE(HDPT(bp));

    if (a_size <= c_size) {     // 줄이는 경우
        place(bp, a_size);
        return bp;
    }

    // 힙 끝에 붙어 있으면 (바로 뒤가 epilogue 거나, 뒤 프리 블록 다음이 epilogue)
    // 모자란 만큼만 힙을 늘려서 뒤에 붙임
    next = NEXT_BLKP(bp);
    if (GET_SIZE(HDPT(next)) == 0 ||
        (!IS_ALLOCATED(HDPT(next)) && GET_SIZE(HDPT(NEXT_BLKP(next))) == 0)) {
        size_t have = c_size + (IS_ALLOCATED(HDPT(next)) ? 0 : GET_SIZE(HDPT(next)));

        if (have < a_size && extend_heap(MAX(a_size - have, MIN_BLOCK) / WSIZE) == NULL)
            return NULL;
    }

    // 뒤 블록이 프리고 합쳐서 충분하면 붙여서 씀
    if (!IS_ALLOCATED(HDPT(next)) && c_size + GET_SIZE(HDPT(next)) >= a_size) {
        c_size += GET_SIZE(HDPT(next));
        PUTTER(HDPT(bp), PACK(c_size, 1));
        PUTTER(FTPT(bp), PACK(c_size, 1));
        place(bp, a_size);
        return bp;
    }

    if ((new_bp = mm_malloc(size)) == NULL)
        return NULL;
    memcpy(new_bp, bp, c_size - DSIZE);     // 기존 payload 전체
    mm_free(bp);
    return new_bp;
}
