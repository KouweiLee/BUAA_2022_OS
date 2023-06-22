# Lab3

## 实验内容

* 地址空间布局

```c
 o     4G ----------->  +----------------------------+------------0x100000000
 o                      |       ...                  |  kseg3
 o                      +----------------------------+------------0xe000 0000
 o                      |       ...                  |  kseg2
 o                      +----------------------------+------------0xc000 0000
 o                      |   Interrupts & Exception   |  kseg1
 o                      +----------------------------+------------0xa000 0000
 o                      |      Invalid memory        |   /|\
 o                      +----------------------------+----|-------Physics Memory Max
 o                      |       ...                  |  kseg0
 o  VPT,KSTACKTOP-----> +----------------------------+----|-------0x8040 0000----end
 o                      |       Kernel Stack         |    | KSTKSIZE            /|\
 o                      +----------------------------+----|------                |
 o                      |       Kernel Text          |    |                    PDMAP
 o      KERNBASE -----> +----------------------------+----|-------0x8001 0000    | 
 o                      |   Interrupts & Exception   |   \|/                    \|/
 o      ULIM     -----> +----------------------------+------------0x8000 0000----   
 o                      |         User VPT           |     PDMAP                /|\ 
 o      UVPT     -----> +----------------------------+------------0x7fc0 0000    |
 o                      |         PAGES              |     PDMAP                 |
 o      UPAGES   -----> +----------------------------+------------0x7f80 0000    |
 o                      |         ENVS               |     PDMAP                 |
 o  UTOP,UENVS   -----> +----------------------------+------------0x7f40 0000    |
 o  UXSTACKTOP -/       |     user exception stack   |     BY2PG                 |
 o                      +----------------------------+------------0x7f3f f000    |
 o                      |       Invalid memory       |     BY2PG                 |
 o      USTACKTOP ----> +----------------------------+------------0x7f3f e000    |
 o                      |     normal user stack      |     BY2PG                 |
 o                      +----------------------------+------------0x7f3f d000    |
 a                      |                            |                           |
 a                      ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~                           |
 a                      .                            .                           |
 a                      .                            .                         kuseg
 a                      .                            .                           |
 a                      |~~~~~~~~~~~~~~~~~~~~~~~~~~~~|                           |
 a                      |                            |                           |
 o       UTEXT   -----> +----------------------------+                           |
 o                      |                            |     2 * PDMAP            \|/
 a     0 ------------>  +----------------------------+ -----------------------------
 o
```

### 工具

#### 函数

```c
void bcopy(const void *src, void *dst, size_t len)
```

#### 链表宏

```
LIST_HEAD(HEADNAME,TYPE) head;//使用时加&
LIST_ENTRY(type) field;
LIST_FIRST(head) 
LIST_INIT(head) 
LIST_NEXT(elm, field)
LIST_INSERT_AFTER(listelm, elm, field)
LIST_INSERT_HEAD(head, elm, field)
LIST_REMOVE(elm, field)
LIST_FOREACH(var, head, field)// (var) = LIST_FIRST((head))
```

#### 宏定义

```c
#define LOG2NENV    10
#define NENV        (1<<LOG2NENV)//1024，ENV结构体的数量
#define ENVX(envid) ((envid) & (NENV - 1))//envid对应的env结构体在envs中的位置
#define GET_ENV_ASID(envid) (((envid)>> 11)<<6)
TIMESTACK 0x8200_0000
```

```c
//include/mmu.h
#define BY2PG       4096        // bytes to a page
#define PDMAP       (4*1024*1024)   // bytes mapped by a page directory entry
#define PGSHIFT     12
#define PDSHIFT     22      // log2(PDMAP)
#define PDX(va)     ((((u_long)(va))>>22) & 0x03FF)
#define PTX(va)     ((((u_long)(va))>>12) & 0x03FF)
#define PTE_ADDR(pte)   ((u_long)(pte)&~0xFFF)

// page number field of address
#define PPN(va)     (((u_long)(va))>>12)
#define VPN(va)     PPN(va)

#define VA2PFN(va)      (((u_long)(va)) & 0xFFFFF000 ) // va 2 PFN for EntryLo0/1
#define PTE2PT      1024
//$#define VA2PDE(va)       (((u_long)(va)) & 0xFFC00000 ) // for context

/* Page Table/Directory Entry flags
 *   these are defined by the hardware
 */
#define PTE_G       0x0100  // Global bit
#define PTE_V       0x0200  // Valid bit
#define PTE_R       0x0400  // Dirty bit ,'0' means only read ,otherwise make interrupt
#define PTE_D       0x0002  // fileSystem Cached is dirty
#define PTE_COW     0x0001  // Copy On Write
#define PTE_UC      0x0800  // unCached
#define PTE_LIBRARY     0x0004  // share memmory
```



### 进程控制块

进程就是执行中的程序，拥有独立的代码段、数据段、堆栈

进程控制块：**PCB是系统感知进程存在的唯一标志。进程与 PCB 是一一对应的**。

