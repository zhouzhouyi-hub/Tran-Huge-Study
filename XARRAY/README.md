# Study on Linux XArray

The XArray is an abstract data type which behaves like a very large array of pointers. It meets many of the same needs as a hash or a conventional resizable array [1].

Radix tree was previously  used in page cache for searching page caches. However, after version 4.20 of the Linux kernel, it has been replaced by the XArray structure.

## 1. Structure of XArray 

![Structure of XAarray](Overview.svg)

In include/linux/xarray.h there are macro definitions about node of XArray:
```
#define XA_CHUNK_SIZE           (1UL << XA_CHUNK_SHIFT)
#define XA_CHUNK_MASK           (XA_CHUNK_SIZE - 1)
```
In figure above, XA_CHUNK_SHIFT is defined to be 3, and XA_CHUNK_SIZE is defined to be 1<<3 = 8, and XA_CHUNK_MASK is defined to be 8 - 1 = 7. So there are 8 slots per XArray node.

## 2. Example of xas_load
![example xas_load](traverse.svg)


Above figure gives a example of XArray tree traverse:

The head node of the tree has shift 9, so when we try to load index 64th element from the tree,
we compute the offset first:
```
203	static void *xas_descend(struct xa_state *xas, struct xa_node *node)
204	{
205		unsigned int offset = get_offset(xas->xa_index, node);
206		void *entry = xa_entry(xas->xa, node, offset);
207	
208		xas->xa_node = node;
```
* The offset at the head node of our current search is computed as 0, so we got the pointer to next node we travel from slot 0
* The offset at the second level node (shift == 6) is computed as 1, so we got the pointer to next node we travel from slot 1
* The offset at the third level node (shift == 3) is computed as 0, so we got the pointer to next node we travel from slot 1 and
* The offset at the leaf node (shift == 0) is computed 0, so we got the entry from slot 0.

## 3. Multi-order Xarray
Much like her ancestor radix-tree in Linux kernel [2], Xarray has multi-order technology to
insert an entry that covers multiple indices and have operations on indices in that range.

We will visit the structure of Multi-order Xarray from easiest to hardest cases in this section

### 3.1 Index == 0 and order is multiple of XA_CHUNK_SHIFT
![multiorder 1](multi-order-1.svg)

In figure above, we store entry 1 with order 12 and index 10 to Xarray, so:
* A node with shift == 12 is allocated and is assigned as header
* Value 1 is stored to slot 1 of above node.

### 3.2 Index=4004 and order=7
![multiorder 1\label{multiorder1}](multi-order-2.svg)
<div>
    {#fig:multiorder1}
</div>


As mentioned in [2], "For orders greater than 1, there can 
simply be multiple sibling entries that all point back to 
the actual radix-tree entry".

Note in the figure @fig:{multiorder1}, 7th entry of node with shift-6 is
a sibling node that point back to 6th entry of the same node. Both 6th
and 7th entries represent indices of order 6 (1<<6), and they formed
together the index combinations of order 7.
 
## 4. Marks
In XArray data structure, each entry (see figures above) in the array has three bits associated with it called marks [3].

### 4.1 Marks' use case in Linux kernel
In Linux kernel's function mark_buffer_dirty, __folio_mark_dirty is called to mark folio's
corresponding indices in mapping dirty.
```
2666		xa_lock_irqsave(&mapping->i_pages, flags);
2667		if (folio->mapping) {	/* Race with truncate? */
2668			WARN_ON_ONCE(warn && !folio_test_uptodate(folio));
2669			folio_account_dirtied(folio, mapping);
2670			__xa_set_mark(&mapping->i_pages, folio_index(folio),
2671					PAGECACHE_TAG_DIRTY);
2672		}
2673		xa_unlock_irqrestore(&mapping->i_pages, flags);
```

### 4.2 Set mark example

![set mark1\label{set_mark1}](set_mark1.svg)

<p style="text-align: center;">{#fig:set_mark1}</p>

In figure @fig:{set_mark1}, we want to set mark: XA_MARK_0 in index 64 of XArray. Following is the mark process.

* The head of the array is a node with shift == 6, so to keep traversing, offset is computed as 64>>6&7==1.
* Entry 1 of head is a node with shift == 3, and the next offset is computed as 64>>3&7 == 0.
* Entry 0 of previous node is a node with shift = 0, and the offset is computed as 64>>0&7 == 0, so mark0[offset] is set.
* Traverse back the tree from above node to head, and set mark0[offset] of each node.

## 5. xas_split

xas_split is added in [4] to support spliting a multi-index entry (eg if a file is truncated in
the middle of a huge page entry) [4].

### 5.1 xas_split example 1

![xas split example 1\label{xassplit1}](Split-1.svg)

<p style="text-align: center;">{#fig:xassplit1}</p>

In figure @fig:{xassplit1}, the new_order for split is 1, which means each entry in the array has one sibling (1<<1 == 2). the "order" to be splited is 2, which means there are 4 (1<<2) to be splitted.

### 5.2 xas_split example 2

The second example of xas_split is from Linux kernel, when we invoke
```
echo 1 > /sys/kernel/debug/split_huge_pages
```
Linux kernel call split_huge_pages_all => split_folio => split_folio_to_list => split_huge_page_to_list => xas_split to split
a multi-index entry in memory map
```
2762				xas_split(&xas, folio, folio_order(folio));
```

![xas split example 2\label{xassplit2}](Split-2.svg)

<p style="text-align: center;">{#fig:xassplit2}</p>

Note that in Linux kernel, there 64 entries in a XArray node.

## 6. Conclusion


## 7. references
[1] https://www.kernel.org/doc/html/latest/core-api/xarray.html

[2] https://lwn.net/Articles/688130/

[3] https://docs.kernel.org/core-api/xarray.html

[4] XArray: add xas_split https://patchwork.kernel.org/project/linux-mm/patch/20201016024156.AmjHOFeMg%25akpm@linux-foundation.org/
