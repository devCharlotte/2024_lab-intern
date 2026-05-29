// Explicit Linked List - Best fit
//
// 프리 블록만 따로 이중 연결 리스트로 관리 (LIFO, 새 프리 블록은 맨 앞에)
// 프리 블록 payload 앞 8B에 pred / succ 를 넣는데, 64비트에서도 4B 칸에 들어가도록
// 포인터 대신 힙 시작 주소 기준 오프셋을 저장 (0 이면 NULL)
// 할당 블록 구조, prologue / epilogue, coalesce 방식은 implicit 버전과 같음

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
#define MIN_BLOCK   (2 * DSIZE)     // header + pred + succ + footer

#define MAX(x, y)   ((x) > (y) ? (x) : (y))

#define PACK(size, alloc)   ((size) | (alloc))

#define GETTER(p)       (*(unsigned int *)(p))
#define PUTTER(p, val)  (*(unsigned int *)(p) = (unsigned int)(val))

#define GET_SIZE(p)       (GETTER(p) & ~0x7)   // 블록 크기
#define IS_ALLOCATED(p)   (GETTER(p) & 0x1)    // 블록 할당 여부

#define HDPT(bp)    ((char *)(bp) - WSIZE)                         // 헤더 포인터
#define FTPT(bp)    ((char *)(bp) + GET_SIZE(HDPT(bp)) - DSIZE)    // 풋터 포인터

#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE))   // 다음 블록
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))   // 이전 블록

// 프리 리스트 링크 (오프셋 <-> 포인터)
#define TO_OFS(p)    ((p) == NULL ? 0 : (unsigned int)((char *)(p) - heap_base))
#define TO_PTR(ofs)  ((ofs) == 0 ? NULL : heap_base + (ofs))

#define PRED_FREEPT(bp)     TO_PTR(GETTER(bp))                      // 이전 프리 블록
#define SUCC_FREEPT(bp)     TO_PTR(GETTER((char *)(bp) + WSIZE))    // 다음 프리 블록
#define SET_PRED(bp, p)     PUTTER(bp, TO_OFS(p))
#define SET_SUCC(bp, p)     PUTTER((char *)(bp) + WSIZE, TO_OFS(p))

static char* heap_base = NULL;  // 힙 시작 주소 (오프셋 기준)
static char* heap_list = NULL;  // prologue 블록
static char* free_list = NULL;  // 프리 리스트 맨 앞

static void* extend_heap(size_t words);
static void* coalesce(void* bp);
static void* best_fit(size_t a_size);
static void place(void* bp, size_t a_size);
static size_t adjust_size(size_t size);
static void putFreeBlock(void* bp);
static void rmFreeBlock(void* bp);

// mm_init
int mm_init(void)
{
    if ((heap_list = mem_sbrk(4 * WSIZE)) == (void*)-1)
        return -1;

    heap_base = heap_list;
    free_list = NULL;

    PUTTER(heap_list, 0);                               // 정렬용 패딩
    PUTTER(heap_list + (1 * WSIZE), PACK(DSIZE, 1));    // prologue header
    PUTTER(heap_list + (2 * WSIZE), PACK(DSIZE, 1));    // prologue footer
    PUTTER(heap_list + (3 * WSIZE), PACK(0, 1));        // epilogue header
    heap_list += (2 * WSIZE);

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}

// extend_heap - words 만큼 힙을 늘리고 새 프리 블록을 리턴 (프리 리스트에 들어간 상태)
static void* extend_heap(size_t words)
{
    char* bp;
    size_t size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    PUTTER(HDPT(bp), PACK(size, 0));
    PUTTER(FTPT(bp), PACK(size, 0));
    PUTTER(HDPT(NEXT_BLKP(bp)), PACK(0, 1));    // 새 epilogue

    return coalesce(bp);
}