```c
struct Env {
     struct Trapframe env_tf;      // Saved registers发生进程调度或陷入内核时保存当前进程的现场 
     LIST_ENTRY(Env) env_link;     // with env_free_list保存空闲进程 
     u_int env_id;                 // Unique environment identifier 
     u_int env_parent_id;          // env_id of this env's parent 
     u_int env_status;             // Status of the environment 
     Pde *env_pgdir;               // Kernel virtual address of page dir 
     u_int env_cr3; 			   //保存了该进程页目录的物理地址
     LIST_ENTRY(Env) env_sched_link;//用来构造调度队列
     u_int env_pri;					//保存了该进程的优先级
};
/*
env_status取值：
ENV_FREE：该PCB处于空闲链表
ENV_NOT_RUNNABLE：处于阻塞状态
ENV_RUNNABLE：执行或就绪状态
*/
```

```c
struct Trapframe { //lr:need to be modified(reference to linux pt_regs) TODO
    /* Saved main processor registers. */
    unsigned long regs[32];

    /* Saved special registers. */
    unsigned long cp0_status;
    unsigned long hi;
    unsigned long lo;//这就是乘除法那个hi和lo
    unsigned long cp0_badvaddr;
    unsigned long cp0_cause;
    unsigned long cp0_epc;//保存异常返回地址
    unsigned long pc;//程序执行入口地址
};
```



* `void env_init()`，初始化`env_free_list`, 使从list取出的顺序和envs数组顺序相同。

```c
void env_init(void)//in mips_init() 
{
    int i;

    LIST_INIT(&env_free_list);
    LIST_INIT(&env_sched_list[0]);
    LIST_INIT(&env_sched_list[1]);

    struct Env *temp;
    for(i=NENV-1;i>=0;i--){
        temp = envs + i;
        temp->env_status = ENV_FREE;
        LIST_INSERT_HEAD(&env_free_list, temp, env_link);
    }

}
```



#### envid

1. `u_int mkenvid(struct Env *e)`， 生成一个新的进程id（和ASID不同）。生成逻辑

```c
u_int mkenvid(struct Env *e) {
    u_int idx = e - envs;
    u_int asid = asid_alloc();//生成一个介于0和63之间的数，为新创建的进程分配一个异于当前所有未被释放的进程的 ASID
    return (asid << (1 + LOG2NENV)) | (1 << LOG2NENV) | idx;
    //return asid << 11 | 1 << 10 | e-envs，也就是说，第0-9位为e-envs，第10位为1，第11-16位为asid
}
```

2. `int envid2env(u_int envid, struct Env **penv, int checkperm)`,通过一个env 的id获取该id 对应的进程控制块。该PCB不能是ENV_FREE，若checkperm为1，则检查权限：当前进程curenv必须有权限查询envid对应的env，即：要么penv是curenv，要么penv是curenv的直接孩子

   ```c
   int envid2env(u_int envid, struct Env **penv, int checkperm)
   {
       struct Env *e;
       /* Hint: If envid is zero, return curenv.*/
       /* Step 1: Assign value to e using envid. */
       if(envid == 0){
           *penv = curenv;
           return 0;//必须在这里返回，否则在e->env_id != envid会卡住
       }
   
       e = envs + ENVX(envid);
   
       if (e->env_status == ENV_FREE || e->env_id != envid) {
           *penv = 0;
           return -E_BAD_ENV;
       }
       /* Hints:
        *  Check whether the calling env has sufficient permissions
        *    to manipulate the specified env.
        *  If checkperm is set, the specified env
        *    must be either curenv or an immediate child of curenv.
        *  If not, error! */
       /*  Step 2: Make a check according to checkperm. */
       if(checkperm){
           if(!(e == curenv || e->env_parent_id == curenv->env_id)){
               *penv = 0;
               return -E_BAD_ENV;
           }
       }
   
       *penv = e;
       return 0;
   }
   ```

### 创建进程

> 注意，创建进程仅仅是初始化一个进程并将其放入env_sched_list[0]中，并不执行。真正的执行在sched_yield。

* 创建进程用到两个宏定义：

```c
#define ENV_CREATE_PRIORITY(x, y) 
{
    extern u_char binary_##x##_start[]; 
    extern u_int binary_##x##_size;
    env_create_priority(binary_##x##_start, (u_int)binary_##x##_size, y);
}
#define ENV_CREATE(x) 
{ 
    extern u_char binary_##x##_start[];
    extern u_int binary_##x##_size; 
    env_create(binary_##x##_start, (u_int)binary_##x##_size); 
}
//##代表字符串拼接
```

* 创建进程的直接函数有2个

1. `env_create_priority()`, 该函数仅仅在内核初始化时被调用，在第一个用户进程前。

   ```c
   void env_create_priority(u_char *binary, int size, int priority)
   {  
       struct Env *e;
       /* Step 1: Use env_alloc to alloc a new env. */
       if(env_alloc(&e, 0) != 0) return;
       /* Step 2: assign priority to the new env. */
       e->env_pri = priority;
       /* Step 3: Use load_icode() to load the named elf binary,
          and insert it into env_sched_list using LIST_INSERT_HEAD. */
       load_icode(e, binary, size);
       LIST_INSERT_HEAD(&env_sched_list[0], e, env_sched_link);
   }  
   ```

