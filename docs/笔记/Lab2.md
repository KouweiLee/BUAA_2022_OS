# Lab2

我们所能管理的物理空间为`[0x0040_0000, 0x03ff_ffff (maxpa-1)]`，

### 宏定义总结

```
BY2PG 4096 一页的大小4KB
PDMAP 4*1024*1024 所有页表的大小4MB
ULIM 0x8000_0000 用户态最高地址
UVPT 0x7fc0_0000 用户态下的页表基地址，内含一个一级页表，共4MB大小
UPAGES 0x7f80_0000 用户态下的page结构体的基地址，共4MB大小
```

### 虚拟地址到物理地址

CPU发出虚拟地址，然后需要映射到物理地址，进行访问内存 。具体访问内存规则为：

| 虚拟地址                        | 物理地址                                                   |
| ------------------------------- | ---------------------------------------------------------- |
| 0xa0000000~0xbfffffff (kseg1)   | 最高3位置0得物理地址，**不通过**cache，映射外设。          |
| 0x8000_0000~0x9fff_ffff (kseg0) | 最高位置0得物理地址，通过 cache访存。存放内核与数据结构    |
| 0x00000000~0x7fffffff (kuseg)   | 通过TLB获取物理地址，通过cache访存，用户态只可以访问此空间 |

MOS物理内存是64MB，页大小为4KB

c语言程序会首先汇编成一条条汇编指令，但这时候地址不会进行映射，也就是说，`lw k0, 0x8000_0000(zero)`，还是会采用虚拟地址。虚拟地址到物理地址的变换是由MMU自动完成的，当MMU我们写完之后，就不用考虑映射了，MMU会自动进行映射。

### 启动流程

mips_init()函数，实现于init/init.c，按顺序调用了5个函数

1. mips_detect_memory()，初始化和内存管理有关的变量

   - `maxpa = 0x0400_0000`，物理内存的大小，物理地址的最大值 +1，即 `[0, maxpa − 1]` 范围内所有整数所组成的集合等于物理地址的集合。
   - `basemem = 64MB`，表示物理内存对应的字节数。
   - `npage = basemem / BY2PG`，表示总物理页数。

2. mips_vm_init()，调用了alloc函数

   > 问题：page结构体分配的虚拟地址空间在kseg0, UPAGES是个啥？
   >
   > page结构体本身，会占用一定的物理空间。UPAGES是pages链表的物理地址对应的虚拟地址，专门弄出来一个UPAGES是为了让用户态可以访问到page结构体。UPAGES和pages两个不同的虚拟地址都对应同一个物理地址。不过是UPAGES是通过页表对应的，pages是直接最高位抹0对应的

   ```c
   void mips_vm_init()
   {
       extern char end[];
       extern int mCONTEXT;
       extern struct Env *envs;
   
       Pde *pgdir;
       u_int n;
   
       /* Step 1: Allocate a page for page directory. */
       pgdir = alloc(BY2PG, BY2PG, 1);
       printf("to memory %x for struct page directory.\n", freemem);
       mCONTEXT = (int)pgdir;
   
       boot_pgdir = pgdir;
   
       pages = (struct Page *)alloc(npage * sizeof(struct Page), BY2PG, 1);
       //为page结构体分配物理内存，同时page结构体的虚拟地址在kseg0
       printf("to memory %x for struct Pages.\n", freemem);
       n = ROUND(npage * sizeof(struct Page), BY2PG);
       boot_map_segment(pgdir, UPAGES, n, PADDR(pages), PTE_R);
   	//将UPAGES(位于kuseg)，知道UPAGES+n的虚拟地址，映射到PADDR(pages)中。也就是说，将pages所占的物理空间，映射到内核页表boot_pgdir中，对应的虚拟地址是UPAGES
   
       envs = (struct Env *)alloc(NENV * sizeof(struct Env), BY2PG, 1);
       n = ROUND(NENV * sizeof(struct Env), BY2PG);
       boot_map_segment(pgdir, UENVS, n, PADDR(envs), PTE_R);
   
       printf("pmap.c:\t mips vm init success\n");
   }
   ```

   

   * `alloc`函数

