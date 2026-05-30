#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>

#define DBFS_FILE_PATH  "/sys/kernel/debug/paddr/output"
#define PADDR   0xff000

struct packet {
        pid_t pid;
        unsigned long vaddr;
        unsigned long paddr;
};

int main(void)
{
        int fd, mem, ret;
        void *vaddr;
        struct packet pckt;

        // 물리주소 PADDR 페이지를 내 주소공간에 매핑 (root 필요)
        mem = open("/dev/mem", O_RDWR);
        if (mem < 0) {
                perror("open /dev/mem");
                return 1;
        }

        vaddr = mmap(NULL, sizeof(unsigned long), PROT_READ | PROT_WRITE, MAP_SHARED, mem, PADDR);
        if (vaddr == MAP_FAILED) {
                perror("mmap");
                close(mem);
                return 1;
        }

        pckt.pid = getpid();
        pckt.vaddr = (unsigned long)vaddr;
        pckt.paddr = 0;

        fd = open(DBFS_FILE_PATH, O_RDONLY);
        if (fd < 0) {
                perror("open " DBFS_FILE_PATH);
                return 1;
        }

        ret = read(fd, &pckt, sizeof(struct packet));
        if (ret < 0) {
                perror("read");
                return 1;
        }

        printf("vaddr 0x%lx -> paddr 0x%lx\n", pckt.vaddr, pckt.paddr);
        if (pckt.paddr == PADDR)
                printf("[TEST CASE]    PASS\n");
        else
                printf("[TEST CASE]    FAIL (expected 0x%x)\n", PADDR);

        close(fd);
        munmap(vaddr, sizeof(unsigned long));
        close(mem);

        return pckt.paddr == PADDR ? 0 : 1;
}