2. `env_create()`, 

   ```c
   void env_create(u_char *binary, int size)
   {
       /* Step 1: Use env_create_priority to alloc a new env with priority 1 */
       env_create_priority(binary, size, 1);
   }
   ```

#### 打造PCB

* 步骤（`env_alloc`函数实现）

1. 申请空闲PCB
2. 初始化PCB
3. 为新进程分配地址空间(`env_setup_vm`函数实现)，建立自映射页表机制
4. 将PCB从空闲链表中摘除

##### 相关函数

1. `int env_alloc(struct Env **new, u_int parent_id)`, 打造PCB的核心函数。主要功能是：

   将一个PCB从空闲链表中取出，为它分配`envid`, `status`变为`RUNNABLE`， 设置 `parent_id`。同时设置`env_tf.cp0_status`和`env_tf.regs[29]`（堆栈）。当进程被调度时，会使用`env_pop_tf`函数将`env_tf.cp0_status`装入SR寄存器

   > `env_tf.cp0_status`被设为0x10001004 ，之后异常返回时
   >
   > - `CU0` 被置高, 意味着 **CP0 被启用**.允许用户态使用mtc0和mfc0
   > - `IM[8 + 4]` 被置高, 意味着**接受 4 号中断**(MOS 中 4 号中断是时钟中断)
   > - `IEp` 被置高, 返回用户态时, `IEc` 将获得 `IEp` 的值(二重栈), 这意味着**返回用户态后启用全局中断使能**.

 ```c
 int env_alloc(struct Env **new, u_int parent_id)
 {
     int r;
     struct Env *e;
 
     /* Step 1: Get a new Env from env_free_list*/
     if(LIST_EMPTY(&env_free_list)){
         *new = NULL;
         return -E_NO_FREE_ENV;
     }
     e = LIST_FIRST(&env_free_list);
 
     /* Step 2: init kernel memory layout for this new Env. The function mainly maps the kernel address to this new Env address. */
     env_setup_vm(e);
 
     /* Step 3: Initialize every field of new Env with appropriate values.*/
     e->env_id = mkenvid(e);
     e->env_status = ENV_RUNNABLE;
     e->env_parent_id = parent_id;
     /* Step 4: Focus on initializing the sp register and cp0_status of env_tf field, located at this new Env. */
     e->env_tf.cp0_status = 0x10001004;
     e->env_tf.regs[29] = USTACKTOP;//设置用户栈
 
     /* Step 5: Remove the new Env from env_free_list. */
     LIST_REMOVE(e, env_link);
     *new = e;
     return 0;
 }
 ```

2. `int env_setup_vm(struct Env *e)`, 为新进程e初始化地址空间，建立自映射页表机制。另外，高2G空间，对于用户进程页表没有用，不用管。

   ```c
   static int env_setup_vm(struct Env *e)
   {
       int i, r;
       struct Page *p = NULL;
       Pde *pgdir;//这里的pgdir是内核中的地址，可以把它当做物理地址，对应着用户态下自映射的那个页目录。也就是说，用户态下的页目录的物理地址，就是PADDR(pgdir)
   
       /* Step 1: Allocate a page for the page directory*/
       if ((r = page_alloc(&p)) < 0) {
           panic("env_setup_vm - page alloc error\n");
           return r;
       }
       p->pp_ref++;
       pgdir = (Pde*) page2kva(p);
   
       /* Step 2: Zero pgdir's field before UTOP. */
       for(i=0;i<PDX(UTOP);i++){        //因此不同进程的异常栈也不同
           pgdir[i] = 0;
       }
       for(i=PDX(UTOP); i<1024 ;i++){   //将UTOP以上的部分，暴露给用户态
           if(i != PDX(UVPT)){          //UVPT是user virtual page table的起始地址
               pgdir[i] = boot_pgdir[i];
           }
       }
   
       e->env_pgdir = pgdir;
       e->env_cr3 = PADDR(pgdir);
       /* UVPT maps the env's own page table, with read-only permission.*/
       e->env_pgdir[PDX(UVPT)]  = e->env_cr3 | PTE_V;//实现自映射，即页目录项指向页目录；由于没有PTE_R位，用户态下用户进程无法通过vpt,vpd修改页表
       return 0;
   }
   ```

##### CP0_STATUS

* 有关cp0_status寄存器的说明：

<img src="https://mdpics4lgw.oss-cn-beijing.aliyuncs.com/aliyun/202306221634070.png" alt="image-20230622090633874" style="zoom:50%;" />

第28bit 设置为1，表示允许在用户模式下使用 CP0 寄存器。

第12bit 设置为1，表示4 号中断可以被响应。

**重点**：

