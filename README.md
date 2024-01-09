# Tran-Huge-Study

Transparent huge page study following Maskray's guide.
When reading Maskray's post [1]

I compiled Masyray's program, and modified kernel according to [2], I found the example program is not stable, i.e sometime the program success, and sometime the program failed with either huge page not allocated or the page does not exits.

## Modify Linux kernel
I modified Linux kernel according to [2].
```
 include/linux/huge_mm.h | 1 -
 1 file changed, 1 deletion(-)
diff --git a/include/linux/huge_mm.h b/include/linux/huge_mm.h
index fa0350b0812a..4c9e67e9000f 100644
--- a/include/linux/huge_mm.h
+++ b/include/linux/huge_mm.h
@@ -126,7 +126,6 @@ static inline bool file_thp_enabled(struct vm_area_struct *vma)
 	inode = vma->vm_file->f_inode;
 
 	return (IS_ENABLED(CONFIG_READ_ONLY_THP_FOR_FS)) &&
-	       (vma->vm_flags & VM_EXEC) &&
 	       !inode_is_open_for_write(inode) && S_ISREG(inode->i_mode);
 }
```
## Debug Linux kernel
I set breakpoint in add_to_pagemap, and kernel will stop because of test.cc's
```
check_page(__ehdr_start); //A: enable us to trace walk_pmd_range
```
The kernel stop at breakpoint, then I do a backtrace in gdb:
```
#1  0xffffffff814c9236 in pagemap_pmd_range (pmdp=0xffff888135df3550, 
    addr=93824990838784, end=93824990842880, walk=0xffffc90001e4fd18)
    at fs/proc/task_mmu.c:1552
1552			err = add_to_pagemap(addr, &pme, pm);
(gdb) f 2
#2  0xffffffff8139a857 in walk_pmd_range (pud=0xffff888135ddbaa8, 
    pud=0xffff888135ddbaa8, walk=0xffffc90001e4fd18, end=93824990842880, 
    addr=93824990838784) at mm/pagewalk.c:143
143				err = ops->pmd_entry(pmd, addr, next, walk);
(gdb) p pmd
$1 = (pmd_t *) 0xffff888135df3550
```
Then I set watchpoint for (pmd_t *) 0xffff888135df3550
After while the kernel will stop at:
### pmdp_collapse_flush
Following is the backtrace:
```
#0  0xffffffff8139b991 in pmdp_collapse_flush ()
    at ./arch/x86/include/asm/pgtable.h:233
#1  0xffffffff8140caaa in retract_page_tables (mapping=0xffff888010c1f4e0, 
    pgoff=0) at mm/khugepaged.c:1745
#2  0xffffffff8140df08 in collapse_file (mm=0xffff888100dfc0c0, 
    addr=93824990838784, file=0xffff8880049d5600, start=0, 
    cc=0xffffffff82b5de40 <khugepaged_collapse_control>)
    at mm/khugepaged.c:2158
#3  0xffffffff8140e936 in hpage_collapse_scan_file (mm=0xffff888100dfc0c0, 
    addr=93824990838784, file=0xffff8880049d5600, start=0, 
    cc=0xffffffff82b5de40 <khugepaged_collapse_control>)
    at mm/khugepaged.c:2307
#4  0xffffffff8140ed02 in khugepaged_scan_mm_slot (pages=4096, 
    result=0xffffc9000019fe80, 
    cc=0xffffffff82b5de40 <khugepaged_collapse_control>)
    at mm/khugepaged.c:2403
#5  0xffffffff8140f0b9 in khugepaged_do_scan (
    cc=0xffffffff82b5de40 <khugepaged_collapse_control>)
    at mm/khugepaged.c:2506
#6  0xffffffff8140f427 in khugepaged (none=0x0 <fixed_percpu_data>)
    at mm/khugepaged.c:2562
#7  0xffffffff811514d1 in kthread (_create=0xffff88810089cd40)

```
Above code set the pmd entry to empty, and create a new kernel page and assign it to the file map:
```
1789 static int collapse_file(struct mm_struct *mm, unsigned long addr,
1790                          struct file *file, pgoff_t start,
1791                          struct collapse_control *cc)
...
2149         xas_set_order(&xas, start, HPAGE_PMD_ORDER);
2150         xas_store(&xas, hpage);
```
### let pagefault fill the pmd entry
In example program test.cc, we have 
```
memcpy(ptr, __ehdr_start, HPAGE_SIZE);  //C: let pagefault to fill in the pmd entry 
```
Then the kernel will fill the pmd with huge page:
```
#0  do_set_pmd (vmf=vmf@entry=0xffffc90001e4fe18, page=<optimized out>) at mm/memory.c:4307
#1  0xffffffff81339791 in filemap_map_pmd (start=0, folio=0xffffea0004e68000, vmf=0xffffc90001e4fe18) at mm/filemap.c:3417
#2  filemap_map_pages (vmf=0xffffc90001e4fe18, start_pgoff=0, end_pgoff=15) at mm/filemap.c:3551
#3  0xffffffff81385a59 in do_fault_around (vmf=0xffffc90001e4fe18) at mm/memory.c:4525
#4  do_read_fault (vmf=0xffffc90001e4fe18) at mm/memory.c:4558
#5  do_fault (vmf=0xffffc90001e4fe18) at mm/memory.c:4705
#6  do_pte_missing (vmf=0xffffc90001e4fe18) at mm/memory.c:3669
#7  handle_pte_fault (vmf=0xffffc90001e4fe18) at mm/memory.c:4978
#8  0xffffffff8137e1be in __handle_mm_fault (vma=vma@entry=0xffff888119f8dbb0, address=address@entry=93824990838784, 
    flags=flags@entry=4692) at mm/memory.c:5127
#9  0xffffffff813864f4 in handle_mm_fault (vma=vma@entry=0xffff888119f8dbb0, address=address@entry=93824990838784, 
    flags=<optimized out>, regs=regs@entry=0xffffc90001e4ff58) at mm/memory.c:5293
#10 0xffffffff81101389 in do_user_addr_fault (regs=0xffffc90001e4ff58, error_code=4, address=93824990838784)
    at arch/x86/mm/fault.c:1364
#11 0xffffffff820274ae in handle_page_fault (address=93824990838784, error_code=4, regs=0xffffc90001e4ff58)
    at arch/x86/mm/fault.c:1505
#12 exc_page_fault (regs=0xffffc90001e4ff58, error_code=4) at arch/x86/mm/fault.c:1561
#13 0xffffffff82201266 in asm_exc_page_fault () at ./arch/x86/include/asm/idtentry.h:570
```
 
A very fruitful journal, Thank Maskray's guidance!

## references
[1] https://maskray.me/blog/2023-12-17-exploring-the-section-layout-in-linker-output#transparent-huge-pages-for-mapped-files

[2] https://lore.kernel.org/linux-mm/ZYSXH58aQpI1SLr2@casper.infradead.org/T/
