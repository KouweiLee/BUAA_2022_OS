# Lab4

## 实验学习

### 工具

##### page相关函数

1. `page_alloc(struct Page **p)`，该函数里执行了bzero操作，并没有执行ref++
2. `page_free(struct Page *pp)`，判断是否可以被free并free掉
3. `page_decref(struct Page *pp)`， pp->ref--， 并如果能free就free掉
4. `int pgdir_walk(Pde *pgdir, u_long va, int create, Pte **ppte)`，**运行时的二级页表检索函数**，目的是寻找va对应的二级页表项。将二级页表项地址放在ppte
5. `int page_insert(Pde *pgdir, struct Page *pp, u_long va, u_int perm)`， 增加地址映射函数，将va 这一虚拟地址映射到内存控制块 pp 对应的物理页面，并将页表项权限为设置为 perm。函数内部自动执行了`tlb_invalidate(pgdir, va)`和`pp->ref++`
6. `struct Page *page_lookup(Pde *pgdir, u_long va, Pte **ppte)`，返回va所在的物理页，以及该页所对应的二级页表项的位置(ppte)。若ppte为NULL,则不管二级页表项；若返回值为0，代表va没有映射到任何一页
7. `void page_remove(Pde *pgdir, u_long va)`，移除va的映射关系

##### PCB

```c
struct Env {
    struct Trapframe env_tf;        // Saved registers
    LIST_ENTRY(Env) env_link;       // Free list
    u_int env_id;                   // Unique environment identifier
    u_int env_parent_id;            // env_id of this env's parent
    u_int env_status;               // Status of the environment
    Pde  *env_pgdir;                // Kernel virtual address of page dir
    u_int env_cr3;
    LIST_ENTRY(Env) env_sched_link;
        u_int env_pri;
    // Lab 4 IPC
    u_int env_ipc_value;            // data value sent to us 
    u_int env_ipc_from;             // envid of the sender  
    u_int env_ipc_recving;          // env is blocked receiving
    u_int env_ipc_dstva;            // va at which to map received page
    u_int env_ipc_perm;             // perm of page mapping received

    // Lab 4 fault handling
    u_int env_pgfault_handler;      // page fault state
    u_int env_xstacktop;            // top of exception stack
};
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



### 系统调用机制的实现

系统调用实际上是操作系统为用户态提供的一组接口，进程在用户态下通过系统调用可以访问内核提供的文件系统等服务.

#### msyscall函数

`syscall_*`函数是用户态下最接近内核的函数，和`sys_*`内核中的系统调用函数一一对应

`syscall_*`函数的实现中，都调用了msyscall函数，该函数的第一个参数都是一个与调用名相似的宏（如`SYS_putchar`）， 这个参数为**系统调用号**。

> 系统调用号是内核区分何种系统调用的唯一依据

```c
//include/unistd.h
#define __SYSCALL_BASE 9527
#define __NR_SYSCALLS 20
#define SYS_putchar         ((__SYSCALL_BASE ) + (0 ) ) 
#define SYS_getenvid        ((__SYSCALL_BASE ) + (1 ) )
#define SYS_yield           ((__SYSCALL_BASE ) + (2 ) )
#define SYS_env_destroy     ((__SYSCALL_BASE ) + (3 ) )
#define SYS_set_pgfault_handler ((__SYSCALL_BASE ) + (4 ) )
#define SYS_mem_alloc       ((__SYSCALL_BASE ) + (5 ) )
#define SYS_mem_map         ((__SYSCALL_BASE ) + (6 ) )
#define SYS_mem_unmap       ((__SYSCALL_BASE ) + (7 ) )
#define SYS_env_alloc       ((__SYSCALL_BASE ) + (8 ) )
#define SYS_set_env_status  ((__SYSCALL_BASE ) + (9 ) )
#define SYS_set_trapframe       ((__SYSCALL_BASE ) + (10 ) )
#define SYS_panic           ((__SYSCALL_BASE ) + (11 ) )
#define SYS_ipc_can_send        ((__SYSCALL_BASE ) + (12 ) )
#define SYS_ipc_recv        ((__SYSCALL_BASE ) + (13 ) )
#define SYS_cgetc           ((__SYSCALL_BASE ) + (14 ) )
```

`msyscall`函数共6个参数，剩下的5个参数是系统调用时需要传给内核的。如何传递呢？

```c
//user/syscall_wrap.S
LEAF(msyscall)
    syscall//这里发生了异常，进入异常分发程序进而进入handle_sys函数。当返回时，会返回到这里的下一条指令
    nop
    jr ra
    nop