* 低6位是二重栈的结构，当中断发生时，硬件自动将KU~p~和IE~p~ 拷贝到KU~o~与IE~o~， KU~c~和IE~c~拷贝到KU~p~和IE~p~ 。调用`rfe`指令(Return From Exception)将previous flags 写入到current flags， 是前者的逆过程； The current Kernel/User(KU~c~)指CPU是否处于内核态，1表示用户态；the current Interrupt Enable(IE~c~)指外部中断是否开启，1表示开启，否则忽略外部中断。中断发生后，KU~c~和IE~c~拷贝到KU~p~和IE~p~ ， p means previous， 之后IE~c~被清零，以防止外部再次中断。

  > gxemul认为STATUS寄存器的KUc为0是内核态，KUc为1是用户态

* 15-8 位为中断屏蔽位，为1代表允许中断。其中 15-10 位代表使能外部中断源，9-8 位是软件可写的中断位。

#### 加载程序代码

将可执行ELF文件加载进内存，由于没有实现文件系统，此刻的elf文件以二进制数组形式代替。

##### 相关函数

函数调用顺序：`load_icode -> load_elf -> load_icode_mapper`

1. `void load_icode`，为新用户进程的堆栈分配了物理页面，指定了程序入口点，并调用了`load_elf`函数将elf文件加载到e中

   ```c
   static void load_icode(struct Env *e, u_char *binary, u_int size)
   {   
       struct Page *p = NULL;
       u_long entry_point;
       u_long r;
       u_long perm;
   
       /* Step 1: alloc a page. */
       r = page_alloc(&p);//no need to ref++, because page_insert will do it 
       if(r<0) return;
       /* Step 2: Use appropriate perm to set initial stack for new Env. */
   
       r = page_insert(e->env_pgdir, p, USTACKTOP-BY2PG, PTE_R);
       if(r<0) return;
       /* Step 3: load the binary using elf loader. */
       load_elf(binary, size, &entry_point, e, load_icode_mapper);
   
       /* Step 4: Set CPU's PC register as appropriate value. */
       e->env_tf.pc = entry_point;
   }
   ```

2. `int load_elf`, 将elf文件载入进程中，其中回调函数load_icode_mapper 

   ```c
   //lib/kernel_elfloader.c
   int load_elf(u_char *binary, int size, u_long *entry_point, void *user_data,
                int (*map)(u_long va, u_int32_t sgsize,
                           u_char *bin, u_int32_t bin_size, void *user_data))
   {
       Elf32_Ehdr *ehdr = (Elf32_Ehdr *)binary;
       Elf32_Phdr *phdr = NULL;
   
       u_char *ptr_ph_table = NULL;
       Elf32_Half ph_entry_count;
       Elf32_Half ph_entry_size;
       int r;
   
       // check whether `binary` is a ELF file.
       if (size < 4 || !is_elf_format(binary)) {
           return -1;
       }
   
       ptr_ph_table = binary + ehdr->e_phoff;
       ph_entry_count = ehdr->e_phnum;
       ph_entry_size = ehdr->e_phentsize;
   
       while (ph_entry_count--) {
           phdr = (Elf32_Phdr *)ptr_ph_table;
   
           if (phdr->p_type == PT_LOAD) {
               /* Real map all section at correct virtual address.Return < 0 if error. */
               r = map(phdr->p_vaddr, phdr->p_memsz, binary + phdr->p_offset, phdr->p_filesz, user_data);
               if(r!=0){
                   return r;
               }
           }
           ptr_ph_table += ph_entry_size;
       }
   
       *entry_point = ehdr->e_entry;//此字段指明程序入口的虚拟地址
       return 0;
   }
   
   ```

   