```c
static void *alloc(u_int n, u_int align, int clear)
//分配n字节的空间，并返回一个虚拟地址（我们把他叫做初始虚拟地址）
//align可以整除这个初始虚拟地址
//clear为1则清零对应空间
{
    extern char end[];
//end[]的虚拟地址为0x80400000,对应物理地址为0x0040_0000,因此可以管理的物理地址为[0x0040_0000,0x03ff_ffff]，一旦分配虚拟地址，物理地址也被分配
    u_long alloced_mem;
//初始虚拟地址
    if (freemem == 0) {
        freemem = (u_long)end;
    }
//freemem为全局变量，表示[end,freemem-1]的虚拟地址都已经被分配了。保证多次调用alloc函数分配地址的正确不重复。

    freemem = ROUND(freemem, align);
//返回一个大于freemem，可以被align整除的地址，地址为[freemem/align]*align,[]表示向上取整，align必须是2的非负整数次幂

    alloced_mem = freemem;

    freemem = freemem + n;
//下一次要分配的起点，这次分配的为[alloced_mem, freemem-1]

    if (PADDR(freemem) >= maxpa) {
//PADDR返回虚拟地址对应的物理地址,maxpa是最大物理地址加1
        panic("out of memory\n");
        return (void *)-E_NO_MEM;
    }
    
    if (clear) {
        bzero((void *)alloced_mem, n);
    }

    return (void *)alloced_mem;
}
```

mips_vm_init()函数实现了

* 为内核一级页表分配一页页对齐的全0的物理内存，boot_pgdir为初始虚拟地址
* 为物理内存管理所用的Page结构体按页分配物理内存，满足起始地址页对齐，分配的空间映射到内核页表，起始虚拟地址为UPAGES，所有page结构体物理内存连续，pages为第一个的地址
* 为进程管理所用的Env结构体按页分配物理内存，NENV为最大进程数，也将分配的空间映射到内核页表中，对应起始虚拟地址为UENVS

3. page_init()，初始化Pages结构体以及空闲链表page_free_list。也就是说，执行这个函数以后，才可以使用page相关的函数。包括

### 物理内存管理

#### 链表宏

*小知识点补充：优先级 **.**和 **->**最高，他们从左向右做运算就行；&在他们之后。*

使用宏对链表的操作进行封装，链表为双向链表，便于节点的删除和插入，因为会涉及大量删除工作。

* `LIST_HEAD(name,type)`，创建一个`struct name`的链表头部的结构体，包含一个`struct type *lh_first`元素，指向链表的首个元素。每个链表的节点类型都是type, 包含`field`属性

```c
//include/queue.h
#define LIST_HEAD(name, type)                       
struct name {
    struct type *lh_first;  /* first element */     
}
//LIST_HEAD(HEADNAME,TYPE) head;使用时加 &
```

* `LIST_ENTRY(type)`，定义一个链表项的field，包含下一个元素的指针，以及指向*上一个元素le_next指针*的指针,这个指针就是想知道上一个list_entry的le_next是啥，也就是说，*le_prev代表的是前一个list_entry的le_next。如果是链表的第一个项le_prev指向lh_first

```c
#define LIST_ENTRY(type)             
struct { 
    struct type *le_next;   /* next element */
    struct type **le_prev;  /* address of previous
    next element */      }
//LIST_ENTRY(type) field;
```

当想要删除这个list entry时，可以这样

```c
*le_prev = le_next;
```

* 检查结构体头部head是否为空，也就是说这个链表一个元素是否没有

```c
#define LIST_EMPTY(head)        ((head)->lh_first == NULL)
```

* 获取链表的第一个元素

``` c
#define LIST_FIRST(head)        ((head)->lh_first)
```

* 将链表初始化，将第一个元素清空，使`LIST_EMPTY(head) = NULL`。之所以用`do...while(0)`包起来，是表名这是一条语句，使用后要加分号。

```c
#define LIST_INIT(head) 
do {
LIST_FIRST((head)) = NULL;
} while (0)
```

* 返回elm结构体（类型就是type）的，类型为LIST_ENTRY、名称为field的成员的le_next指向的下一个元素。即，返回elm的next。

```c
#define LIST_NEXT(elm, field)   ((elm)->field.le_next)
```

* `LIST_INSERT_AFTER(listelm, elm, field)`，将 `elm` 插到已有元素 `listelm` 之后。

```c
#define LIST_INSERT_AFTER(listelm, elm, field) 
do { 
    LIST_NEXT((elm),field) = LIST_NEXT((listelm),field); 
    if(LIST_NEXT((listelm),field)!=NULL){
        LIST_NEXT((listelm),field)->field.le_prev = &LIST_NEXT((elm),field);
    }
    LIST_NEXT((listelm),field) = elm;
    elm->field.le_prev = &LIST_NEXT((listelm),field);
} while(0)
```



* `LIST_INSERT_BEFORE(listelm, elm, field)`，将 `elm` 插到已有元素 `listelm` 之前。

```c
#define LIST_INSERT_BEFORE(listelm, elm, field) 
do {                        
(elm)->field.le_prev = (listelm)->field.le_prev; 
LIST_NEXT((elm), field) = (listelm);   
*(listelm)->field.le_prev = (elm); 
//将listelm前一项的le_next指向elm
(listelm)->field.le_prev = &LIST_NEXT((elm),field);
//将listelm的prev指向elm的le_next的地址
} while (0)
```