END(msyscall)
```

#### handle_sys

```assembly
#lib/syscall.S
NESTED(handle_sys,TF_SIZE, sp)
    SAVE_ALL                            // Macro used to save trapframe
    CLI                                 // Clean Interrupt Mask
    nop
    .set at                             // Resume use of $at

	#返回后返回到syscall的下一条指令
    lw t0, TF_EPC(sp)
    addiu t0, t0, 4
    sw t0, TF_EPC(sp)
    #Copy the syscall number into $a0.
    lw a0, TF_REG4(sp) #其实a0没变

    addiu   a0, a0, -__SYSCALL_BASE     // a0 <- relative syscall number
    sll     t0, a0, 2                   // t0 <- relative syscall number times 4
    la      t1, sys_call_table          // t1 <- syscall table base
    addu    t1, t1, t0                  // t1 <- table entry of specific syscall
    lw      t2, 0(t1)                   // t2 <- function entry of specific syscall

    lw      t0, TF_REG29(sp)            // t0 <- user's stack pointer
    lw      t3, 16(t0)                  // t3 <- the 5th argument of msyscall
    lw      t4, 20(t0)                  // t4 <- the 6th argument of msyscall
    #在当前内核栈下压入调用sys_*函数的6个参数，并且将前4个参数放入$a0-a4中，满足mips调用规范
    lw a0, TF_REG4(sp)
    lw a1, TF_REG5(sp)
    lw a2, TF_REG6(sp)
    lw a3, TF_REG7(sp)
    //调用sys_*函数前的参数入栈。
    addiu sp, sp, -24
    /*sw a0, 0(sp)
    sw a1, 4(sp)
    sw a2, 8(sp)
    sw a3, 12(sp)*/
    sw t3, 16(sp)
    sw t4, 20(sp)

    jalr    t2                          // 跳转到sys_*函数
    nop

    #恢复内核栈
    addiu sp, sp, 24
    #将sys_*函数的返回值保存到trapFrame中，使用户进程恢复后可见
    sw      v0, TF_REG2(sp)             

    j       ret_from_exception          // Return from exeception
    nop
    END(handle_sys)

sys_call_table:                         // Syscall Table，注意，这里的顺序和系统调用号一致
	.align 2
    .word sys_putchar
    .word sys_getenvid
    .word sys_yield
    .word sys_env_destroy
    .word sys_set_pgfault_handler
    .word sys_mem_alloc
    .word sys_mem_map
    .word sys_mem_unmap
    .word sys_env_alloc
    .word sys_set_env_status
    .word sys_set_trapframe
    .word sys_panic
    .word sys_ipc_can_send
    .word sys_ipc_recv
    .word sys_cgetc
```

> 如果考虑syscall位于延迟槽的时候，.set at 指令后加
>
> ```assembly
> 	lw		t0, TF_EPC(sp)
> 	lw		t1, TF_CAUSE(sp)
> 	lui		t2, 0x8000
> 	and		t1, t1, t2
> 	bnez	t1, IS_BD
> 	nop
> 	addiu	t0, t0, 4
> 	j BD_IF_END
> 	nop
> IS_BD:
> BD_IF_END:
> 	sw		t0, TF_EPC(sp)
> ```
>
> 注意，如果延迟槽指令发生异常，epc中存储的是跳转指令的地址，cause寄存器最高位为1

`ret_from_exception`函数定义：异常返回

```assembly
//lib/genex.S
FEXPORT(ret_from_exception)
    RESTORE_SOME//恢复除了sp寄存器的值
    
    lw  k0,TF_EPC(sp)
    lw  sp,TF_REG29(sp) /* Deallocate stack */

    jr  k0
    rfe
```

```assembly
.macro RESTORE_SOME
        .set    mips1
        mfc0    t0,CP0_STATUS
        ori t0,0x3
        xori    t0,0x3
        mtc0    t0,CP0_STATUS//set lower 2 bits to 0
        lw  v0,TF_STATUS(sp)
        li  v1, 0xff00
        and t0, v1
        nor v1, $0, v1
        and v0, v1
        or  v0, t0
        mtc0    v0,CP0_STATUS
        lw  v1,TF_LO(sp)
        mtlo    v1
        lw  v0,TF_HI(sp)
        lw  v1,TF_EPC(sp)
        mthi    v0
        mtc0    v1,CP0_EPC
        lw  $31,TF_REG31(sp)
        lw  $30,TF_REG30(sp)
        lw  $28,TF_REG28(sp)
        lw  $25,TF_REG25(sp)
        lw  $24,TF_REG24(sp)
        ...
