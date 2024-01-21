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

## references
[1] https://maskray.me/blog/2023-12-17-exploring-the-section-layout-in-linker-output#transparent-huge-pages-for-mapped-files

[2] https://github.com/zhouzhouyi-hub/Tran-Huge-Study/blob/main/README.md