3. `int load_icode_mapper(...)`, 将单个 segment 加载到对应的虚地址，以实现segment的正确载入内存。

   > va(该段需要被加载到的虚地址)
   >
   > sgsize(该段在内存中的大小)
   >
   > bin(该段的位置，直接用)
   >
   > bin_size(该段在文件中的大小)

   ```c
   static int load_icode_mapper(u_long va, u_int32_t sgsize,
                                u_char *bin, u_int32_t bin_size, void *user_data)
   {
       struct Env *env = (struct Env *)user_data;
       struct Page *p = NULL;
       u_long i = 0;// i is offset from va
       int r, size;
       u_long offset = va - ROUNDDOWN(va, BY2PG);//how far va from the page
   
       //split into 3 segments,va~BY2PG, BY2PG~BY2PG, BY2PG~bin_size
       /* Step 1: load all content of bin into memory. */
       if(offset){
           p = page_lookup(env->env_pgdir, va+i, NULL);
           if(p == 0){//page mapped to va isn't exist
               if((r = page_alloc(&p)) < 0){
                   return r;
               }
               if((r = page_insert(env->env_pgdir, p, va+i, PTE_R)) < 0){
                   return r;
               }
           }
           size = MIN(BY2PG-offset, bin_size-i);
           bcopy(bin+i, page2kva(p)+offset, size);
           i+=size;
       }
   
       while(i < bin_size) {
           if((r = page_alloc(&p)) < 0){
               return r;
           }
           if((r = page_insert(env->env_pgdir, p, va+i, PTE_R)) < 0){
               return r;
           }
           size = MIN(BY2PG, bin_size - i);
           bcopy(bin+i, page2kva(p), size);
           i+=BY2PG;
       }
       /* Step 2: alloc pages to reach `sgsize` when `bin_size` < `sgsize`.
        * hint: variable `i` has the value of `bin_size` now! */
       i = bin_size;
       offset = va+i - ROUNDDOWN(va+i, BY2PG);
       if(offset){
           p = page_lookup(env->env_pgdir, va+i, NULL);
           if(p == 0){//page mapped to va isn't exist
               if((r = page_alloc(&p)) < 0){
                   return r;
               }
               if((r = page_insert(env->env_pgdir, p, va+i, PTE_R)) < 0){
                   return r;
               }
           }
           size = MIN(BY2PG-offset, sgsize-i);
           bzero(page2kva(p)+offset, size);
           i+=size;
       }    
   
       while (i < sgsize) {
           if((r = page_alloc(&p)) < 0){
               return r;
           }
           if((r = page_insert(env->env_pgdir, p, va+i, PTE_R)) < 0){
               return r;
           }
           size = MIN(BY2PG, sg_size - i);
           bzero(page2kva(p), size);
   		i+=BY2PG;
       }
       return 0;
   }
   ```

   

### 进程的运行和切换

#### 进程的运行

进程运行使用的基本函数

1. `void env_run(struct Env *e)`,包括两部：

• 保存当前进程上下文 (如果当前没有运行的进程就跳过这一步)

• 恢复要启动的进程e的上下文，然后运行该进程e。

进程的上下文指进程运行时的周围环境信息，包括寄存器变量，也就是env_tf。而这些寄存器在lab3中的位置是在TIMESTACK区域。

```c
extern void env_pop_tf(struct Trapframe *tf, int id);
extern void lcontext(u_int contxt);

void env_run(struct Env *e)
{
    /* Step 1: 保存当前进程的上下文信息，设置当前进程上下文中的 pc 为epc */
    if(curenv){
        struct TrapFrame *old;
        old = (struct TrapFrame *)(TIMESTACK - sizeof(struct TrapFrame));
        //将SAVE_ALL保存到TIMESTACK中的数据，保存到当前到旧进程的env_tf中。
        bcopy(old, &(curenv->env_tf), sizeof(struct TrapFrame));
        curenv->env_tf.pc = curenv->env_tf.cp0_epc;
    }

    /* Step 2: 切换 curenv 为即将运行的进程 */
    curenv = e;

    /* Step 3: 调用 lcontext 函数，设置全局变量mCONTEXT为当前进程页目录地址，这个值将在TLB重填时用到。 */
    lcontext(e->env_pgdir);

    /* Step 4: Use env_pop_tf() to restore the environment's
     * environment registers and return to user mode. 恢复进程e现场，异常返回
     */
    env_pop_tf(&(e->env_tf), GET_ENV_ASID(e->env_id));
}
```

2. `env_pop_tf(struct Trapframe * tf, int asid)`， 恢复tf中内容到协处理器和寄存器堆。具体执行了以下事情
   1. 设置CP0_ENTRYHI的值为asid
   2. 设置CP0_STATUS的低2位为0， 也就是KU~c~和IE~c~
   3. 将寄存器堆的32个寄存器的值置为tf中的32个regs($26与\$27除外)， hi 与 lo， cp0_epc，pc, cp0_status同理。也就是说，配置好进程要执行的环境。
   4. 跳转到pc寄存器中的地址
   5. 执行`rfe` ，Return From Exception， 将previous flags 写入到current flags， 是前者的逆过程。*（进程每次被调度运行前一定会执行的 rfe汇编指令。）*

#### 进程销毁

1. `void env_destroy(struct Env *e)`， 进程的销毁，如果销毁的是当前进程，那么去运行另一个进程

```c
//Free env e, and schedule to run a new env if e is the current env.
void env_destroy(struct Env *e)
{
    /* Hint: free e. */
    env_free(e);

    /* Hint: schedule to run a new environment. */
    if (curenv == e) {
        curenv = NULL;
        /* Hint: Why this? */
        bcopy((void *)KERNEL_SP - sizeof(struct Trapframe),
              (void *)TIMESTACK - sizeof(struct Trapframe),
              sizeof(struct Trapframe));
        printf("i am killed ... \n");
        sched_yield();
    }
}
```