* `LIST_INSERT_HEAD(head, elm, field)`，将 `elm` 插到头部结构体 `head` 对应链表的头部。

```c
#define LIST_INSERT_HEAD(head, elm, field) //head is a pointer
do {                     
if ((LIST_NEXT((elm), field) = LIST_FIRST((head))) != NULL)
    LIST_FIRST((head))->field.le_prev = &LIST_NEXT((elm), field); 

LIST_FIRST((head)) = (elm); 
(elm)->field.le_prev = &LIST_FIRST((head)); 

} while (0)
    page_free_list;
LIST_INSERT_HEAD(&page_free_list, ,pp_link)
```

* `LIST_INSERT_TAIL(head, elm, field)`，将 `elm` 插到头部结构体 `head` 对应链表的尾部。

```c
#define LIST_INSERT_TAIL(head, elm, field) 
do { 
    if (LIST_FIRST((head)) != NULL) { 
        LIST_NEXT((elm), field) = LIST_FIRST((head)); 
        while (LIST_NEXT(LIST_NEXT((elm), field), field) != NULL) { 
            LIST_NEXT((elm), field) = LIST_NEXT(LIST_NEXT((elm), field), field); 
        }
        LIST_NEXT(LIST_NEXT((elm), field), field) = (elm); 
        (elm)->field.le_prev = &LIST_NEXT(LIST_NEXT((elm), field), field); 
        LIST_NEXT((elm), field) = NULL; 
    } else { 
        LIST_INSERT_HEAD((head), (elm), field); 
           } 
} while (0)
```

* `LIST_REMOVE(elm, field)`，将 `elm` 从对应链表中删除。

```c
#define LIST_REMOVE(elm, field) 
do {                                        \
    if (LIST_NEXT((elm), field) != NULL)                            \
        LIST_NEXT((elm), field)->field.le_prev =                \
        (elm)->field.le_prev;                   \
        *(elm)->field.le_prev = LIST_NEXT((elm), field);                \
   } while (0)
```

* `LIST_FOREACH`, 遍历整个链表

```c
#define LIST_FOREACH(var, head, field)//var变量为 type类型的指针
for ((var) = LIST_FIRST((head)); (var); (var) = LIST_NEXT((var), field))   
```



#### 内存控制块Page

> 问题：page和物理页面一一顺序对应，是在哪个函数实现的 呢？
>
> 下面的内存地址转换函数就实现了这个功能，它在分配内存时，就认为page和物理页面顺序对应。

巧妙的地方在于，Page就可以充当物理页面，只需要对page在空闲链表中的操作 ，就可以管理物理页面。   

* 内存地址转换函数

```c
//include/pmap.h，page映射文件
u_long page2ppn(struct Page *pp)
{
    return pp - pages;
}//将pp所对应的页号返回

u_long page2pa(struct Page *pp){
    return page2ppn(pp) << 12;
//页号右移12位，一页大小4KB，得到第几页的物理地址
}//返回page对应的物理页的物理地址

struct Page *pa2page(u_long pa){
    return &pages[pa>>12];
}//pa为所在的物理页的物理地址（不需要页对齐），所对应的page结构体

u_long page2kva(struct Page *pp){
    return KADDR(page2pa(pp));
}//返回 page对应物理页，对应的内核虚拟地址
PPN(va) va>>12
PGSHIFT=12
```

* 内存控制块就是Page结构体，共npage个。每一个内存控制块（Page结构体）管理一页的物理内存的分配。而且每个Page和物理页面一一顺序对应。所以，可以用page直接对应到物理页面，用上述宏就可以。结构体定义为

```c
//include/pmap.h
LIST_HEAD(Page_list, Page);
struct Page_list page_free_list;
/*
struct Page_list{
    struct {
        struct {
            struct Page *le_next;
            struct Page **le_prev;
        } pp_link;
        u_short pp_ref;
    }* lh_first;
    //struct Page *lh_first;
} page_free_list;
*/

typedef LIST_ENTRY(Page) Page_LIST_entry_t;
/*
typedef struct {
	struct Page* le_next;
	struct Page** le_prev;
} Page_LIST_entry_t;
*/

struct Page {
    Page_LIST_entry_t pp_link;//就是field    
    u_short pp_ref;//对应该页物理内存被引用的次数，等于有多少虚拟页映射到该物理页
};
//struct Page *pages;
```

* 空闲链表：page_free_list，这个链表的每项为Page结构体。内存控制块的插入(物理内存使用完毕，引用次数为0)、删除(该页物理内存需要被分配给一个进程)都是在空现链表的头部。
* page_init(): 初始化Pages结构体以及空闲链表

