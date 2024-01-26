/* to compile 
 c++ -fuse-ld=lld -Wl,-z,max-page-size=2097152 -o mprotect -g3 mprotect.cc
*/
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
//echo 10 > /sys/kernel/mm/transparent_hugepage/khugepaged/scan_sleep_millisecs
//echo 3 > /proc/sys/vm/drop_caches
#define HPAGE_SIZE (1 << 21)

int main() {
  char ptr1[HPAGE_SIZE];
  char buf[32];
  int fd, fd1;
  int secs;
  fd1 = open("/sys/kernel/mm/transparent_hugepage/khugepaged/scan_sleep_millisecs", O_RDONLY);
  read(fd1, buf, 32);
  close(fd1);
  //Learning from https://maskray.me/blog/2023-12-17-exploring-the-section-layout-in-linker-output#transparent-huge-pages-for-mapped-files
  secs = atoi(buf) * 1000;
  fd = open("carlclone.tar.xz", O_RDONLY);
  void * ptr;
  ptr = mmap (0, HPAGE_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
  memcpy(ptr1, ptr, HPAGE_SIZE);
  mprotect(ptr, HPAGE_SIZE, PROT_READ|PROT_WRITE);
  int ret = madvise(ptr, HPAGE_SIZE, MADV_HUGEPAGE);//let's trigger khugepaged
  usleep(secs); // wait for khugepaged work
  memcpy(ptr1, ptr, HPAGE_SIZE); //let pagefault fill the pmd
  close(fd);

}
