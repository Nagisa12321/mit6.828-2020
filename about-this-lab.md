## A kernel page table per process
- modify the kernel so that every process uses its own copy of the kernel page table when executing in the kernel
- when alloc proc, I should create a kernel_pagetable too.
```c
    p->kernel_pagetable = proc_kernel_pagetable(p);
    if(p->kernel_pagetable == 0){
        freeproc(p);
        release(&p->lock);
        return 0;
    }
```
- create a empty page table .
```c
pagetable_t proc_kernel_pagetable(struct proc *p) {
    pagetable_t pagetable = uvmcreate();
    if (pagetable == 0) {
        return 0;
    }

    return pagetable;
}
```

- and I should make a kernel stack when alloc process. 
```c
void 
allockstack(struct proc *p) {
    // Allocate a page for the process's kernel stack.
    // Map it high in memory, followed by an invalid
    // guard page.
    char *pa = kalloc();
    if(pa == 0)
        panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    p->kstack = va;
    // copy the stack in kernel
    uvmmap(p->kernel_pagetable, va, (uint64) pa, PGSIZE, PTE_R | PTE_W);
}
```

- In the scheduler, I should change the HW to use each process's page table, when there is no proc, then use the kernel_pagetable. 

```c
void
uvminithart(pagetable_t pagetable) {
  w_satp(MAKE_SATP(pagetable));
  sfence_vma();
}

```

- how to free the kernel page table ? 
    - just 'walk' and free, but don't free the pa. 
    - It means free every level of the page table .