.endm        
```



### 用户态系统调用函数

位于**user/syscall_lib.c**

1. `int syscall_mem_map(u_int srcid, u_int srcva, u_int dstid, u_int dstva, u_int perm)`

   ```c
   int syscall_mem_map(u_int srcid, u_int srcva, u_int dstid, u_int dstva, u_int perm)
   {
       return msyscall(SYS_mem_map, srcid, srcva, dstid, dstva, perm);
   }
   ```

   

### 内核态系统调用函数

位于`lib/syscall_all.c`。

1. `int sys_mem_alloc(int sysno,u_int envid, u_int va, u_int perm)`， 为`envid`进程的虚拟地址va申请一块内存。需要保证权限位`perm`中有`PTE_V`且无`PTE_COW`，`va < UTOP`。若之前va已经有对应的内存了，则解除原来映射关系。*确保执行该系统调用的进程是该进程或其父进程*？？？

   ```c
   int sys_mem_alloc(int sysno, u_int envid, u_int va, u_int perm)
   {
       struct Env *env;
       struct Page *ppage;
       int ret;
       ret = 0;
       if(va >= UTOP) return -E_INVAL;
       if((perm & PTE_V) == 0) return -E_INVAL;
       if((perm & PTE_COW) != 0) return -E_INVAL;//若va要申请的页需要有PTE_COW写时复制机制，显然是不合理的。
   
       ret = page_alloc(&ppage);
       if(ret < 0) return ret;
       ret = envid2env(envid, &env, 1);//确保执行该系统调用的进程是该进程或其父进程
       if(ret < 0) return ret;
       ret = page_insert(env->env_pgdir, ppage, va, perm);
       if(ret < 0) return ret;
       return 0;
   
   }
   ```

2. `int sys_mem_map(int sysno,u_int srcid, u_int srcva, u_int dstid, u_int dstva, u_int perm)`，将进程`srcid`的`srcva`对应的物理页面映射到进程`dstid`的`dstva`虚拟地址 (若`dstva`已经对应了一块物理页面则取消映射关系)。对于`dstid`的权限位为`perm`。若`srcva`并未映射到实际物理空间，返回负值

   ```c
   int sys_mem_map(int sysno, u_int srcid, u_int srcva, u_int dstid, u_int dstva,
                   u_int perm)
   {
       int ret;
       u_int round_srcva, round_dstva;
       struct Env *srcenv;
       struct Env *dstenv;
       struct Page *ppage;
       Pte *ppte;
   
       ppage = NULL;
       ret = 0;
       round_srcva = ROUNDDOWN(srcva, BY2PG);
       round_dstva = ROUNDDOWN(dstva, BY2PG);
       if(srcva >= UTOP || dstva >= UTOP) return -E_INVAL;
       if((perm & PTE_V) == 0) return -E_INVAL;
       ret = envid2env(srcid, &srcenv, 0);
       if(ret < 0) return ret;
       ret = envid2env(dstid, &dstenv, 0);
       if(ret < 0) return ret;
       ppage = page_lookup(srcenv->env_pgdir, round_srcva, NULL);
       if(ppage == 0) return -E_INVAL;
       ret = page_insert(dstenv->env_pgdir, ppage, round_dstva, perm);
       return ret;
   }
   ```

3. `int sys_mem_unmap(int sysno,u_int envid, u_int va)`, 解除进程envid虚拟内存和物理内存之间的映射关系，若并未映射，则什么也不干。

   ```c
   int sys_mem_unmap(int sysno, u_int envid, u_int va)
   {
       // Your code here.
       int ret;
       struct Env *env;
       if(va >= UTOP) return -E_INVAL;
       ret = envid2env(envid, &env, 1);
       if(ret < 0) return ret;
       page_remove(env->env_pgdir, va);
       return ret;
   }
   ```

   

4. `void sys_yield(void)`，实现用户进程对CPU的放弃， 从而调度其他的进程。

   > 这里需要将进程上下文信息保存到TIMESTACK中，因为执行sched_yield()函数时，会调用env_run()，将TIMESTACK中的进程上下文保存到被替换进程的PCB中。
   >
   > 执行前需要进程的状态变为ENV_NOT_RUNNABLE

   ```c
   void sys_yield(void)
   {
       bcopy((void *)KERNEL_SP - sizeof(struct Trapframe),
             (void *)TIMESTACK - sizeof(struct Trapframe),
             sizeof(struct Trapframe));
       sched_yield();
   }
   ```

5. `sys_env_alloc`，

6. 其他非常用系统调用。

   1. 获取当前进程的`env_id`

      ```c
      u_int sys_getenvid(void)
      {
          return curenv->env_id;
      }
      ```

      

   2. 销毁`envid`进程

      ```c
      int sys_env_destroy(int sysno, u_int envid);
      ```

   3. 设置envid进程的status。设置`envid`进程的`env_status`。如果env_status不为`ENV_RUNNABLE`并且`status`为`ENV_RUNNABLE`，则将其加入调度队列尾

      ```c
      int sys_set_env_status(int sysno, u_int envid, u_int status)
      {
          struct Env *env;
          int ret;
          if(status != ENV_FREE && status != ENV_RUNNABLE && status != ENV_NOT_RUNNABLE)
              return -E_INVAL;
          ret = envid2env(envid, &env, 0);
          if(ret < 0) return ret;
          if(status == ENV_RUNNABLE && env->env_status != ENV_RUNNABLE){
              LIST_INSERT_TAIL(&env_sched_list[0], env, env_sched_link);
          }
          env->env_status = status;
          return 0;
          //  panic("sys_env_set_status not implemented");
      }
      ```

      

### 进程间通信机制IPC

两个进程之间可以通信，需要共享物理页面，而所有的进程都共享了内核所在的2G空间。 发送方进程可以将数据以系统调用的形式存放在内核空间（该内核空间具体是指PCB进程控制块）中，接收方进程同样以系统调用的方式在内核找到对应的数据，读取并返回。

* PCB中有关IPC的内容，都是针对接受方而言的，换言之，这些是接收方的PCB

  ```c
  //发送方改接收方的
  	u_int env_ipc_value;            // 发送方发给接收方的一个值
      u_int env_ipc_from;             // 发送方的envid  
      u_int env_ipc_perm;             // 接收到的物理页面的perm
  //接收方改接收方的
      u_int env_ipc_recving;          // 是否准备好接受数据
      u_int env_ipc_dstva;            // 接收到的物理页面和接收方的哪个地址对应
  ```


#### 内核系统调用

* 接收方内核系统调用：准备接收数据，设置接收地址，阻塞进程让给发送方

```c
//lib/syscall_all.c
void sys_ipc_recv(int sysno, u_int dstva)
{
    if(dstva >= UTOP) return ;
    curenv->env_ipc_recving = 1;    //接收方准备接受消息
    curenv->env_ipc_dstva = dstva;  //接收到的物理页面和哪个地址对应
    curenv->env_status = ENV_NOT_RUNNABLE;//阻塞接收方，让发送方发送数据
    sys_yield();//让出进程，给发送方
}