2. `void env_free(e)`， 仅仅销毁进程，和它所占有的所有物理空间。同时从env_sched_list中取出，将其放置在env_free_list中。

   ```c
   /* Overview:
    *  Free env e and all memory it uses.
    */
   void
       env_free(struct Env *e)
   {
       Pte *pt;
       u_int pdeno, pteno, pa;
   
       /*输出某进程要 消亡*/
       printf("[%08x] free env %08x\n", curenv ? curenv->env_id : 0, e->env_id);
   
       /* Hint: Flush all mapped pages in the user portion of the address space */
       for (pdeno = 0; pdeno < PDX(UTOP); pdeno++) {
           /* Hint: only look at mapped page tables. */
           if (!(e->env_pgdir[pdeno] & PTE_V)) {
               continue;
           }
           /* Hint: find the pa and va of the page table. */
           pa = PTE_ADDR(e->env_pgdir[pdeno]);
           pt = (Pte *)KADDR(pa);
           /* Hint: Unmap all PTEs in this page table. */
           for (pteno = 0; pteno <= PTX(~0); pteno++)//PTX(~0) = 1023
               if (pt[pteno] & PTE_V) {
                   page_remove(e->env_pgdir, (pdeno << PDSHIFT) | (pteno << PGSHIFT));//删除va虚拟地址对物理地址的映射。该物理地址所在页的引用次数减1
               }
           /* Hint: free the page table itself. */
           e->env_pgdir[pdeno] = 0;
           page_decref(pa2page(pa));
       }
       /* Hint: free the page directory. */
       pa = e->env_cr3;
       e->env_pgdir = 0;
       e->env_cr3 = 0;
       /* Hint: free the ASID */
       asid_free(e->env_id >> (1 + LOG2NENV));
       page_decref(pa2page(pa));
       /* Hint: return the environment to the free list. */
       e->env_status = ENV_FREE;
       LIST_INSERT_HEAD(&env_free_list, e, env_link);
       LIST_REMOVE(e, env_sched_link);
   }
   ```

### 中断与异常

*凡是引起控制流突变的都叫做异常，而中断仅仅是异常的一种，并且是仅有的一种异步异常。*

发生异常（在本实验中，大多是中断）时，硬件CPU会处理：

1. 设置 EPC 指向异常结束时重新返回的地址。

2. 设置 SR 寄存器，**强制 CPU 进入内核态**（行驶更高级的特权）并禁止中断。

   > 将 IEc,KUc 拷贝至 KUp 和IEp 中，同时将 IEc 置为 0，表示关闭全局中断使能，将 KUc 置 0，表示处于内核态。

3. 设置 Cause 寄存器，保存 ExcCode 段（中断异常码为0），用于记录异常发生的原因。

4. CPU 开始从异常入口位置 `0x80000080`取指，此后一切交给软件处理。

软件开始处理：

1. 首先进入异常分发程序，调用相应的异常处理程序。` In boot/start.S`， 之后跳转到对应的异常处理函数中。如果是中断，相应的异常处理函数（中断处理程序）为`handle_int`。
2. 在中断处理程序`handle_int`中，判断 CP0_CAUSE 寄存器中是由几号中断位引发的中断，然后进入不同中断对应的中断服务函数。对于时钟中断，进入`time_req`函数，执行`sched_yield`。
3. 中断处理完成，将 EPC 的值取出到 PC 中，恢复 SR 中相应的中断使能，继续执行。

有3个cp0寄存器我们用到：

#### Cause寄存器

其中保存着 CPU 中哪一些中断或者异常已经发生。

ExcCode是记录发生了什么异常。

如果发生了特别的异常：中断(ExcCode 为 0)， 15-8 位保存着哪一些中断发生了，其中 15-10 位来自硬件，9-8 位可以由软件写入。当 SR 寄存器中相同位允许中断（为 1）时，Cause 寄存器这一位活动就会导致中断。

> **BD域为一个接口（和操作系统通信的协议），如果异常指令为 延迟槽指令，该位置1. 当异常指令为延迟槽指令时，`epc`为延迟槽的前一条指令，即跳转指令。**

#### 异常分发

一旦 CPU 发生异常，就会自动跳转到地址 0x80000080 处，开始执行异常分发程序(`boot/start.S`)。该程序作用就是跳转到$exception\_handlers[CP0\_CAUSE[6:2]]$对应的中断处理函数。

```c
//boot/start.S
.section .text.exc_vec3 //表示代码地址为0x80000080
NESTED(except_vec3, 0, sp)
1:
    mfc0 k1,CP0_CAUSE
    la k0,exception_handlers
    andi k1,0x7c //fetch CAUSE[6:2] << 4
    addu k0,k1
    lw k0,(k0)   //right k0 is the &exception_handlers[CAUSE[6:2]]
    nop
    jr k0        //skip to the corresponding program to solve exception
    nop
END(except_vec3)
```



##### 异常向量组

异常分发程序通过 exception_handlers 数组(异常向量组)定位中断处理程序。向量的意思就是指向，数组中存放的是函数的地址。

* exception_handlers数组的初始化

`trap_init()`函数初始化异常向量组，

