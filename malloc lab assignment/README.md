# malloc lab

| 파일 | 프리 블록 관리 | 탐색 |
|---|---|---|
| implicit/mm_1.c | implicit list | first fit |
| implicit/mm_2.c | implicit list | next fit |
| implicit/mm_3.c | implicit list | best fit |
| implicit/mm_4.c | implicit list | worst fit |
| explicit/mm_5.c | explicit list (LIFO) | first fit |
| explicit/mm_6.c | explicit list (LIFO) | best fit |
| explicit/mm_7.c | explicit list (LIFO) | worst fit |
| segregated/mm_8.c | segregated list (크기 2^i 구간 20개) | first fit |
| segregated/mm_9.c | segregated list | best fit |
| segregated/mm_10.c | segregated list | worst fit |

- 공통: header/footer 4B, 8B 정렬, 최소 블록 16B, free 할 때 바로 coalesce
- realloc 은 줄이거나 뒤 프리 블록(힙 끝이면 힙 확장)을 붙여서 되면 제자리에서 처리
- explicit / segregated 는 프리 블록 링크를 포인터 대신 힙 시작 기준 4B 오프셋으로 저장해서 -m32 없이 64비트에서도 돌아감
- implicit 폴더의 확장자 없는 1 ~ 4 는 예전에 짜던 버전이라 그대로 둠

사용할 때는 원하는 파일을 handout 의 `mm.c` 로 복사해서 `make` 후 `./mdriver -V`