```



* 发送方系统调用：发送消息到`envid`进程，消息可以是一个数`value`，也可以是一个物理页面（将接收方虚拟地址`dstva`映射到对应的`srcva`对应的物理页面，若`srcva`为0，表示不映射物理页面，只传递`value`。）

```c
int sys_ipc_can_send(int sysno, u_int envid, u_int value, u_int srcva,
                     u_int perm)
{

    int r;
    struct Env *e;
    struct Page *p;
    if(srcva >= UTOP) return -E_INVAL;
    r = envid2env(envid, &e, 0);
    if(r<0) return r;
    if(e->env_ipc_recving == 0) return -E_IPC_NOT_RECV;//接收方处于接受状态否
	//对接收方进程进行操作
    e->env_ipc_recving = 0;
    e->env_ipc_from = curenv->env_id;
    e->env_ipc_value = value;
    e->env_ipc_perm = perm;
    e->env_status = ENV_RUNNABLE;
    //传递物理页面
    if(srcva != 0){
        p = page_lookup(curenv->env_pgdir, srcva, NULL);
        if(p == 0) return -E_INVAL;
        r = page_insert(e->env_pgdir, p, e->env_ipc_dstva, perm);
        if(r < 0) return r;
    }
    return 0;
}
```



#### 用户调用函数

1. 接收方调用函数。其中whom保存发送方的envid，dstva设置接收地址，perm为相关页面的权限位，返回值为传递的value。

   ```c
   u_int ipc_recv(u_int *whom, u_int dstva, u_int *perm)
   {
       //printf("ipc_recv:come 0\n");
       syscall_ipc_recv(dstva);
   
       if (whom) {//whom指针不为空
           *whom = env->env_ipc_from;//env为当前进程
       }
   
       if (perm) {//perm指针不为空
           *perm = env->env_ipc_perm;
       }
   
       return env->env_ipc_value;
   }
   ```

2. 发送方调用函数。发送方将val发送给whom，会一直尝试，直到成功为止。srcva为传递页面的虚拟地址，perm为接收方相应权限位的设置。

   ```c
   void ipc_send(u_int whom, u_int val, u_int srcva, u_int perm)
   {
       int r;
   
       while ((r = syscall_ipc_can_send(whom, val, srcva, perm)) == -E_IPC_NOT_RECV) {
           syscall_yield();//从这里可以看出，可以先调用ipc_send再调用ipc_recv
           //writef("QQ");
       }
       
       if (r == 0) {
           return;
       }
       
       user_panic("error in ipc_send: %d", r);
   }
   ```

### Fork

> fork 失败的情况下，子进程不会被创建，且父进程将得到小于 0 的返回值。
>
> 默认情况下，父进程退出后子进程不会被强制杀死。在操作系统看来，父子进程之间更像是兄弟关系。

 fork 时，操作系统会为新进程分配独立的虚拟地址空间。但是，子进程地址空间中的**代码段、数据段、堆栈**等都被映射到 父进程中**相同区段对应的页面**。虽然两者的**地址空间（页目录和页表）是不同**（这个不同是说页目录和页表 是不同的，不同进程不能共享整个页表，它们有不同的地址空间）的，但是它们此时还对应相同的物理内存。

> 虽然对应着相同的物理空间，但是父子进程在相应的页面权限上是PTE_COW，意为写时复制，当任意一方修改内存时，内核捕获页写入异常，异常处理时为**修改内存的进程**的地址空间中相应地址分配新的物理页面，并取消该虚拟地址的PTE_COW。

#### fork执行过程

##### fork

```c
//user/fork.c， 注意，此时在用户态下
int fork(void)
{
    u_int newenvid;
    extern struct Env *envs;
    extern struct Env *env;
    u_int i;

    //The parent installs pgfault using set_pgfault_handler
    set_pgfault_handler(pgfault);//见页写入异常

    newenvid = syscall_env_alloc();//子进程在此返回0，父进程在此返回子进程id。注意，子进程不会执行内核中的系统调用函数，其第一条执行的指令是在msyscall函数中syscall的后一条语句，jr ra
    if(newenvid == 0){
        //若为子进程返回,注意，开始执行这段代码时，父进程已经执行完fork函数了
        newenvid = syscall_getenvid();//当前进程为子进程，获取子进程的id
        env = &envs[ENVX(newenvid)];
        return 0;
    }   
    //为子进程映射页面，  
    for(i=0;i<VPN(USTACKTOP);i++){
        if(((*vpd)[i>>10] & PTE_V) && ((*vpt)[i] & PTE_V)){
            duppage(newenvid, i); // i is virtual page number, newenvid is the id of son 
        }   
    }   

    //为子进程异常栈分配物理空间，确保页写入异常的正确处理
    syscall_mem_alloc(newenvid, UXSTACKTOP - BY2PG, PTE_V | PTE_R);
    //为新进程注册页写入异常处理函数
    syscall_set_pgfault_handler(newenvid, __asm_pgfault_handler, UXSTACKTOP);
    //使子进程可以开始执行
    syscall_set_env_status(newenvid, ENV_RUNNABLE);
    return newenvid;
}   

