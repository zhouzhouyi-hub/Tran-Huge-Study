# Study on large folio support for file systems

In the kernel email thread "PATCH: mm: remove VM_EXEC requirement for THP eligibility" [1], I
learned that CONFIG_READ_ONLY_THP_FOR_FS is a preliminary hack which solves some
problems,  the real solution is using large folios, which at the moment supported only on XFS and AFS.



## Conclusion


## references
[1] https://lore.kernel.org/linux-mm/ZYSXH58aQpI1SLr2@casper.infradead.org/T/#m1d0f6c347e5729ca069da9b6f1fb7247b898e7ad