```c
unsigned long exception_handlers[32];
//lib/traps.c
void trap_init()
{
    int i;
    for (i = 0; i < 32; i++) {
        set_except_vector(i, handle_reserved);//handle_reserved函数什么也不干
    }

    set_except_vector(0, handle_int);
    set_except_vector(1, handle_mod);
    set_except_vector(2, handle_tlb);
    set_except_vector(3, handle_tlb);
    set_except_vector(8, handle_sys);
}

void *set_except_vector(int n, void *addr)//设置异常向量组[n]=addr, 返回原来的old_addr.
{
    unsigned long handler = (unsigned long)addr;
    unsigned long old_handler = exception_handlers[n];
    exception_handlers[n] = handler;
    return (void *)old_handler;
}
```

**0 号异常**的处理函数为`handle_int`，表示中断，由时钟中断、控制台中断等中断造成

**1 号异常**的处理函数为`handle_mod`，表示存储异常，进行存储操作时该页被标记为只读

**2 号异常**的处理函数为`handle_tlb`，TLB 异常，TLB 中没有和程序地址匹配的有效入口

**3 号异常**的处理函数为`handle_tlb`，TLB 异常，TLB 失效，且未处于异常模式（用于提高处理效率）

**8 号异常**的处理函数为`handle_sys`，**系统调用**，陷入内核，执行了 syscall 指令

##### 0号异常(中断)

`handle_int`：

```c
//lib/genex.S
NESTED(handle_int, TF_SIZE, sp)
.set    noat//静止assembler 使用 $at寄存器

//1: j 1b
nop

SAVE_ALL    //保存所有寄存器，包括寄存器堆和CP0(除了CP0.PC)，在本函数中保存到TIMESTACK中;保存程序被中断的指令的地址到栈EPC
CLI			//将IE设为0，屏蔽中断；将CU0设为1，允许用户态使用CP0寄存器(发生异常时已经进入内核态了)???
.set    at
mfc0    t0, CP0_CAUSE
mfc0    t2, CP0_STATUS
and     t0, t2

andi    t1, t0, STATUSF_IP4//IP4 = 0x1000，只有当t0&0x1000不为0时，才会执行timer_irq
bnez    t1, timer_irq      //t1不等于0跳转，判断是否是4号中断位引发的中断，如果是，跳转到中断服务程序timer_irq
nop
END(handle_int)
```

* `SAVE_ALL`宏

```c
.macro SAVE_ALL//保存除了CP0.PC之外的所有寄存器，之所以不保存PC，是因为之后在env_run()中PC被设置为EPC，不必保存
1:
        move    k0,sp
        get_sp  //设置sp
        move    k1,sp
        subu    sp,k1,TF_SIZE //初始化sp, TF_REG0 = 0, TF_SIZE = Size of stack frame
        sw  k0,TF_REG29(sp)   //先保存栈指针和$v0, 使之后可以放心使用
        sw  $2,TF_REG2(sp)//v0
        mfc0    v0,CP0_STATUS
        sw  v0,TF_STATUS(sp)
        mfc0    v0,CP0_CAUSE
        sw  v0,TF_CAUSE(sp)
        mfc0    v0,CP0_EPC
        sw  v0,TF_EPC(sp)
        mfc0    v0, CP0_BADVADDR
        sw  v0, TF_BADVADDR(sp)
        mfhi    v0
        sw  v0,TF_HI(sp)
        mflo    v0
        sw  v0,TF_LO(sp)
        sw  $0,TF_REG0(sp)
        ...
        sw  $31, TF_REG31(sp)
.endm

.macro get_sp      //macro
    mfc0    k1, CP0_CAUSE
    andi    k1, 0x107C
    xori    k1, 0x1000
    bnez    k1, 1f     //if is time_interrupt, k1 should be 0. not branch
            		   //if is syscall, k1 should not be 0.	branch
    nop
    li  sp, 0x82000000 // sp = TIMESTACK，这一步就是将sp设为TIMESTACK
    j   2f
    nop
1:
    bltz    sp, 2f     //sp < 0, branch 
    nop
    lw  sp, KERNEL_SP  //KERNEL_SP是一个变量
    nop

2:  nop


.endm
```



#### 时钟中断

CPU是如何知晓一个进程的时间片结束的呢？就是通过定时器产生的时钟中断。`kclock_init` 函数完成了时钟的初始化，仅调用了`set_timer()`。

1. `set_timer`.开启时钟中断

   > 0xb5000000 是模拟器(gxemul) 映射实时钟的位置，对于外部设备，它们会映射到某个内存地址被系统管理。偏移量为0x100 表示来设置实时钟中断的频率，0xc8 则表示1 秒钟中断200次，如果写入0，表示关闭实时钟

