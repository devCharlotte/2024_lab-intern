# kernellab

debugfs 로 커널 정보를 꺼내 보는 과제 두 개

- `ptree/` : input 에 pid 를 쓰면 init 부터 그 프로세스까지 부모 체인을 출력
- `paddr/` : 프로세스의 가상 주소를 page table 따라가서 물리 주소로 변환

## ptree

```
cd ptree
make
make insmod
echo $$ | sudo tee /sys/kernel/debug/ptree/input
sudo cat /sys/kernel/debug/ptree/ptree
make rmmod
```

## paddr

```
cd paddr
make
make insmod
sudo ./app      # [TEST CASE]    PASS 나오면 성공
make rmmod
```

app 은 `/dev/mem` 의 물리주소 0xff000 을 mmap 해서 확인하기 때문에 root 로 실행해야 함