```

##### set_pgfault_handler

为进程自身注册页写入异常处理函数，为进程的用户态页写入异常处理函数变量赋值；为进程异常处理栈分配空间；设置`env_pgfault_handler`域指向的异常处理函数。

```c
//user/pgfault.c
void set_pgfault_handler(void (*fn)(u_int va))//fn是一个函数指针，表示fn指向的函数返回值类型为void，参数有u_int va。注意，一般的函数名就是一个地址，指针。
{
    if (__pgfault_handler == 0) {//变量具体含义见下
        // 为异常处理栈分配一页空间
        // register assembly handler and stack with operating system
        if (syscall_mem_alloc(0, UXSTACKTOP - BY2PG, PTE_V | PTE_R) < 0 ||
            syscall_set_pgfault_handler(0, __asm_pgfault_handler, UXSTACKTOP) < 0) {
            //第一个参数0是envid，第二个参数是用户态下页写入异常处理函数地址，第三个参数为异常处理栈栈顶
            writef("cannot set pgfault handler\n");
            return;
        }

        //      panic("set_pgfault_handler not implemented");
    }
    // Save handler pointer for assembly to call.
    __pgfault_handler = fn;
}
```

* 变量`__pgfault_handler`的定义，初始值为0。值为用户态真正处理页写入异常的函数地址

  > 注意它是在entry.S中定义，有`.data`标识，表示这个变量在数据段定义，相当于用户程序里的全局变量。由于每个进程是独立的，因此这个变量不是每个进程共有的，而是不同进程有着不同的变量值。

  ```assembly
      #user/entry.S
      .globl __pgfault_handler
      __pgfault_handler:
      .word 0
  ```

* `sys_set_pgfault_handler`函数，为**envid**进程设置`env_pgfault_handler`(在这里为`__asm_pgfault_handler`)和`env_xstacktop`(在这里设置为`UXSTACKTOP`)域，即**设置该进程的异常处理栈和异常处理函数。**

  ```c
  int sys_set_pgfault_handler(int sysno, u_int envid, u_int func, u_int xstacktop)
  {
      // Your code here.
      struct Env *env;
      int ret;
      ret = envid2env(envid, &env, 0);
      if(ret < 0) return ret;
      env->env_pgfault_handler = func;//__asm_pgfault_handler
      env->env_xstacktop = xstacktop; //UXSTACKTOP
      return 0;
      //  panic("sys_set_pgfault_handler not implemented");
  }
  ```

  

##### syscall_env_alloc()

 是一个内联函数，编译器编译时直接将该函数的机器码插入到代码中。*这个函数并不会被编译为一个函数，而是直接内联展开在fork函数内，也就是说这个函数最后是以`msyscall`的形式出现在`fork`函数中。所以`syscall_env_alloc`的栈帧就不存在了， `msyscall` 函数直接返回到了fork函数内*。

```c
//user/lib.h
inline static int syscall_env_alloc(void)
{
    return msyscall(SYS_env_alloc, 0, 0, 0, 0, 0);
    //系统调用结束，在此返回，返回值有两种， 0——子进程
    //								  非0——父进程
}
```

* `sys_env_alloc()`，当前进程为父进程，复制出一个子进程，其PCB中env_tf存放父进程当前的上下文信息。只有父进程在这里会返回，返回值为子进程id。

  ```c
  int sys_env_alloc(void)
  {
      int r;
      struct Env *e;
      r = env_alloc(&e, curenv->env_id);//为子进程控制块分配地址空间，并初始化PCB。不放入调度队列
      if(r < 0) return r;
      
      struct Trapframe *old = (struct Trapframe *)(KERNEL_SP - sizeof(struct Trapframe)); 
      //复制一份当前进程上下文Trapframe 到子进程的进程控制块中，注意，系统调用发生后执行SAVE_ALL函数保存了进程上下文
      bcopy((void *)(old), &(e->env_tf), sizeof(struct Trapframe));
      //待子进程被调度时，执行env_run()时env_tf会被env_pop_tf恢复进程现场。
      
      e->env_tf.pc = e->env_tf.cp0_epc;//子进程开始运行的指令为syscall下一条指令，即从msyscall返回到syscall_env_alloc的指令
      e->env_status = ENV_NOT_RUNNABLE;
      e->env_tf.regs[2] = 0;           //这里确保子进程msyscall返回值为0，$v0
      e->env_pri = curenv->env_pri;      
      return e->env_id;      //返回子进程id
      //  panic("sys_env_alloc not implemented");
  }
  ```

##### duppage

将当前进程的虚拟页`pn`与`envid`同样的虚拟页进行映射。

```c
//user/fork.c， 注意，此时在用户态下
static void duppage(u_int envid, u_int pn)
{
    u_int addr;
    u_int perm;
    addr = pn << PGSHIFT;
    perm = (*vpt)[pn] & 0xfff;
    int flag = 0;
	//置PTE_COW的条件: perm可写PTE_R ; perm非共享。如果原来perm就是PTE_COW，那么子进程还是会PTE_COW
    if((perm & PTE_R) && !(perm & PTE_LIBRARY)){
        //共享可写，即父子进程映射到相同的物理页，并且修改的结果相互可见
        perm = perm | PTE_COW;
        flag = 1;
    }
    syscall_mem_map(0, addr, envid, addr, perm);
    if(flag) //子进程如果置PTE_COW，父进程也必须置PTE_COW位
        syscall_mem_map(0, addr, 0, addr, perm);
    //  user_panic("duppage not implemented");

}
```



##### sys_set_env_status

设置`envid`进程的`env_status`。如果env_status不为`ENV_RUNNABLE`并且`status`为`ENV_RUNNABLE`，则将其加入调度队列尾（当然，需要保证之前不在调度队列中）

```c
int sys_set_env_status(int sysno, u_int envid, u_int status)
{
    struct Env *env;
    int ret;
    if(status != ENV_FREE && status != ENV_RUNNABLE && status != ENV_NOT_RUNNABLE)
        return -E_INVAL;
    
    ret = envid2env(envid, &env, 0);
    if(ret < 0) return ret;
    if(status == ENV_RUNNABLE && env->env_status != ENV_RUNNABLE){//小心哪
        LIST_INSERT_TAIL(&env_sched_list[0], env, env_sched_link);
    }
    env->env_status = status;
    return 0;
}
```



#### 页写入异常

CPU 的**页写入异常**会在用户进程写入被标记为 PTE_COW 的页面时产生。经过异常分发进入异常处理函数`handle_mod`中。

##### handle_mod

* `handle_mod`，处理页写入异常，跳转到`page_fault_hanlder`函数

  ```assembly
  #lib/genex.S
  .align  5
  NESTED(handle_mod, TF_SIZE, sp)
  .set    noat
  SAVE_ALL
  CLI
  .set    at
  move    a0, sp  #这个sp为KERNEL_SP栈
  #将trapFrame传给函数
  jal page_fault_handler
  nop
  j   ret_from_exception
  nop
  END(handle_mod)
  ```


##### page_fault_handler

1. `page_fault_handler`，处理写时复制的内核函数。将当前现场保存在**异常处理栈**（栈顶为UXSTACKTOP）中，并设置trapframe里epc的值，使得从中断恢复后能够跳转到`env->env_pgfault_handler`域存储的异常处理函数的地址。

   ```c
   //lib/traps.c   处于内核态
   void page_fault_handler(struct Trapframe *tf)
   {
       struct Trapframe PgTrapFrame;
       extern struct Env *curenv;
   
       bcopy(tf, &PgTrapFrame, sizeof(struct Trapframe));
   	
       //将当前进程现场保存在异常处理栈
       if (tf->regs[29] >= (curenv->env_xstacktop - BY2PG) &&
           tf->regs[29] <= (curenv->env_xstacktop - 1)) {
           //若进程堆栈地址就位于异常处理栈
               tf->regs[29] = tf->regs[29] - sizeof(struct  Trapframe);
               bcopy(&PgTrapFrame, (void *)tf->regs[29], sizeof(struct Trapframe));
           } else {
               tf->regs[29] = curenv->env_xstacktop - sizeof(struct  Trapframe);
               bcopy(&PgTrapFrame,(void *)tf->regs[29],sizeof(struct Trapframe));
           }
       // TODO: Set EPC to a proper value in the trapframe
       tf->cp0_epc = curenv->env_pgfault_handler;//值为__asm_pgfault_handler
       return;
   }
   ```

   执行完后退出中断，跳转到`__asm_pgfault_handler`

##### __asm_pgfault_handler

1. `__asm_pgfault_handler`函数，处理写时复制问题的用户态汇编函数。**调用**真正处理写时复制的`pgfault`函数，并恢复进程现场。

   > 注意，下面的$sp为异常栈底，栈中存放着发生页写入异常时的进程上下文

   ```assembly
   #user/entry.S    处于用户态
   .globl __asm_pgfault_handler
   __asm_pgfault_handler:
       # save the caller-save registers
       # $sp的值为异常栈底，保存着刚发生异常的进程上下文
       /*
       此时$sp刚被ret_from_exception设置为KERNEL_SP栈的regs[29]，
       其值为异常栈底，栈存放着发生页写入异常时的进程上下文。(除了epc,regs[29]以外，异
       常栈和退出异常时的KERNEL_SP相同。在上一个函数我们将内核栈的epc设置为
       env_pgfault_handler)。使用时就当成最初KERNEL_SP栈的内容即可。BadVAddr寄存器保存着引发
       页写入异常的虚拟内存地址
       */
       lw  a0, TF_BADVADDR(sp)
       #__pgfault_handler变量存放着真正处理页写入异常的函数pgfault
       lw  t1, __pgfault_handler
       #跳转到pgfault函数
       jalr    t1
   
   nop
       #恢复现场，此时仍在用户态
       lw  v1,TF_LO(sp)
       mtlo    v1
       lw  v0,TF_HI(sp)
       #现在栈是异常处理栈，其epc存放是最初进入中断时KERNLE_SP的内容，也就是发生页写入异常的地址
       lw  v1,TF_EPC(sp)
       mthi    v0
       mtc0    v1,CP0_EPC
       lw  $31,TF_REG31(sp)
     	...
       lw  k0,TF_EPC(sp)   //atomic operation needed
       #跳转返回
       jr  k0
       #恢复进程原先的栈
       lw  sp,TF_REG29(sp)  /* Deallocate stack */
   ```


##### pgfault

1. `void pgfault(u_int va)`, 去除进程虚拟页的`PTE_COW`的标志，通过一个临时页，为虚拟地址va重新分配一个物理页，内容与原页相同。

   ```c
   //user/fork.c
   static void pgfault(u_int va)//va为引发页写入异常的虚拟内存地址
   {
       u_int *tmp = USTACKTOP;//临时页起始地址
       u_int perm =(*vpt)[VPN(va)] & 0xfff;
       //判断是否是页写入异常造成的， 否则报错 
       if((perm & PTE_COW) == 0){
           user_panic("pgfault in fork.c");
       }
       perm -= PTE_COW;//PTE_COW == 1
       
       //为临时页申请内存
       syscall_mem_alloc(0, tmp, perm);
       //拷贝内容到临时页
       user_bcopy(ROUNDDOWN(va, BY2PG), tmp, BY2PG);
       //为虚拟地址va重新映射物理页
       syscall_mem_map(0, tmp, 0, va, perm);
       //取消临时页面的映射关系 
       syscall_mem_unmap(0, tmp);
   }  
   ```

   

## 实验难点

1. 系统调用的过程，以syscall_putchar为例

   <img src="https://mdpics4lgw.oss-cn-beijing.aliyuncs.com/aliyun/202306221637808.svg" alt="lab4" style="zoom:67%;" />

2. 页写入异常设置流程，子进程和父进程都是在fork函数中设置。

   <img src="https://mdpics4lgw.oss-cn-beijing.aliyuncs.com/aliyun/202306221638980.png" alt="lab4-2" style="zoom:67%;" />

3. 页写入异常的处理过程

   CPU 的**页写入异常**会在用户进程写入被标记为 PTE_COW 的页面时产生。经过异常分发进入异常处理函数`handle_mod`中。

<img src="https://mdpics4lgw.oss-cn-beijing.aliyuncs.com/aliyun/202306221638993.png" alt="lab4-3" style="zoom:80%;" />



## 体会与感想

Lab4实验的fork函数感觉是本学期最难的一部分，做的过程中脑子很乱，做完之后再次整理才明白各函数究竟做了什么。但是助教王廉杰非常热心，提出的所有问题都会积极回答，以下的实验疑问如果没有王助教无法解决。

## 实验疑问

本次实验过程中，实验疑问非常多，但是目前都已解决。

1. 为什么exersize4.0让*`env_tf.cp0_status` 的值为 `0x1000100c`*呢？

   在gxemul中，KU~c~表示是否处于用户态，1表示处于用户态，这和之前学习的不太一样。 

2. syscall指令有什么用呢？

   异常指令，会产生8号系统调用异常。执行`msyscall`的函数会调用`syscall`，`msyscall`函数的6个参数由于保存到\$a0-$a3和栈中，可以直接取出来用。

3. MIPS调用规范？

   函数f调用函数g时，会为函数f自身的**局部变量、返回地址、函数g的参数**分配存储空间（叶函数没有后两者），在函数调用结束之后会对栈指针做加法（弹栈）来释放这部分空间，我们把 这部分空间称为**栈帧（Stack Frame）**

   临时寄存器被调用函数使用时不需要保存，由调用函数保存。

4. 如果想要增加一个系统调用，需要被修改的文件有：

   * `lib/syscall.S`，存放sys_call_table，保存`sys_*`函数的地址

* `include/unistd.h`, 保存各系统调用的系统调用号
  * `lib/syscall_all.c`，实现所有`sys_*`函数（内核中的系统调用函数）
* `user/syscall_lib.c`，实现所有`syscall_*`函数（用户系统调用函数）
  * `user/lib.h`，声明所有`syscall_*`函数

4. 处理系统调用时的内核仍然是代表当前进程的，如何理解？

   在内核处理系统调用时，并没有切换 CPU 的地址空间（页目录地址），最后也没有将进程上下文（`Trapframe`）保存到进程控制块`env_tf`中，只是切换到内核态下， 执行了一些内核代码。执行完后也没有进行`sched_yield()`进行进程的切换，而是通过`ret_from_exception`返回，并没有执行`env_run()`函数。

   而发生时钟中断时，切换进程会执行`env_run()`函数，其会执行`lcontext()`函数切换到新进程的页目录地址，而且会将进程上下文保存到旧进程控制块中，再切换到新进程。

5. user/libos.c 的实现中，用户程序在运行时入口会将一个用户空间中的指针变量 `struct Env *env` 指向当前进程的控制块，如何理解？

   `user/user.lds`指明了用户进程的入口地址为`_start`，`lds` 会把用户程序里的 `_start` 链接到 UTEXT

   注意，`entry_point`就是`UTEXT`，值为`0x400000`。

   * **_start**，用户进程初始从这里开始执行

   ```assembly
   #user/entry.S 用户态
   	.text
       .globl _start
   _start:
       lw  a0, 0(sp)#$sp 此时为 USTACKTOP，这两句话没用，lab6才有用
       lw  a1, 4(sp)
       jal libmain
       nop
   ```

   * **libmain**函数，用户进程入口的 C 语言部分，负责完成执行用户程序 umain 前后 的准备和清理工作，是我们这次需要了解的函数之一。

   ```c
   //user/libos.c
   void exit(void)
   {
       syscall_env_destroy(0);
   }
   
   struct Env *env;
   
   void libmain(int argc, char **argv)
   {
       // set env to point at our env structure in envs[].
       env = 0;    // Your code here.
       //writef("xxxxxxxxx %x  %x  xxxxxxxxx\n",argc,(int)argv);
       int envid;
       envid = syscall_getenvid();
       envid = ENVX(envid);
       env = &envs[envid];
       // call user main routine
       umain(argc, argv);//用户程序开始执行，在本lab里它是一个测试函数
       // exit gracefully
       exit();
   }
   ```

   这里需要一个`env`变量的原因是有些函数需要`env`，如`ipc.c`。

6. **note 4.6**是什么意思

   在堆栈所在页面设置PTE_COW之前，在`for`和`if`之间、`duppage`函数调用的过程中，可能会对堆栈进行操作，而此时由于父进程已经从`syscall_env_alloc`返回，故对应的调用栈帧也就销毁了，很可能其保存的函数返回地址被覆盖。因此把`syscall_env_alloc`函数设置成内联函数。

7. `duppage`函数为什么必须保证子进程先映射PTE_COW，之后父进程才能映射？

   因为执行duppage时，仍处于用户态，如果先为父进程页面置PTE_COW，那么如果这个页面是堆栈的话，执行之后的syscall_*函数可能会改变这个堆栈产生页写入异常，进而父进程该页面消除PTE_COW标记，而子进程会有PTE_COW标记。

   这就导致如果父进程之后修改 了这个 页面，那么不会产生页写入异常，子进程会同时修改，产生问题。

   另外，经过实验验证，如果除了堆栈以外的映射都是父进程在前，子进程在后，是正确的。

   ```c
   //duppage函数...
       if((perm & PTE_R) && !(perm & PTE_LIBRARY)){
           perm = perm | PTE_COW;
           //flag = 1; 
           if(addr != USTACKTOP - BY2PG){
               syscall_mem_map(0, addr, 0, addr, perm);
               syscall_mem_map(0, addr, envid, addr, perm);
           }   
           else {
               syscall_mem_map(0, addr, envid, addr, perm);
               syscall_mem_map(0, addr, 0, addr, perm);
           }   
   
       } else {
           syscall_mem_map(0, addr, envid, addr, perm);
       }
   ```

8. 为什么Lab3的load_icode函数映射的页面的权限一定要PTE_R

   之后对.data和.bss进行修改时，如果不为PTE_R，无法修改。

9. 异常处理栈处理完一个异常之后会恢复吗？

   不用恢复，每次发生异常会重新设置栈顶。设置位置是在page_fault_handler函数

    注意`__pgfault_handler`是在entry.S中定义，有`.data`标识，表示这个变量在数据段定义，相当于用户程序里的全局变量。由于每个进程是独立的，因此这个变量不是每个进程共有的，而是不同进程有着不同的变量值。

10. 如果延迟槽指令发生异常，epc中存储的是跳转指令的地址，cause寄存器最高位为1