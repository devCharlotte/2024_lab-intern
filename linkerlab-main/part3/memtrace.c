//------------------------------------------------------------------------------
// 
// Assignment 1. linkerlab
// Seong Joon Hee (220254007)
//------------------------------------------------------------------------------
// part3/memtrace.c
//
// trace calls to the dynamic memory manager
//


#define _GNU_SOURCE  // #RTLD_NEXT

#include <dlfcn.h>  // dlopen, dlsym, dlerror
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <memlog.h>
#include <memlist.h>

//
// function pointers to stdlib's memory management functions
//
static void *(*mallocp)(size_t size) = NULL;
static void (*freep)(void *ptr) = NULL;
static void *(*callocp)(size_t nmemb, size_t size) = NULL;
static void *(*reallocp)(void *ptr, size_t size) = NULL;

//
// statistics & other global variables
//
static unsigned long n_malloc  = 0;
static unsigned long n_calloc  = 0;
static unsigned long n_realloc = 0;
static unsigned long n_allocb  = 0;
static unsigned long n_freeb   = 0;
static item *list = NULL;


static __thread int silent = 0;

//
// init - this function is called once when the shared library is loaded
//
__attribute__((constructor))
void init(void) {
  char *error;

  LOG_START();  // [0001] Memory tracer started.

  // initialize a new list to keep track of all memory (de-)allocations
  // (not needed for part 1)
  list = new_list();

  // ...
  dlerror();

  mallocp  = (void *(*)(size_t))         dlsym(RTLD_NEXT, "malloc");
  callocp  = (void *(*)(size_t,size_t))  dlsym(RTLD_NEXT, "calloc");
  reallocp = (void *(*)(void*,size_t))   dlsym(RTLD_NEXT, "realloc");
  freep    = (void (*)(void*))           dlsym(RTLD_NEXT, "free");

  if ((error = dlerror()) != NULL) {
    fputs(error, stderr);
    exit(1);
  }
}

//
// fini - this function is called once when the shared library is unloaded
//
__attribute__((destructor))
void fini(void) {
  // ...
  unsigned long total_calls = n_malloc + n_calloc + n_realloc;
  unsigned long avg_alloc   = (total_calls > 0) ? (n_allocb / total_calls) : 0;

  silent = 1;
  
  LOG_STATISTICS(n_allocb, avg_alloc, n_freeb);  // LOG_STATISTICS(0L, 0L, 0L);

  int printed = 0; // part2
  for (item *it = list->next; it; it = it->next) {
    if (it->cnt > 0) {
      if (!printed) {
        LOG_NONFREED_START();
        printed = 1;
      }
      LOG_BLOCK(it->ptr, it->size, it->cnt);
    }
  }

  // 
  LOG_STOP();

  silent = 0;

  // free list (not needed for part 1)
  free_list(list);
}

// ...
void *malloc(size_t size) {
  if (silent) return mallocp(size);
  void *ptr = mallocp(size);
  n_malloc++;
  n_allocb += size;
  if (ptr) alloc(list, ptr, size); // part2
  LOG_MALLOC(size, ptr);
  return ptr;
}

void *calloc(size_t nmemb, size_t size) {
  if (silent) return callocp(nmemb, size);
  void *ptr = callocp(nmemb, size);
  n_calloc++;
  n_allocb += (unsigned long)nmemb * (unsigned long)size;
  if (ptr) alloc(list, ptr, (size_t)nmemb*(size_t)size); // part2
  LOG_CALLOC(nmemb, size, ptr);
  return ptr;
}

void *realloc(void *ptr, size_t size) {
  if (silent) return reallocp(ptr, size);

  // ptr == NULL -> malloc 동일하게 정상 경로로 처리
  if (ptr != NULL) {
    item *it = find(list, ptr);
    if (it == NULL) {
      // never allocated
      LOG_ILL_FREE();
      // 기존 블록 해제 없이 새로운 할당처럼 동작
      void *newptr = reallocp(NULL, size);
      n_realloc++;
      n_allocb += size;
      if (newptr) alloc(list, newptr, size);
      LOG_REALLOC(ptr, size, newptr);
      return newptr;
    }
    if (it->cnt == 0) {
      // already freed
      LOG_DOUBLE_FREE();
      // 기존 블록 해제 없이 새로운 할당처럼 동작
      void *newptr = reallocp(NULL, size);
      n_realloc++;
      n_allocb += size;
      if (newptr) alloc(list, newptr, size);
      LOG_REALLOC(ptr, size, newptr);
      return newptr;
    }

    // 정상일 때 old 블록을 dealloc하여 freed_total 업데이트
    it = dealloc(list, ptr);
    if (it) n_freeb += it->size;
  }

  // 실제 리얼록
  void *newptr = reallocp(ptr, size);
  n_realloc++;
  n_allocb += size;
  if (newptr) alloc(list, newptr, size);
  LOG_REALLOC(ptr, size, newptr);
  return newptr;
}


void free(void *ptr) {
  if (silent) { freep(ptr); return; }

  // 프리 시도
  LOG_FREE(ptr);
  if (ptr == NULL) return; // free(NULL)은 아무것도 안 함

  // 상태 확인
  item *it = find(list, ptr);
  if (it == NULL) {
    // 불법 해제
    LOG_ILL_FREE();
    return; // 진짜 프리 x
  }
  if (it->cnt == 0) {
    // already freed
    LOG_DOUBLE_FREE();
    return; // 진짜 프리 x
  }
  // 정상 경로일 때 dealloc로 통계 업데이트한 다음에 프리
  it = dealloc(list, ptr);
  if (it) n_freeb += it->size;
  freep(ptr);
}