```c
void page_init(void)
{
    /* Step 1: Initialize page_free_list. */
    /* Hint: Use macro `LIST_INIT` defined in include/queue.h. */
    LIST_INIT(&page_free_list);

    /* Step 2: Align `freemem` up to multiple of BY2PG. */
    freemem =  ROUND(freemem, BY2PG);
//freemem 是mips_vm_init时所用到的地址上限
    /* Step 3: Mark all memory blow `freemem` as used(set `pp_ref`
     * filed to 1) */

    struct Page *temp = pages;
    while(page2kva(temp)<freemem){
//page和物理页面顺序对应，而分配的时候，物理页面又是顺序分出去的，所以，page2kva<freemem可以得到被分配出去的物理页面对应的虚拟地址
        temp -> pp_ref =1;
        temp ++;//now pages is only an array, not the list
    }
    while(page2ppn(temp) < npage){
        temp -> pp_ref = 0;
        LIST_INSERT_HEAD(&page_free_list,temp ,pp_link);
        temp ++;
    }
    /* Step 4: Mark the other memory as free. */
}
```



* 空现链表的分配，由`page_alloc(struct Page **p)`函数实现，将 `page_free_list` 空闲链表头部内存控制块对应的物理页面分配出去，将 `pp` 指向的空间赋值为这个内存控制块的地址。 如果空闲链表为空，报错。**注意，使用这个函数之后，要对page->pp_ref++**

```c
//mm/pmap.c
int page_alloc(struct Page **pp)
{
    struct Page *tmp;
    if (LIST_EMPTY(&page_free_list)) return -E_NO_MEM;
    // negative return value indicates exception.

    tmp = LIST_FIRST(&page_free_list);

    /* III. remove this page from the list */;
    LIST_REMOVE(tmp, pp_link);
    bzero(page2kva(tmp), BY2PG);

    *pp = tmp;
    return 0;
}
```

* bzero函数，将虚拟地址b开始，共len长度的区间`[b,b+len-1]`清零

```c
void bzero(void *b, size_t len)
{
    void *max;

    max = b + len;

    //printf("init.c:\tzero from %x to %x\n",(int)b,(int)max);

    // zero machine words while possible

    while (b + 3 < max) {
        *(int *)b = 0;
        b += 4;
    }   

    // finish remaining 0-3 bytes
    while (b < max) {
        *(char *)b++ = 0;
    }   

}
```

* `page_free(struct Page *pp)`，判断 `pp` 指向内存控制块对应的物理页面引用次数是否为 `0`，若是则该物理页面为空闲页面，将其对应的内存控制块重新插入到 `page_free_list`。
* `page_decref(struct Page *pp)`，它的作用是令 `pp` 对应内存控制块的引用次数减少 `1`，如果引用次数为 `0` 则会调用 `page_free` 函数。

### 虚拟内存管理

> 问题：这里的页表的虚拟地址都是在kuseg,假设页表基地址为p, *p需要tlb映射，但是此时还未建立tlb映射，怎么办？
>
> 一旦物理内存分配了空间，kseg中的虚拟地址其实也被分配了（因为映射关系是线性的）。因此，kseg0中也有一个页表(包括一级页表和二级页表)，而且，lab2中的页表都是指kseg0中的页表。因为此时低2G还未建立tlb的映射机制。

> 问题：指针本身的地址在哪里？
>
> 指针是c语言中的，本身地址在栈里，咱们不用管。关心指针所指向的地址就行了。比如，`Pte *p = 0x111`的意思就是指针p指向地址0x111

#### 两级页表结构

* 权限位

```c
#define PTE_V       0x0200  // Valid bit
#define PTE_R       0x0400	// means the page allows writes
```

对于之前的kseg0的虚拟地址，可以使用`PADDR`和`KADDR`宏进行虚拟地址和物理地址的转换。

对于kuseg的虚拟地址，采用两级页表结构进行管理。

```c
//include/mmu.h
#define PPN(va)     (((u_long)(va))>>12)
// page number field of address，虚拟地址的31-12位

#define PTE_ADDR(pte)   ((u_long)(pte)&~0xFFF)
//将低12位清0，获得页号<<12

#define KADDR(pa) pa + ULIM
// translates from physical address to kernel virtual address.


#define PADDR(kva) kva - ULIM
// translates from kernel virtual address in kseg0 to physical address.

##define ULIM 0x80000000
```

第一级为一级页表，即页目录（Page Directory），第二级为二级页表，即页表(Page Table)

两级页表结构为

