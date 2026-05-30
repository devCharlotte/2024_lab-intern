# linkerlab

LD_PRELOAD 로 malloc / calloc / realloc / free 를 가로채서 호출을 기록하는 라이브러리

- part1 : 호출 로그 + 할당 통계
- part2 : 블록별로 추적해서 freed_total 계산, 끝날 때 해제 안 된 블록 출력
- part3 : 잘못된 free (ILLEGAL_FREE, DOUBLE_FREE) 는 로그만 남기고 무시

handout 의 utils (memlog, memlist) 랑 test 프로그램은 안 올림
handout 디렉토리에 memtrace.c 넣고 각 part 에서

```
make compile
make run test1
```