```c
//.kclock_asm.S
.macro  setup_c0_status set clr 
//定义一个宏setup_c0_status， 以及形参set 和 clr。set和clr使用时，前加\。
//该宏的作用是，将SR寄存器中set对应的位设为1，clr对应的位设为0
    .set    push                //save all settings setted before， 比如之前的.set设置
    mfc0    t0, CP0_STATUS	    //t0 = CP0_STATUS
    or  t0, \set|\clr		   
    xor t0, \clr			  
    mtc0    t0, CP0_STATUS	   
    .set    pop				   //restore saved settings
.endm

.text
LEAF(set_timer)

    li t0, 0xc8                    //为t0赋值0xc8，代表一秒中断200次
    sb t0, 0xb5000100			   //将0xc8存入0xb5000100处，0xb5000100存储的数字就是中断频率
    sw  sp, KERNEL_SP			   //将栈指针的值存入KERNEL_SP中
	setup_c0_status STATUS_CU0|0x1001 0 
    /*
    * 调用宏，将SR的第28位、第12位，第0位置1，即允许用户态使用CP0，
    * 允许4号中断（时钟中断），允许外部中断
    */
    jr ra					       //函数返回

    nop
END(set_timer)
```

2. 当时钟中断来临时，`timer_irq`执行，开始进程调度。

```c
timer_irq:

    sb zero, 0xb5000110 //承认这次中断，详见https://gavare.se/gxemul/gxemul-stable/doc/experiments.html#expdevices
1:  j   sched_yield    //进程调度
    nop
    j   ret_from_exception //不会执行 
    nop
```



### 进程调度

使用时间片轮转算法。

算法分析：

* 什么时候调用？时钟中断来临；第一次执行一个用户进程时；当改变e的进程状态时
* 若不是第一次执行用户进程，那么两种可能：时钟中断来临时时间片用完；e的状态不为runnable，这两种请况都需要从当前调度队列中移出，移到对面的队列。（若e为ENV_FREE, 直接移出，但不出现应该）

```c
//lib/sched.c
void sched_yield(void)
{

    static int count = 0; // remaining time slices of current env
    static int point = 0; // current env_sched_list index, 0 or 1
    static struct Env *e = NULL;

    if(count == 0 || e == NULL || e->env_status != ENV_RUNNABLE){
        if(e != NULL){ //count == 0 || status is not runnable
            LIST_REMOVE(e, env_sched_link);
            //一般不会出现ENV_FREE的情况。
            if(e->env_status != ENV_FREE){ 
                LIST_INSERT_TAIL(&env_sched_list[1-point], e, env_sched_link);
            }
        }
        while(1){
            while(LIST_EMPTY(&env_sched_list[point]))
                point = 1 - point;
            e = LIST_FIRST(&env_sched_list[point]);
            if(e->env_status == ENV_FREE){
                LIST_REMOVE(e, env_sched_link);
            } else if(e->env_status == ENV_NOT_RUNNABLE){
                LIST_REMOVE(e, env_sched_link);
                LIST_INSERT_TAIL(&env_sched_list[1-point], e, env_sched_link);
            } else {
                count = e->env_pri;
                break;
            }
        }
    }

    count --;
    env_run(e);
}
```



## 实验难点

该实验的难点主要集中在进程运行和异常处理的流程是否搞清楚了。

* 创建进程

<img src="https://mdpics4lgw.oss-cn-beijing.aliyuncs.com/aliyun/202306221634011.png" alt="lab3-1" style="zoom: 80%;" />

---



* 异常处理（针对时钟中断）

![lab3-2](https://mdpics4lgw.oss-cn-beijing.aliyuncs.com/aliyun/202306221634020.png)





## 实验疑点

以下是本人做Lab3时产生的疑点，都已解决。

1. 内核页表的作用？

   页表本身作用有很多。

   * 内核启动过程中，为了实现分页机制，需要内核页表。
   * 用户进程创建时，需要复制高于UTOP的地址空间布局

2. entry_point是干什么的?

   `entry_point`是ehdr->e_entry， 此字段指明程序入口的虚拟地址。即当文件被加载到进程空间里后，入口程序在进程地址空间里的地址。对于可执行程序文件来说，当 ELF 文件完成加载之 后，程序将从这里开始执行

3. `load_icode_mapper`函数中`va`如果不按页对齐，而那个页面已经存放别的内容了，有影响吗？

   各个进程是相互独立的，互相不可见的。创建进程时，对于进程低2G空间都是空的，不会有别的东西的。

4. tlb重填的时候，怎么知道在哪里找页表？

   进程开始执行时，函数`env_run()`会执行`lcontext()`函数，更新mCONTEXT为当前进程页表基地址。mCONTEXT 中存储了当前进程一级页表基地址位于 kseg0 的虚拟地址，tlb重填就是根据mCONTEXT重填。

5. 非叶子函数究竟是怎么回事？

## 实验补充

### 非叶子函数

定义：

```c
#define NESTED(symbol, framesize, rpc)     
   		...
   		.frame  sp, framesize, rpc
        //.frame framereg, framesize, returnreg
           
```

这里.frame标识有三个操作数：

* **framereg**:    寄存器用来获取本地栈，通常为**$sp**
* **returnreg**：保存函数返回地址的寄存器，通常为**$0**，表明返回地址存在栈中。如果为叶子函数（不调用其他函数）则为**$31**
* **framesize**：为函数分配的栈帧大小，满足**\$sp + framesize = previous $sp**