// coalesce - 앞뒤 프리 블록을 리스트에서 빼서 합친 뒤 합친 블록을 리스트에 넣음
static void* coalesce(void* bp)
{
    size_t prev_alloc = IS_ALLOCATED(FTPT(PREV_BLKP(bp)));
    size_t next_alloc = IS_ALLOCATED(HDPT(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDPT(bp));

    if (prev_alloc && !next_alloc) {            // 뒤만 프리
        rmFreeBlock(NEXT_BLKP(bp));
        size += GET_SIZE(HDPT(NEXT_BLKP(bp)));
        PUTTER(HDPT(bp), PACK(size, 0));
        PUTTER(FTPT(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc) {       // 앞만 프리
        rmFreeBlock(PREV_BLKP(bp));
        size += GET_SIZE(HDPT(PREV_BLKP(bp)));
        PUTTER(FTPT(bp), PACK(size, 0));
        PUTTER(HDPT(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else if (!prev_alloc && !next_alloc) {      // 둘 다 프리
        rmFreeBlock(PREV_BLKP(bp));
        rmFreeBlock(NEXT_BLKP(bp));
        size += GET_SIZE(HDPT(PREV_BLKP(bp))) + GET_SIZE(FTPT(NEXT_BLKP(bp)));
        PUTTER(HDPT(PREV_BLKP(bp)), PACK(size, 0));
        PUTTER(FTPT(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    putFreeBlock(bp);
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

    if ((bp = best_fit(a_size)) == NULL) {
        extend_size = MAX(a_size, CHUNKSIZE);
        if ((bp = extend_heap(extend_size / WSIZE)) == NULL)
            return NULL;
    }

    rmFreeBlock(bp);
    place(bp, a_size);
    return bp;
}

// best fit - 리스트 전체에서 들어가는 블록 중 가장 작은 것
static void* best_fit(size_t a_size)
{
    char* bp;
    char* best = NULL;
    size_t b_size = 0;

    for (bp = free_list; bp != NULL; bp = SUCC_FREEPT(bp)) {
        size_t size = GET_SIZE(HDPT(bp));

        if (size < a_size)
            continue;
        if (best == NULL || size < b_size) {
            best = bp;
            b_size = size;
            if (size == a_size)     // 딱 맞으면 바로 끝
                break;
        }
    }
    return best;
}

// place - bp를 a_size 만큼 할당 (bp는 프리 리스트에서 빠져 있어야 함)
// 남는 공간이 최소 블록 이상이면 쪼개서 나머지는 프리로
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
        coalesce(rest);     // 리스트에 넣는 것까지 여기서
    }
    else {
        PUTTER(HDPT(bp), PACK(c_size, 1));
        PUTTER(FTPT(bp), PACK(c_size, 1));
    }
}

// putFreeBlock - 프리 리스트 맨 앞에 추가
static void putFreeBlock(void* bp)
{
    SET_PRED(bp, NULL);
    SET_SUCC(bp, free_list);
    if (free_list != NULL)
        SET_PRED(free_list, bp);
    free_list = bp;
}

// rmFreeBlock - 프리 리스트에서 제거
static void rmFreeBlock(void* bp)
{
    char* pred = PRED_FREEPT(bp);
    char* succ = SUCC_FREEPT(bp);

    if (pred != NULL)
        SET_SUCC(pred, succ);
    else
        free_list = succ;

    if (succ != NULL)
        SET_PRED(succ, pred);
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

    // 힙의 마지막 블록이면 모자란 만큼만 힙을 늘림
    next = NEXT_BLKP(bp);
    if (GET_SIZE(HDPT(next)) == 0) {
        if (extend_heap(MAX(a_size - c_size, MIN_BLOCK) / WSIZE) == NULL)
            return NULL;
    }

    // 뒤 블록이 프리고 합쳐서 충분하면 붙여서 씀
    if (!IS_ALLOCATED(HDPT(next)) && c_size + GET_SIZE(HDPT(next)) >= a_size) {
        rmFreeBlock(next);
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
