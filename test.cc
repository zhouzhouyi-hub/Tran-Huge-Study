#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/kernel-page-flags.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

// Adapted from https://mazzo.li/posts/check-huge-page.html
// normal page, 4KiB
#define PAGE_SIZE (1 << 12)
// huge page, 2MiB
#define HPAGE_SIZE (1 << 21)

// See <https://www.kernel.org/doc/Documentation/vm/pagemap.txt> for
// format which these bitmasks refer to
#define PAGEMAP_PRESENT(ent) (((ent) & (1ull << 63)) != 0)
#define PAGEMAP_PFN(ent) ((ent) & ((1ull << 55) - 1))

extern char __ehdr_start[];
__attribute__((used)) const char pad[HPAGE_SIZE] = {};

// Checks if the page pointed at by `ptr` is huge. Assumes that `ptr` has
// already been allocated.

static void check_huge_page(void *ptr) {
  if (getuid())
    return warnx("not root; skip KPF_THP check");
  int pagemap_fd = open("/proc/self/pagemap", O_RDONLY);
  if (pagemap_fd < 0)
    errx(1, "could not open /proc/self/pagemap: %s", strerror(errno));
  int kpageflags_fd = open("/proc/kpageflags", O_RDONLY);
  if (kpageflags_fd < 0)
    errx(1, "could not open /proc/kpageflags: %s", strerror(errno));

  // each entry is 8 bytes long
  uint64_t ent;
  if (pread(pagemap_fd, &ent, sizeof(ent), ((uintptr_t)ptr) / PAGE_SIZE * 8) != sizeof(ent))
    errx(1, "could not read from pagemap\n");

  if (!PAGEMAP_PRESENT(ent)) {
    printf("ent = %x\n", ent);
    errx(1, "page not present in /proc/self/pagemap, did you allocate it?\n");
  } else
    printf("ent = %x pfn = %x\n", ent, PAGEMAP_PFN(ent));
  if (!PAGEMAP_PFN(ent)) {
    printf("ent = %x\n", ent);
    errx(1, "page frame number not present, run this program as root\n");
  }

  uint64_t flags;
  if (pread(kpageflags_fd, &flags, sizeof(flags), PAGEMAP_PFN(ent) << 3) != sizeof(flags))
    errx(1, "could not read from kpageflags\n");
  if (!(flags & (1ull << KPF_THP)))
    errx(1, "could not allocate huge page\n");
  if (close(pagemap_fd) < 0)
    errx(1, "could not close /proc/self/pagemap: %s", strerror(errno));
  if (close(kpageflags_fd) < 0)
    errx(1, "could not close /proc/kpageflags: %s", strerror(errno));
}
static void check_page(void *ptr) {
   uint64_t ent;
   int pagemap_fd = open("/proc/self/pagemap", O_RDONLY);
   if (pread(pagemap_fd, &ent, sizeof(ent), ((uintptr_t)ptr) / PAGE_SIZE * 8) != sizeof(ent))
     return;
}
int main() {
  char ptr[HPAGE_SIZE];
  printf("__ehdr_start: %p\n", __ehdr_start);
  check_page(__ehdr_start); //A: enable us to trace walk_pmd_range
  int ret = madvise(__ehdr_start, HPAGE_SIZE, MADV_HUGEPAGE);
  if (ret)
    err(1, "madvise");
  
  sleep(10); //B: let khugepaged a change to scan memory
  memcpy(ptr, __ehdr_start, HPAGE_SIZE);  //C: let pagefault to fill in the pmd entry
  check_huge_page(__ehdr_start);
  
  size_t size = HPAGE_SIZE;
  char *buf = (char *)malloc(size);
  
  int fd = open("/proc/self/maps", O_RDONLY);
  printf("fd = %d\n", fd);
  read(fd, buf, HPAGE_SIZE);
  write(STDOUT_FILENO, buf, strstr(buf, "[heap]\n") - buf + 7);
}
