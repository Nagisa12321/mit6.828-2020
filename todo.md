1. modify the fork. COW fork create just a pagetable for the children. and then make the parent/children page table not writeable. When write on the parent's or the children's pagetab will cause a page fault.

2. the kernel page-fault handler will alloc a pagetable and copy the physic pagetable to it.And then map it, then make both two pagetable writeable. 

3. should user a 'counter' while freeing the pagetable. Because the pagetable may be referred by multiple process.  

- uvmcopy():
		1. map the parent's page to the child(not alloc);
		2. clear the PTE_W both parent and children
- usertrap(): deal with the pagefault
        1. kalloc to alloc a free page, and copy.
        2. make the both page PTE_W set.