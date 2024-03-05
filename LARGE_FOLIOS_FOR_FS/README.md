# Study on large folio support for file systems

In the kernel email thread "PATCH: mm: remove VM_EXEC requirement for THP eligibility" [1], I
learned that CONFIG_READ_ONLY_THP_FOR_FS is a preliminary hack which solves some
problems,  the real solution is using large folios, which at the moment supported only on XFS and AFS.

## XFS's using of large folios

As mentioned in [1], XFS and AFS support large folio, and support of large folio of XFS is done in [2]:
```
diff --git a/fs/xfs/xfs_icache.c b/fs/xfs/xfs_icache.c
index da4af2142a2b..cdc39f576ca1 100644
--- a/fs/xfs/xfs_icache.c
+++ b/fs/xfs/xfs_icache.c
@@ -87,6 +87,7 @@ xfs_inode_alloc(
        /* VFS doesn't initialise i_mode or i_state! */
        VFS_I(ip)->i_mode = 0;
        VFS_I(ip)->i_state = 0;
+       mapping_set_large_folios(VFS_I(ip)->i_mapping);
 
        XFS_STATS_INC(mp, vn_active);
        ASSERT(atomic_read(&ip->i_pincount) == 0);
@@ -320,6 +321,7 @@ xfs_reinit_inode(
        inode->i_rdev = dev;
        inode->i_uid = uid;
        inode->i_gid = gid;
+       mapping_set_large_folios(inode->i_mapping);
        return error;
 }
```

By debugging Linux kernel, in function page_cache_ra_order, we have:
```
496		if (!mapping_large_folio_support(mapping) || ra->size < 4)
497			goto fallback;
```

## Conclusion


## references
[1] https://lore.kernel.org/linux-mm/ZYSXH58aQpI1SLr2@casper.infradead.org/T/#m1d0f6c347e5729ca069da9b6f1fb7247b898e7ad
[2] 6795801366da("xfs: Support large folios")