![lab2-pgtable.svg](https://os.buaa.edu.cn/assets/courseware/v1/abd9ba016a284a6508e6a9075ef94d85/asset-v1:BUAA+B3I062270+2022_SPRING+type@asset+block/lab2-pgtable.svg)

页表项都是包括20位的物理页号，12位的权限位。一级页表项共1024个，二级页表项最多可以由 2的20次方个。使用 `Pde` 来表示一个一级页表项（*pde是指向这个一级页表项的指针，也是这个页表项的地址），用 `Pte` 来表示一个二级页表项，这两者的本质都是 `u_long`。

各级页表项的偏移量可以使用宏

```c
//include/mmu.h
#define PDX(va)     ((((u_long)(va))>>22) & 0x03FF)
//Page Directory 
#define PTX(va)     ((((u_long)(va))>>12) & 0x03FF)
//Page Table
```

注意：CPU发出的地址都是虚拟地址，所以获取到物理地址需要转换为虚拟地址再访问。一级页表中取到的二级页表地址为虚拟地址.

#### 系统启动相关函数

7个与页表相关的函数，其中，

* 带有boot前缀的函数表明**该函数是在系统启动、初始化时调用的**。启动时，就意味着，处在kseg0内核态。物理地址和虚拟地址的转化使用`PADDR`和`KADDR`即可 

- 不以 boot 为前缀的函数名表示**该函数是在用户进程的生命周期中被调用的，用于操作用户进程的页表**。

1. **启动时的二级页表检索函数**，`Pte *boot_pgdir_walk(Pde *pgdir, u_long va, int create)`，返回虚拟地址为va, 一级页表pgdir对应的二级页表项的虚拟地址(in kseg0),如果二级页表不存在，则创建一个二级页表。

   ```c
   static Pte *boot_pgdir_walk(Pde *pgdir, u_long va, int create) {
       Pde *pgdir_entry = pgdir + PDX(va); // 一级页表基地址加一级页表项偏移量，得到一个指向对应二级页表项的指针，这个指针还是虚拟地址
   
       // check whether the page table exists
       if ((*pgdir_entry & PTE_V) == 0) {
           //exists not
           if (create) {
               *pgdir_entry = PADDR(alloc(BY2PG,BY2PG,1));
               *pgdir_entry = (*pgdir_entry) | PTE_V | PTE_R;
           } else return 0; // exception
       }
       // PTE_ADDR(*pgdir_entry)是物理页号<<12，转化为虚拟地址，这个虚拟地址也是在kseg0,也就是二级页表的基地址
       return ((Pte *)(KADDR(PTE_ADDR(*pgdir_entry))) + PTX(va);
   }
   ```

   这个函数也表明，二级页表是按需分配的，因此，虚拟地址占用4MB，不代表真的就需要4MB物理内存。

2. **启动时区间地址映射函数**：`void boot_map_segment(Pde *pgdir, u_long va, u_long size, u_long pa, int perm)`，将一级页表基地址 pgdir 对应的两级页表结构做区间地址映射，将虚拟地址区间 [va, va + size − 1] 映射到物理地址区间 [pa, pa + size − 1]，因为是按页映射，要求 size 必须是页面大小的整数倍。同时为相关页表项的权限为设置为 perm。

```c
void boot_map_segment(Pde *pgdir, u_long va, u_long size, u_long pa, int perm)
{
    int i, va_temp;
    Pte *pgtable_entry;
    for (i = 0, size = ROUND(size, BY2PG); i < size; i += BY2PG) {
        pgtable_entry = boot_pgdir_walk(
            pgdir,
            va + i,
            1 
        );
        /* Step 2. fill in the page table */
        *pgtable_entry = (pa+i)
            | perm | PTE_V;
    }
}
```

3. **运行时的二级页表检索函数**，将二级页表项地址放在ppte。

```c
int pgdir_walk(Pde *pgdir, u_long va, int create, Pte **ppte)
{
    Pde *pgdir_entry = pgdir + PDX(va);
    struct Page *page;
    int ret;

    // check whether the page table exists
    if ((*pgdir_entry & PTE_V) == 0) {
        if (create) {
            if ((ret = page_alloc(&page)) < 0) return ret;
            *pgdir_entry = (page2pa(page))
                | PTE_V | PTE_R;
            page->pp_ref++;//!!!
        } else {
            *ppte = 0;//page table不存在且不create
            return 0;
        }
    }
    *ppte = ((Pte *)(KADDR(PTE_ADDR(*pgdir_entry))) + PTX(va);
	//这个地址还是在kseg0
    return 0;
}
```

4. **增加地址映射函数**，给定一级页表基地址 pgdir ，将va 这一虚拟地址映射到内存控制块 pp 对应的物理页面，并将页表项权限为设置为 perm。由于这个函数是在用户进程中被调用的，所以va是在用户态下的虚拟地址。

主要思想：每个虚拟地址，都唯一对应一个虚拟页，而一个虚拟页又唯一对应一个页表项，那么就让这个页表项中的内容填上物理地址即可。**注意，在分配的时候，不管物理页面是否被分配过了。这就使得，允许多个进程映射到同一物理地址，同一个进程不同虚拟地址映射到同一物理地址。**

```c
int page_insert(Pde *pgdir, struct Page *pp, u_long va, u_int perm)
{
    Pte *pgtable_entry;
    int ret;

    perm = perm | PTE_V;//valid

    // Step 0. check whether `va` is already mapping to `pa`。this.va can be anything
    pgdir_walk(pgdir, va, 0 /* for check */, &pgtable_entry);//pgtable_entry in kseg0
    if (pgtable_entry != 0 && (*pgtable_entry & PTE_V) != 0) {
        //va is already mapping to a pa
        // check whether `va` is mapping to frame
        if (pa2page(*pgtable_entry) != pp) {
            page_remove(pgdir, va); // unmap it!
        } else { // va is already mapping to this physical frame
            tlb_invalidate(pgdir, va);              // 之前的启动时，不需要tlb_invalidate是因为，他们都是在kseg0地址下操作的，此时未建立tlb机制。但是用户态需要修改页表项时，就tlb__invalidate
            *pgtable_entry = page2pa(pp) | perm;    // update the permission
            return 0;
        }
    }
    tlb_invalidate(pgdir, va);                      // <~~
    /* Step 1. use `pgdir_walk` to "walk" the page directory to get an page table entry */
    if ((ret = pgdir_walk(pgdir, va, 1, &pgtable_entry)) < 0)
        return ret; // exception
    /* Step 2. fill in the page table */
    *pgtable_entry = (page2pa(pp)) | perm;
    pp->pp_ref++;
    return 0;
}
```

5. `page_lookup`，返回va所在的物理页，以及该页所对应的二级页表项的位置(ppte)。

   ```c
   struct Page *page_lookup(Pde *pgdir, u_long va, Pte **ppte)
   {
       struct Page *ppage;
       Pte *pte;
   
       /* Step 1: Get the page table entry. */
       pgdir_walk(pgdir, va, 0, &pte);
   
       /* Hint: Check if the page table entry doesn't exist or is not valid. */
       if (pte == 0) {
           return 0;
       }
       if ((*pte & PTE_V) == 0) {
           return 0;    //the page is not in memory.
       }
   
       /* Step 2: Get the corresponding Page struct. */
   
       /* Hint: Use function `pa2page`, defined in include/pmap.h . */
       ppage = pa2page(*pte);
       if (ppte) { 
           *ppte = pte;
       }
   
       return ppage;
   }
   ```

6. `page_remove`，删除va虚拟地址对物理地址的映射。该物理地址所在页的引用次数减1。如果引用次数为0，则恢复物理页页面空闲

   ```c
   void page_remove(Pde *pgdir, u_long va)
   {   
       Pte *pagetable_entry;
       struct Page *ppage;
       /* Step 1: Get the page table entry, and check if the page table entry is valid. */
       ppage = page_lookup(pgdir, va, &pagetable_entry);
       if (ppage == 0) {
           return;
       }
       /* Step 2: Decrease `pp_ref` and decide if it's necessary to free this page. */
       /* Hint: When there's no virtual address mapped to this page, release it. */
       ppage->pp_ref--;
       if (ppage->pp_ref == 0) {
           page_free(ppage);
       }
       /* Step 3: Update TLB. */
       *pagetable_entry = 0; 
       tlb_invalidate(pgdir, va);
       return;
   }
   ```

使用 tlb_invalidate 函数可以实现删除特定虚拟地址的映射，每当页表被修改， 就需要调用该函数以保证下次访问该虚拟地址时诱发 TLB 重填以保证访存的正确性。(前提是这个地址位于用户态，需要用tlb进行转换)

### TLB

#### 内存管理相关CP0寄存器

| 寄存器序号 | 寄存器名         | 用途                                                         |
| ---------- | ---------------- | ------------------------------------------------------------ |
| 8          | BadVaddr         | 保存引发地址异常的虚拟地址                                   |
| 10、2      | EntryHi、EntryLo | 所有**读写** TLB 的操作都要通过这两个寄存器，详情请看下一小节 |
| 0          | Index            | Determines which TLB entry will be read/written by  appropriate instructions |
| 1          | Random           | pseudo-random value (actually a free-running counter)  used by a tlbwr to write a new TLB entry into a ‘‘randomly’’  selected location. |

每个TLB表项(`entry`)都有64位，其中高32位为`key`,低32位为`data`。EntryHi寄存器和EntryLo以对子的方式代表了一个TLB表项

1. EntryHi包括两个部分

   * Virtual Page Number(`VPN`): 虚拟页号，去除低12位的虚拟地址。
     * 当 **TLB** 缺失（CPU 发出虚拟地址，欲在 TLB 中查找物理地址但未查到）时，**EntryHi** 中的 **VPN** 自动（由硬件）填充为对应虚拟地址的虚页号。 
     * 当需要**填充**或**检索** TLB 表项时，软件需要将 VPN 段填充为对应的虚拟地址。也就是说，除了发生缺失异常不用管以外，查找或者添加都需要软件填写。
   * Address Space Identifier(`ASID`):用于区分不同的地址空间。多任务下，可能出现不同进程同时拥有全部的虚拟地址空间，这时候就需要使用ASID将他们映射到不同的物理地址。

2. EntryLo包括：

   - PFN：Physical Frame Number，去除低12位的物理地址。软件通过填写 **PFN**，随后使用 **TLB** **写指令**，才将此时的 **Key** 与 **Data**写入 **TLB** 中。 

   - N：Non-cachable。当该位置高时，后续访存将不通过 cache。 
   - D：Dirty。事实上是可写位。当该位置高时，该地址可写；否则任何写操作都将引发 TLB 异常。
   - V：Valid。如果该位为低，则任何访问该地址的操作都将引发 TLB 异常。
   - G：Global。此时ASID无效，所有进程共用一套地址空间。

#### TLB指令

MIPS中对TLB相关指令有：

- Read TLB entry at index：`tlbr`，以 Index 寄存器中的值为索引,读出 TLB 中对应的表项到 EntryHi 与 EntryLo。
- Write TLB entry at index：`tlbwi`，以 Index 寄存器中的值为索引,将此时 EntryHi 与 EntryLo 的值写到索引指定的 TLB 表项中。
- Write TLB entry selected by Random：`tlbwr`，将 EntryHi 与 EntryLo 的数据随机写到一个 TLB 表项中。
- TLB lookup：`tlbp`, 根据 EntryHi 中的 Key（包含 VPN 与 ASID），查找 TLB 中与之对应的表项，并将表项的索引存入 Index 寄存器（若未找到匹配项，则 Index 最高位被置 1）。注意，并不把data放入EntryLo中

软件只能通过CP0与TLB交互，因此软件操作流程为

1. 填写CP0寄存器
2. 使用TLB指令

#### TLB重填过程

重填过程由do_refill函数完成

```c
//lib/genex.S
NESTED(do_refill,0 , sp)
            //li    k1, '?'
            //sb    k1, 0x90000000
            .extern mCONTEXT
			//一级页表位于kseg0的基地址
1:          //j 1b
            nop
            lw      k1,mCONTEXT
            and     k1,0xfffff000//
            mfc0    k0,CP0_BADVADDR
            srl     k0,20//向右移20位
            and     k0,0xfffffffc//[1:0]位置0，因为一字4B
            addu    k0,k1//k0 = 一级页表基地址 + 一级页表偏移量 = vaddr对应的一级页表项的虚拟地址

            lw      k1,0(k0)//虚拟地址对应的物理内容取出来，给k1
            nop
            move    t0,k1
            and     t0,0x0200//t0 = PTE_V
            beqz    t0,NOPAGE
            nop
            and     k1,0xfffff000//k1 = 二级页表对应的物理页号，也是二级页表的物理基地址
            mfc0    k0,CP0_BADVADDR
            srl     k0,10
            and     k0,0xfffffffc
            and     k0,0x00000fff//得到二级页表项的偏移量
            addu        k0,k1//k0 = 二级页表项的物理地址
                
            or      k0,0x80000000// k0 = 二级页表项的虚拟地址
            lw      k1,0(k0)//k1 = 二级页表项内容 
            nop
            move    t0,k1
            and     t0,0x0200//确定是否有效
            beqz    t0,NOPAGE
            nop
            move    k0,k1
            and     k0,0x1//PTE_COW 为写时复制权限位，将在 lab4 中用到，此时将所有页表项该位视为 0 即可
            beqz    k0,NoCOW
            nop
            and     k1,0xfffffbff
NoCOW:
            mtc0    k1,CP0_ENTRYLO0//将二级页表项的内容存入EntryLo
            nop
            tlbwr    

            j       2f//1f (forward) refers to the next ‘1:’ label in the code, and ‘1b’ (back) refers to the last-met ‘1:’ label. 
            nop
NOPAGE://表项无效，调用pageout函数
//3: j 3b
nop
            mfc0    a0,CP0_BADVADDR
            lw      a1,mCONTEXT
            nop

            sw      ra,tlbra//将返回地址保存好
            jal     pageout//为a0虚拟地址分配一个物理页
    		nop
//3: j 3b
nop
            lw      ra,tlbra
            nop

            j       1b
2:          nop

            jr      ra
            nop
END(do_refill)
```

`tlb_invalidate`函数：使用 `tlb_invalidate` 函数可以实现删除特定虚拟地址在tlb中的映射。每当页表被修改， 就需要调用该函数以保证下次访问该虚拟地址时诱发 TLB 重填以保证访存的正确性。

```c
void tlb_invalidate(Pde *pgdir, u_long va)
//给定一级页表基地址，以及该虚拟地址
{
    if (curenv) {//给定当前的进程id？
        tlb_out(PTE_ADDR(va) | GET_ENV_ASID(curenv->env_id));
    } else {
        tlb_out(PTE_ADDR(va));//将va低地址12位清0，得到VPN
    }
}
//PTE_ADDR(va) va&~0xFFF
```

`pageout`函数：被动地址映射函数，为va随便映射一个物理物理页面

```c
//mm/pmap.c
void pageout(int va, int context)
//context是一级页表在kseg0中的虚拟地址，va是引发异常的虚拟地址
{
    u_long r;
    struct Page *p = NULL;

    if (context < 0x80000000) {
        panic("tlb refill and alloc error!");// when panic, there is an infinite loop
    }//page dir虚拟地址不正确

    if ((va > 0x7f400000) && (va < 0x7f800000)) {
        panic(">>>>>>>>>>>>>>>>>>>>>>it's env's zone");
    }//引发异常的虚拟地址过高

    if (va < 0x10000) {
        panic("^^^^^^TOO LOW^^^^^^^^^");
    }//引发异常的虚拟地址过低

    if ((r = page_alloc(&p)) < 0) {
        panic ("page alloc error!");
    }//为p分配一页物理页

    p->pp_ref++;
	//p将被映射到va，故引用次数加1
    page_insert((Pde *)context, p, VA2PFN(va), PTE_R);
    //将va与p对应的物理页面建立映射关系
    printf("pageout:\t@@@___0x%x___@@@  ins a page \n", va);
}
//VA2PFN(va) va&0xFFFFF000，得到virtual page number
```

`tlb_out`函数（详见T6）：在`mm/tlb_asm.S`中定义，该函数根据传入的参数（TLB 的 Key）找到对应的 TLB 表项，并将其key和data都清空为0。

```c
LEAF(tlb_out)
//定义叶子函数
//1: j 1b
nop
    mfc0    k1,CP0_ENTRYHI
    //将目前EntryHi寄存器的值保存到k1寄存器，以便函数结束时恢复原先EntryHi中的内容
    mtc0    a0,CP0_ENTRYHI
    // a0 is the key, moved to EntryHi
    nop
    tlbp
    // 根据EntryHi的key值，将对应的tlb表项的索引保存到Index寄存器中
    nop
    nop
    nop
    nop
    mfc0    k0,CP0_INDEX
    //将要清空的tlb表项的索引保存到k0
    bltz    k0,NOFOUND
    //如果k0 < 0, 说明tlb中没有key对应的表项 
    nop
    mtc0    zero,CP0_ENTRYHI
    //将EntryHi置0
    mtc0    zero,CP0_ENTRYLO0
    //将EntryLo置0
    nop
    tlbwi
    // 根据Index的索引，将该tlb表项的key和data置0
NOFOUND:

    mtc0    k1,CP0_ENTRYHI 
	//恢复调用函数前的EntryHi的值
    j   ra
	//返回函数
    nop
END(tlb_out)
//结束函数定义
```



### 多级页表与页目录自映射

MOS 中，将页表和页目录映射到了用户空间中的 0x7fc00000-0x80000000（共4MB）区域，这意味着 MOS 中允许在用户态下访问当前进程的页表和页目录

构建自映射方法如下：

1. 给定页表基址PT~base~ , 基址需要4MB对齐，即低22位都为0
2. 页目录表的基址PD~base~ = PT~base~ | (PT~base~ >> 10)
3. 自映射目录表项地址为PDE~self-mapping~ = PT~base~ | (PT~base~ >> 10) | (PT~base~ >> 20)

对2和3的说明：

2. 由于每个虚拟页占空间4KB(12位)，故PT~base~ >> 12 为虚拟地址PT~base~ 是第几个虚页。由于页表项和虚页顺序一一对应，PT~base~ >> 12也就是对应第几个页表项。页表项占空间4B，因此(PT~base~ >> 12) * 4表示PT~base~ 对应的页表项相对于PT~base~ 的offset, PD~base~ 的第一个页目录项pde就对应着PT~base~ ， 因此，PD~base~ = PT~base~ + PT~base~ >> 10

3. PD~base~ >> 12表示其对应第几个页表项，PDE~self-mapping~ 就是指向PD~base~的页表项，

   $PDE_{self-mapping} = PT_{base} + (PD_{base} >> 12 ) * 4$

补充：对于自映射，我们说PT~base~ 的低22位为0。但同时，由于整个页表连续分布在4MB的区域内，故1M个页表项地址的高10位都相同。虚拟地址的前10位仍然是页目录项的索引，中间10位仍是二级页表项的索引。虚拟地址的前10位 也是自映射页目录表项是第几个 页目录项。

自映射，只需要把页目录中对应的表项填上自己就可以了。只要有4M空闲的虚拟地址空间就做自映射，用户或内核空间都可以。