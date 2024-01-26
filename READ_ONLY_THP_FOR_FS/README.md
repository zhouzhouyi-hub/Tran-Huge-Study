# How mprotect affect CONFIG_READ_ONLY_THP_FOR_FS

In mm/Kconfig, the entry for READ_ONLY_THP_FOR_FS says "Allow khugepaged to put read-only file-backed pages in THP.",
and "This is marked experimental because it is a new feature. Write support of file THPs will be developed in the next
few release cycles."

We has been doing experiments around transparent huge page mechanisms with file-backed pages [1][2]. During the madvise
the kernel calls do_madvise => madvise_walk_vmas => madvise_vma_behavior => hugepage_madvise => khugepaged_enter_vma => 
... => file_thp_enabled (the calling route from do_madvise to file_thp_enabled changed very quickly during the Linux kernel
developments, so do file_thp_enabled as follows):
```
199	static inline bool file_thp_enabled(struct vm_area_struct *vma)
200	{
201		struct inode *inode;
202	
203		if (!vma->vm_file)
204			return false;
205	
206		inode = vma->vm_file->f_inode;
207	
208		return (IS_ENABLED(CONFIG_READ_ONLY_THP_FOR_FS)) &&
209		       !inode_is_open_for_write(inode) && S_ISREG(inode->i_mode);
210	}

```

Then question arises, we see !inode_is_open_for_write(inode) on line 209 above, could a subsequent call to syscall mprotect argumented with PROT_WRITE defeat CONFIG_READ_ONLY_THP_FOR_FS and inode_is_open_for_write's functionality as a program goes on?

The rest of the paper is organized as follows, section 1 "inode_is_open_for_write" introduce how function inode_is_open_for_write works and how the operating system affects the return value of this function.

## inode_is_open_for_write
function inode_is_open_for_write is defined in include/linux/fs.h
```
static inline bool inode_is_open_for_write(const struct inode *inode)
{
       return atomic_read(&inode->i_writecount) > 0;
}
```
the field "i_writecount" of struct inode is used to judge wether inode is opened for write, as we can see in fs/open.c
```
873 static int do_dentry_open(struct file *f,
874                           struct inode *inode,
875                           int (*open)(struct inode *, struct file *))
...
892         if ((f->f_mode & (FMODE_READ | FMODE_WRITE)) == FMODE_READ) {
893                 i_readcount_inc(inode);
894         } else if (f->f_mode & FMODE_WRITE && !special_file(inode->i_mode)) {
895                 error = get_write_access(inode);
```
and get_write_access is used to increase the i_writecount field of inode.

## mprotect
In example program [mprotect.cc](https://github.com/zhouzhouyi-hub/Tran-Huge-Study/blob/main/READ_ONLY_THP_FOR_FS/mprotect.cc),
mprotect syscall is invoked to make the memory region writable after a file is opened and mmaped for read:
```
fd = open("carlclone.tar.xz", O_RDONLY);
...
ptr = mmap (0, HPAGE_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
...
mprotect(ptr, HPAGE_SIZE, PROT_READ|PROT_WRITE);
```
What mprotect affect the memory region? To answer that question, I do a debug.

syscall mprotect does its job by change relevant vma->vm_flags 
and vma->vm_page_prot

__x64_sys_mprotect=>__se_sys_mprotect=>__do_sys_mprotect=>do_mprotect_pkey=>mprotect_fixup
```
vm_flags_reset(vma, newflags);
```
__x64_sys_mprotect=>__se_sys_mprotect=>__do_sys_mprotect=>do_mprotect_pkey=>mprotect_fixup=>vma_set_page_prot
```
90	void vma_set_page_prot(struct vm_area_struct *vma)
91	{
92		unsigned long vm_flags = vma->vm_flags;
93		pgprot_t vm_page_prot;
94	
95		vm_page_prot = vm_pgprot_modify(vma->vm_page_prot, vm_flags);
96		if (vma_wants_writenotify(vma, vm_page_prot)) {
97			vm_flags &= ~VM_SHARED;
98			vm_page_prot = vm_pgprot_modify(vm_page_prot, vm_flags);
99		}
100		/* remove_protection_ptes reads vma->vm_page_prot without mmap_lock */
101		WRITE_ONCE(vma->vm_page_prot, vm_page_prot);
102	}
```
and page table's protection are also changed:
__x64_sys_mprotect=>__se_sys_mprotect=>__do_sys_mprotect=>do_mprotect_pkey=>mprotect_fixup=>change_protection=>change_protection_range=>...=>change_pte_range

## references
[1] https://maskray.me/blog/2023-12-17-exploring-the-section-layout-in-linker-output#transparent-huge-pages-for-mapped-files

[2] https://github.com/zhouzhouyi-hub/Tran-Huge-Study/blob/main/README.md
