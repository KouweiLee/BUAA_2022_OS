# Lab4-Challenge实验报告

20373159李国玮

**目录：**

[TOC]



## 一、实现思路

### 线程

#### 任务要求

1. 以线程作为处理机的调度单位，进程作为拥有资源的基本单元
2. 提供对线程的创建、撤销等用户态接口函数，满足POSIX标准：
   * *pthread_create()* ：创建线程
   * *pthread_exit()* ：终止线程 
   * *pthread_cancel()* ：撤销线程 
   * *pthread_join()* ：阻塞至线程结束
   * *pthread_detach()*：实现线程分离
   * *pthread_setcancelstate()*:  设置cancelability state
   * *pthread_setcanceltype()*：设置cancelability type
   * *pthread_testcancel()*：产生一个cancellation point
3. 线程栈空间独立，地址空间共享。

用户态的四个接口函数可以在POSIX标准中找到，接口函数的要求是：

1. **pthread_create**，创建线程，`*thread`为线程id，`start_routine`为线程创建后执行的函数地址，`arg`为该函数参数，`attr`我们没有实现。

```c
int pthread_create(pthread_t * thread, const pthread_attr_t * attr,
                   void * (*start_routine)(void *), void *arg);
```

2. **pthread_exit**，线程退出，该函数由线程自己在程序末尾调用，结束执行。`*retval`可以作为一个返回值，传递给执行了`join`的线程。

```c
void pthread_exit(void *retval);
```

3. **pthread_cancel**，一个线程通过该函数向另一个线程发送“终止执行”的信号，从而令目标线程结束执行。

```c
int pthread_cancel(pthread_t thread);
```

> 参数 thread 用于指定被取消的线程。另外，一个线程是否会被取消**取决于**该线程的`cancelability state and type.`
>
> 执行该函数时，首先检查state，如果state为enabled（默认），则检查type。如果为disabled，则cancel请求排队直到state为enabled。state可以通过函数**pthread_setcancelstate**设置
>
> 其次检查type，可能为asynchronous或deferred（默认）。如果为前者则立即取消，如果为deferred则直到线程调用cancellation point函数才会cancel。type可以通过**pthread_setcanceltype**函数修改。
>
> 本任务中cancellation point为`pthread_testcancel`
>
> 被取消的线程相当于执行`pthread_exit(PTHREAD_CANCELED);`，也就是说，他会给`join`到自己的函数返回一个`PTHREAD_CANCELED`的返回值。
>

4. **pthread_join**，等待线程执行结束，并获取线程执行结束时返回的数据。

```c
int pthread_join(pthread_t thread, void **retval);
```

thread 参数用于指定接收哪个线程的返回值；`*retval` 参数表示接收到的返回值，如果 thread 线程没有返回值，又或者我们不需要接收 thread 线程的返回值，可以将 retval 参数置为 NULL。

另外，一个线程执行结束的返回值**只能由一个** pthread_join() 函数获取，其他线程只有返回失败。

同时，为了拓展`pthread_join`函数，我们还实现了`pthread_detach`函数，用于实现线程的分离。

5. **pthread_detach**，实现线程与其他线程的分离，不能被别的线程加入(`join`)

```c
int pthread_detach(pthread_t thread);
```

明确任务要求后，根据要求实现线程控制块。

#### 线程控制块

线程控制块是记录线程状态的数据结构。类比进程，当引入线程后，线程成为处理机调度的基本单位，进程为管理资源的基本单位，因此，将上下文`tc_tf`、状态`tcb_status`、优先级`tcb_pri`等进程的特性移入线程中；同时仿照进程的调度设置线程调度队列`tcb_sched_list[2]`。以下是线程控制块TCB的内容，TCB的其余部分下文会涉及。

```c
struct Tcb {
    // status information
    struct Trapframe tcb_tf;
    u_int tcb_id;
    u_int tcb_status;
    u_int tcb_pri;
    LIST_ENTRY(Tcb) tcb_sched_link;                 
    
    // pthread_join information
    struct Tcb* tcb_joinedtcb;//save the tcb joining to it
    void **tcb_join_value_ptr;//save the other tcb's exit value
    u_int tcb_detach;         //whether tcb was detached  

    // pthread_exit information
    void *tcb_exit_ptr;   //save the exit value

    // pthread_cancel information
    int tcb_cancelstate; //cancel state
    int tcb_canceltype;  //cancel type
    u_int tcb_canceled;  //whether the cancel signal was received
};
```

添加线程控制块后，还需要修改进程控制块，基于已有架构设计，通过设置`env_threads[16]`域，进程可以直接管理线程，线程也可以固定数量常驻内存。根据UENVS的大小，每个进程最多支持16个线程。设置进程控制块大小为1页是为了可以通过`ROUNDDOWN(tcb, BY2PG)`可以快速找到线程所属的进程。

```c
struct Env {
    LIST_ENTRY(Env) env_link;       // Free list
    u_int env_id;                   // Unique environment identifier
    u_int env_parent_id;            // env_id of this env's parent
    Pde  *env_pgdir;                // Kernel virtual address of page dir
    u_int env_cr3;

    // Lab 4 IPC
    u_int env_ipc_waiting_thread_no;
    u_int env_ipc_value;            // data value sent to us 
    u_int env_ipc_from;             // envid of the sender  
    u_int env_ipc_recving;          // env is blocked receiving
    u_int env_ipc_dstva;        // va at which to map received page
    u_int env_ipc_perm;     // perm of page mapping received

    // Lab 4 fault handling
    u_int env_pgfault_handler;      // page fault state
    u_int env_xstacktop;            // top of exception stack

    // Lab 6 scheduler counts
    u_int env_runs;         // number of times been env_run'ed

    // Lab 4 challenge
    u_int env_thread_count;

    // keep bytes
    u_int env_nop[192];                  // align to avoid mul instruction

    // Lab 4 challenge
    struct Tcb env_threads[16];
};
```

还有一些涉及到的工具函数和宏：

1. `mktcbid`函数。为了根据tcb_id可以找到相应的进程和线程，tcb_id的低4位为其在`env_threads`数组中的下标。其余位为env_id，便于找到相应的`env`。

   ```c
   u_int mktcbid(struct Tcb *t, u_int thread_no){
       struct Env *e = TCB2ENV(t);
       return ((e->env_id << 4) | TCBX(thread_no));
   }    
   ```

2. 宏

   ```c
   #define TCB2ENV(t) ROUNDDOWN(t, BY2PG)//由于env大小为一页，故可根据该宏从线程找到进程
   #define TCBX(t) (t & 0xf)//得到线程id的低4位
   #define TCBE(t) (t >> 4) //得到env_id
   ```

   

#### 线程创建

线程创建有两种方式，第一种通过`ENV_CREATE`创建进程后自动创建主线程；第二种为通过用户接口函数`pthread_create`创建子线程。

##### 进程创建主线程

ENV_CREATE通过函数`env_create_priority`创建进程。由于调度单位是线程，修改相应部分即可。

```c
void env_create_priority(u_char *binary, int size, int priority)
{
    struct Env *e;

    if(env_alloc(&e, 0) != 0) return;                  

    e->env_threads[0].tcb_pri = priority;

    load_icode(e, binary, size);

    LIST_INSERT_HEAD(&tcb_sched_list[0], &e->env_threads[0], tcb_sched_link);
}
```

* **env_alloc**函数，基本和原来的函数相同。创建进程时需要同时创建主线程，因此调用函数`thread_alloc`创建主线程。

  ```c
  int env_alloc(struct Env **new, u_int parent_id)        
  {
      int r;
      struct Env *e;
      struct Tcb *t;
  
      if(LIST_EMPTY(&env_free_list)){
          *new = NULL;
          return -E_NO_FREE_ENV;
      }
      e = LIST_FIRST(&env_free_list);
  
      env_setup_vm(e);
  
      e->env_id = mkenvid(e);
      e->env_parent_id = parent_id;
      e->env_thread_count = 0;
      if((r = thread_alloc(e, &t)) < 0)
          return r;
  
      LIST_REMOVE(e, env_link);
      *new = e;
      return 0;
  }             
  ```

* **thread_alloc**函数，找到进程中空闲且可用的线程控制块，并进行初始化。

  ```c
  int thread_alloc(struct Env *e, struct Tcb **new) {
      if (e->env_thread_count >= THREAD_MAX) 
          return -E_THREAD_MAX;
      u_int i;
      for(i = 0;i < THREAD_MAX; i++){
          if(e->env_threads[i].tcb_status == ENV_FREE && e->env_threads[i].tcb_exit_ptr == 0)
              break;
      }       
      if(i == THREAD_MAX)
          return -E_THREAD_MAX;
      ++(e->env_thread_count);
      struct Tcb *t = &e->env_threads[i];
      t->tcb_id = mktcbid(t, i);
      printf("thread id is %x\n", t->tcb_id);
      t->tcb_status = ENV_RUNNABLE;
      //initialize join information
      t->tcb_joinedtcb = 0;
      t->tcb_join_value_ptr = 0;
      t->tcb_detach = 0;
      
      t->tcb_exit_ptr = (void *)0;
      //initialize cancel information
      t->tcb_cancelstate = PTHREAD_CANCEL_ENABLE;
      t->tcb_canceltype = PTHREAD_CANCEL_DEFERRED;
      t->tcb_canceled = 0;
      //initialize tcb_tf 
      t->tcb_tf.cp0_status = 0x1000100c;
      t->tcb_tf.regs[29] = USTACKTOP - TCB_STACK(TCBX(t->tcb_id));
      
      *new = t;
      return 0;
  }           
  ```

##### 接口函数创建子线程

当进程调用函数`pthread_create`, 可创建子进程。子进程执行函数`start_rountine`，当函数执行结束时，跳转到`son_exit`函数退出。

```c
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void * (*start_rountine)(void *), void *arg) {
    int newthread = syscall_thread_alloc();
    if (newthread < 0) {
        *thread = 0;
        return newthread;
    }
    struct Tcb *t = &env->env_threads[TCBX(newthread)];
    t->tcb_tf.regs[29] = USTACKTOP - TCB_STACK(TCBX(newthread));
    t->tcb_tf.pc = start_rountine;
    t->tcb_tf.regs[29] -= 4;//according to MIPS, parameter should be placed in stack
    t->tcb_tf.regs[4] = arg;//a0 stores arg
    t->tcb_tf.regs[31] = son_exit;//when function finished, return to son_exit
    syscall_set_thread_status(t->tcb_id,ENV_RUNNABLE);  
    *thread = t->tcb_id;
    return 0;
}
```

* **syscall_thread_alloc**函数，

  ```c
  int sys_thread_alloc(void)
  {
      int r;
      struct Tcb *t;
  
      r = thread_alloc(curenv, &t);
      if(r<0) return r;
      t->tcb_pri = curenv->env_threads[0].tcb_pri;
      t->tcb_status = ENV_NOT_RUNNABLE;
      t->tcb_tf.regs[2] = 0;
      return t->tcb_id;                                           
  }
  ```

* **sys_set_thread_status**，如果设置为RUNNABLE, 则加入调度队列；否则，移出调度队列

  ```c
  int sys_set_thread_status(int sysno, u_int threadid, u_int status)
  {   
      struct Tcb *t;
      int r; 
      if (status != ENV_RUNNABLE && status != ENV_NOT_RUNNABLE && status != ENV_FREE)
          return -E_INVAL;
      r = threadid2tcb(threadid,&t);
      if (r < 0)
          return r;
      
      if (status == ENV_RUNNABLE && t->tcb_status != ENV_RUNNABLE) {
          LIST_INSERT_TAIL(&tcb_sched_list[0] , t, tcb_sched_link);
      } else if(status != ENV_RUNNABLE && t->tcb_status == ENV_RUNNABLE) {
          LIST_REMOVE(t,tcb_sched_link);
      }
      
      t->tcb_status = status;
      return 0;
  }  
  ```

#### 线程退出

线程的退出分有许多情况：

1. 线程执行`exit()`函数退出：`exit`函数为进程退出函数，当一个线程执行时所有进程内的线程全部退出执行，主线程不加`pthread_exit`退出时则自动执行`exit`函数。

* **exit**函数

  ```c
  //user/libos.c
  void exit(void)
  {
      syscall_env_destroy(0);                                         
  }
  ```

* **sys_env_destroy**函数调用**env_destroy**函数，再调用**env_free**函数，这三个函数与Lab4类似，只是env_free函数中加入删除线程的语句

  ```c
  void env_free(struct Env *e){
      ...
          for(i=0; i<THREAD_MAX; i++){
              struct Tcb *t = &e->env_threads[i];
              if(t->tcb_status != ENV_FREE){
                  t->tcb_status = ENV_FREE;
                  t->tcb_exit_ptr = 0;
              }
          }
      ...
  }
  ```

2. 线程执行`pthread_exit`函数退出，该函数不会影响到别的线程。

   ```c
   void pthread_exit(void *value_ptr) {
       u_int threadid = pthread_self();
       struct Tcb *t = &env->env_threads[TCBX(threadid)];
       t->tcb_exit_ptr = value_ptr;
       syscall_thread_destroy(threadid);
   }     
   ```

* **sys_thread_destroy**函数，释放被join的线程，调用`thread_destroy`函数。

  ```c
  int sys_thread_destroy(int sysno, u_int threadid)
  {
      int r;
      struct Tcb *t;
      if ((r = threadid2tcb(threadid,&t)) < 0) {
          return r;
      }
      if (t->tcb_status == ENV_FREE) {
          return -E_INVAL;
      }
      struct Tcb *joinedtcb = t->tcb_joinedtcb;
      if(joinedtcb != 0){
          if(joinedtcb->tcb_join_value_ptr != 0)
              *(joinedtcb->tcb_join_value_ptr) = t->tcb_exit_ptr;
          sys_set_thread_status(0,joinedtcb->tcb_id,ENV_RUNNABLE);
      }
      printf("[%08x] destroying tcb %08x\n", curenv->env_id, t->tcb_id);
      thread_destroy(t);
      return 0;
  }    
  ```

* **thread_destroy**函数，完成

  ```c
  void thread_destroy(struct Tcb *t) {
      if (t->tcb_status == ENV_RUNNABLE)
          LIST_REMOVE(t,tcb_sched_link);
      thread_free(t);
      if (curtcb == t) {
          curtcb = NULL;
          bcopy((void *)KERNEL_SP - sizeof(struct Trapframe),
              (void *)TIMESTACK - sizeof(struct Trapframe),
              sizeof(struct Trapframe));
          sched_yield();
      }
  }
  ```

* **thread_free**函数，如果进程的线程全部结束了，那么销毁进程，调用`env_free`函数

  ```c
  void thread_free(struct Tcb *t)
  {
      int i;
      struct Env *e = ROUNDDOWN(t,BY2PG);
      --e->env_thread_count;
      t->tcb_status = ENV_FREE;
      printf("i am thread no.%d, tcbid is %x, i am killed ... \n",TCBX(t->tcb_id), t->tcb_id);
      if(t->tcb_detach == 1){                   
          u_int sp = USTACKTOP - TCB_STACK(TCBX(t->tcb_id));
          for(i = 1; i <= TCB_SNUM; ++i) {
              sys_mem_unmap(0, e->env_id, sp-i*BY2PG);
          }
          t->tcb_exit_ptr = 0;
      }
      if (e->env_thread_count <= 0) {
          env_free(e);
      }
  }
  ```

3. 子线程未执行`pthread_exit`，正常退出，则会执行`son_exit`。`son_exit`将程序的返回值以参数的形式传递给`son_exit_final`函数。

   ```c
   //user/son_exit.S
   LEAF(son_exit)
       move a0, v0
       j son_exit_final
       nop
   END(son_exit)
   
   //user/libos.c
   void son_exit_final(void *exit_ptr){
       struct Tcb *t = &env->env_threads[TCBX(syscall_getthreadid())];
       t->tcb_exit_ptr = exit_ptr;
       syscall_thread_destroy(0);
   }
   ```

#### 线程加入

**pthread_join**函数，线程调用后等到接受到thread线程的返回值value_ptr后，才继续往下执行。之所以弄成系统调用的形式，是要保证原子性，因为用户态下执行`syscall_set_thread_status`后，再执行`syscall_yield`可能出现此线程永远不会被调度的情况。

```c
int pthread_join(pthread_t thread, void **value_ptr) {
    int r = syscall_thread_join(thread,value_ptr);
    return r;
}
```

新版实现

```c
int pthread_join(pthread_t thread, void **value_ptr) {
    struct Tcb *t;
    int r; 
    t = &env->env_threads[TCBX(thread)];
    if(t->tcb_id != thread){
        return -E_BAD_TCB;
    }   
    if (t->tcb_detach || t->tcb_joinedtcb != 0) {
        return -E_INVAL; 
    }   
    if (t->tcb_status == ENV_FREE) {
        if (value_ptr != 0 && t->tcb_exit_ptr != 0) {//tcb_exit_ptr为0表示
            *value_ptr = t->tcb_exit_ptr;
            t->tcb_exit_ptr = 0;
        }   
        return 0;
    }   
    if(tcb->tcb_joinedtcb == t){//造成死锁
        return -E_BAD_TCB;//E_BAD_TCB 
    }   
    t->tcb_joinedtcb = tcb;
    tcb->tcb_join_value_ptr = value_ptr;
    syscall_set_thread_status(0,tcb->tcb_id,ENV_NOT_RUNNABLE);
    syscall_yield();
    return 0;    
}
```



* **sys_thread_join**，该函数成功执行后不会返回，而是当目标线程退出后，该线程返回到系统调用的下一条指令。

  ```c
  int sys_thread_join(int sysno, u_int threadid, void **value_ptr)
  {
      struct Tcb *t;
      int r; 
      t = &curenv->env_threads[TCBX(threadid)];
      if(t->tcb_id != threadid){
          return -E_BAD_TCB;
      }   
      if (t->tcb_detach || t->tcb_joinedtcb != 0) {
          return -E_INVAL; 
      }   
      if (t->tcb_status == ENV_FREE) {
          if (value_ptr != 0 && t->tcb_exit_ptr != 0) {//tcb_exit_ptr为0表示
              *value_ptr = t->tcb_exit_ptr;
              t->tcb_exit_ptr = 0;
          }   
          return 0;
      }   
      if(curtcb->tcb_joinedtcb == t){//造成死锁
          return -E_BAD_TCB;//E_BAD_TCB 
      }   
      t->tcb_joinedtcb = curtcb;
      curtcb->tcb_join_value_ptr = value_ptr;
      sys_set_thread_status(0,curtcb->tcb_id,ENV_NOT_RUNNABLE);
      struct Trapframe *trap = (struct Trapframe *)(KERNEL_SP - sizeof(struct Trapframe));
      trap->regs[2] = 0;
      trap->pc = trap->cp0_epc;
      sys_yield();
      return 0;    
  }                         
  ```

* 当线程退出时，如果有正在`join`的线程，则释放该线程。函数`syscall_thread_destroy`保证该功能

#### 线程独立

为了拓展`pthread_join`函数，实现了`pthread_detach`函数，可以实现线程的独立。独立的线程不能被其他线程加入，且执行结束后会释放所占有的资源。

```c
int pthread_detach(pthread_t thread) {
    struct Tcb *t = &env->env_threads[TCBX(thread)];
    int r;
    int i;
    if (t->tcb_id != thread) {
        return -E_BAD_TCB;
    }
    if (t->tcb_status == ENV_FREE) {
        t->tcb_exit_ptr = 0;
        u_int sp = USTACKTOP - TCB_STACK(TCBX(thread));
        for(i = 1; i <= TCB_SNUM; ++i) {
            r = syscall_mem_unmap(0,sp-i*BY2PG);
            if (r < 0)
                return r;
        }
        user_bzero(t,sizeof(struct Tcb));
    } else {
        t->tcb_detach = 1;
    }
    return 0;
}
```



#### 线程取消

**pthread_cancel**函数，一个线程通过` pthread_cancel()` 函数向另一个线程发送“终止执行”的信号，从而令目标线程结束执行。线程是否取消看线程自身的state和type。线程默认的state为可以取消，type为延迟取消。

```c
int pthread_cancel(pthread_t thread) {
    struct Tcb *t = &env->env_threads[TCBX(thread)];
    if (t->tcb_id != thread || t->tcb_status == ENV_FREE) {
        return -E_BAD_TCB;
    }
    if (t->tcb_cancelstate == PTHREAD_CANCEL_DISABLE) {//cancel state disable
        return -E_THREAD_CANNOTCANCEL;
    }
    t->tcb_exit_ptr = PTHREAD_CANCELED;
    if (t->tcb_canceltype == PTHREAD_CANCEL_ASYNCHRONOUS) {//cancel type asynchronous
        syscall_thread_destroy(thread);
    } else {
        t->tcb_canceled = 1;//get cancel signal, cancel when arriving at cancellation point
    }
    return 0;
}

```

**pthread_setcancelstate**函数，设置state，返回oldvalue。

```c
int pthread_setcancelstate(int state, int *oldvalue) {
    u_int threadid = pthread_self();
    struct Tcb *t = &env->env_threads[TCBX(threadid)];
    if ((state != PTHREAD_CANCEL_ENABLE) & (state != PTHREAD_CANCEL_DISABLE)) {
        return -E_INVAL;
    }
    if (t->tcb_id != threadid) {
        return -E_BAD_TCB;
    }
    if (oldvalue != 0) {
        *oldvalue = t->tcb_cancelstate;
    }
    t->tcb_cancelstate = state;
    return 0;
}
```

**pthread_setcanceltype**函数，设置type，返回oldvalue

```c
int pthread_setcanceltype(int type, int *oldvalue) {
    u_int threadid = pthread_self();
    struct Tcb *t = &env->env_threads[TCBX(threadid)];
    if (type != PTHREAD_CANCEL_ASYNCHRONOUS && type != PTHREAD_CANCEL_DEFERRED) {
        return -E_INVAL;
    }
    if (t->tcb_id != threadid) {
        return -E_BAD_TCB;
    }
    if (oldvalue != 0) {
        *oldvalue = t->tcb_canceltype;
    }
    t->tcb_canceltype = type;
    return 0;
}
```

当线程执行函数**pthread_testcancel**，则产生一个`cancellation point`。如果线程收到取消信号并且是可取消状态，则取消该线程，退出值存放在`PTHREAD_CANCELED`中。

```c
void pthread_testcancel() {
    u_int threadid = pthread_self();
    struct Tcb *t = &env->env_threads[TCBX(threadid)];
    if (t->tcb_id != threadid) {
        user_panic("panic at pthread_testcancel!\n");
    }
    if (t->tcb_canceled && t->tcb_cancelstate == PTHREAD_CANCEL_ENABLE) {
        t->tcb_exit_ptr = PTHREAD_CANCELED;
        syscall_thread_destroy(t->tcb_id);
    }
}
```

### 信号量

需要实现：

* sem_init() ：初始化信号量 
* sem_destroy() ：销毁信号量 
* sem_wait() ：对信号量的 P 操作（阻塞） 
* sem_trywait() ：对信号量的 P 操作（非阻塞） 
* sem_post() ：对信号量的 V 操作 
* sem_getvalue() ：读取信号量的值

因此，要实现信号量机制，首先要有能保存该信号量的值、状态、阻塞队列的数据结构，如下所示：

```c
struct sem {
    // basic information
    int sem_value;       //value of sem
    int sem_status;      //the status of sem
    int sem_wait_count;  //the num of waiting threads
    
    // wait_queue information
    u_int sem_head_index;//specifies the newly blocked thread position
    u_int sem_tail_index;//specifies the exit position
    struct Tcb *sem_wait_list[SEM_MAXNUM];// queue to store waiting threads
};
```

#### sem_init

完成信号量各域的初始化。

```c
int sem_init(sem_t *sem,int shared,unsigned int value) 

{
    if (sem == 0) {
        return -E_SEM_ERROR;
    }
    sem->sem_head_index = 0;
    sem->sem_tail_index = 0;
    sem->sem_value = value;
    sem->sem_status = SEM_VALID;
    sem->sem_wait_count = 0;
    int i;
    for(i = 0; i < SEM_MAXNUM; ++i) {
        sem->sem_wait_list[i] = 0;
    }
    return 0;
}    
```

#### sem_wait

相当于执行P操作。为了保证执行`wait`操作的原子性，采用系统调用的形式。

```c
int sem_wait(sem_t *sem) {
    return syscall_sem_wait(sem);
}

int sys_sem_wait(int sysno, sem_t *sem)
{
    if (sem->sem_status == SEM_FREE) {
        return -E_SEM_ERROR;
    }
    int i;
    if (sem->sem_value > 0) {
        --sem->sem_value;
        return 0;
    }
    //需要阻塞
    if (sem->sem_wait_count >= SEM_MAXNUM) {
        return -E_SEM_ERROR;
    }
    sem->sem_wait_list[sem->sem_head_index] = curtcb;
    sem->sem_head_index = (sem->sem_head_index + 1) % SEM_MAXNUM;
    ++sem->sem_wait_count;
    sys_set_thread_status(0,0,ENV_NOT_RUNNABLE);
    //保证进程恢复执行时，执行msyscall的下一条指令
    struct Trapframe *trap = (struct Trapframe *)(KERNEL_SP - sizeof(struct Trapframe));
    trap->regs[2] = 0;
    trap->pc = trap->cp0_epc;          
    sys_yield();
    return -E_SEM_ERROR;
}
```

#### sem_trywait

非阻塞式P操作，如果信号量的值为0，则返回错误值`E_SEM_EAGAIN`，故使用时需要判断返回值。

```c
int sem_trywait(sem_t *sem) {
	return syscall_sem_trywait(sem);
}

int sys_sem_trywait(int sysno, sem_t *sem)
{
	if (sem->sem_status == SEM_FREE) {
		return -E_SEM_ERROR;
	}
	if (sem->sem_value > 0) {
		--sem->sem_value;
		return 0;
	}
	return -E_SEM_EAGAIN;
}
```

#### sem_post

**sem_post**函数，对信号量实现解锁，如果信号量大于0，则加一；如果等于0，则如果有正在阻塞的线程，则释放该线程并占有锁；如果没有等待的线程，则加一。

```c
int sem_post(sem_t *sem) {
    return syscall_sem_post(sem);
}

int sys_sem_post(int sysno, sem_t *sem)
{
    if (sem->sem_status == SEM_FREE) {
        return -E_SEM_ERROR;
    }
    if (sem->sem_value > 0) {
        ++sem->sem_value;
    } else {
        if (sem->sem_wait_count == 0) {
            ++sem->sem_value;
        }
        else {
            struct Tcb *t;
            --sem->sem_wait_count;
            t = sem->sem_wait_list[sem->sem_tail_index];
            sem->sem_wait_list[sem->sem_tail_index] = 0;
            sem->sem_tail_index = (sem->sem_tail_index + 1) % SEM_MAXNUM;
            sys_set_thread_status(0,t->tcb_id,ENV_RUNNABLE);
        }
    }
    return 0;
}     
```

#### sem_getvalue

**sem_getvalue**函数，通过valp返回当前信号量的值。如果有正在阻塞的进程，则返回0。

```c
int sem_getvalue(sem_t *sem,int *valp) {
	return syscall_sem_getvalue(sem,valp);
}

int sys_sem_getvalue(int sysno, sem_t *sem, int *valp)
{
    if (sem->sem_status == SEM_FREE) {
        return -E_SEM_ERROR;
    }
    if (valp != 0) {
        *valp = sem->sem_value;
    }
    return 0;
}
```

#### sem_destroy

**sem_destroy**函数，并不会释放还在被阻塞的线程。

```c
int sem_destroy(sem_t *sem) {
	return syscall_sem_destroy(sem);	
}

int sys_sem_destroy(int sysno,sem_t *sem)
{
    if (sem->sem_envid != curenv->env_id && sem->sem_shared == 0) {
        return -E_SEM_NOTFOUND;
    }
    if (sem->sem_status == SEM_FREE) {
        return 0;
    }
    sem->sem_status = SEM_FREE;
    return 0;
}          
```

## 二、功能测试

### 线程功能测试

测试文件均在`user/`目录下。分开测试均没有问题，进程的组合测试也没有问题。同时，Lab4原有的课下测试也都没有问题。

1. `pthread_create`函数的正确性。在文件`mycreatetest.c`。基本原理：创建15个子线程，并传入参数，通过输出判断参数是否正常传递。之后通过信号量机制，检测所有线程是否创建完毕并输出，然后线程统一依次退出，并输出相关信息。

   ```c
   #include "lib.h"
   sem_t mutex;
   void *test(int *args){
       pthread_t threadid = pthread_self();
       int i;
       writef("son%d is begin\n", TCBX(threadid));
       writef("son%d's arg is %d\n",TCBX(threadid),args[TCBX(threadid)]);
       sem_wait(&mutex); 
       args[0]--;
       sem_post(&mutex);
       while(1){                                                               
           sem_wait(&mutex);
           if(args[0]==0){
           sem_post(&mutex);
           break;
           }
           sem_post(&mutex);
           syscall_yield();
       }
   }
   
   void umain(){
       pthread_t threads[15], tmp;
       int args[16];
       int i, r;
       sem_init(&mutex, 0, 1);
       for(i=0;i<16;i++){
           args[i] = i;
       }
       args[0] = 15;//子线程的数量
       for(i=0; i<15;i++){
           r = pthread_create(&threads[i], NULL, test, (void *)args);
           if(r < 0)
               user_panic("create is wrong");
           syscall_yield();
       }
       pthread_exit(NULL);
   }
   ```

   运行结果：从`son1`到`son15`依次输出，再逐一乱序被销毁。省去部分无关紧要的输出。由于`writef`函数并不是原子的，因此出现了输出被打乱的情况。

   ```
   thread id is 4000
   
   thread id is 4001
   
   son1 is begin
   
   son1's arg is 1
   
   thread id is 4002
   
   son2 is begin
   
   son2's arg is 2
   ...
   
   [00000400] destroying tcb 0000400f
   
   i am thread no.15, tcbid is 400f, i am killed ... 
   
   [00000400] destroying tcb 00004002
   
   i am thread no.2, tcbid is 4002, i am killed ... 
   
   [00000400] destroying tcb 00004004
   
   i am thread no.4, tcbid is 4004, i am killed ... 
   
   [00000400] destroying tcb 00004006
   
   ...
   
   [00000400] destroying tcb 0000400d
   
   [00000400] free env 00000400
   
   i am thread no.13, tcbid is 400d, i am killed ... 
   ```

2. `exit`测试。`exit`表示进程的退出，执行后所有线程退出；`pthread_exit`为线程的独立退出，不会影响其他线程的执行。在文件`myexittest.c`。子线程执行exit，判断主线程是否退出。如果没有退出则Panic

   ```c
   #include "lib.h"
   void *test(void *args){
       exit();
       user_panic("son doesn't exit");
   }
   
   void umain(){
       pthread_t son;
       writef("father begins\n");
       pthread_create(&son, NULL, test, NULL);
       writef("create son!\n");
       syscall_yield();
       user_panic("father doesn't exit!");
       pthread_exit(NULL);
   }
   ```

   运行结果，省去部分无关输出

   ```
   thread id is 4000
   
   father begins
   
   thread id is 4001
   
   create son!
   
   [00000400] destroying 00000400
   
   [00000400] free env 00000400
   ```

3. `pthread_cancel`测试：

   * `test1`测试默认状态下`pthread_cancel`是否执行正确；

   * `test2`测试函数`pthread_setcancelstate`；

   * `test3`测试函数`pthread_setcanceltype`

   ```c
   #include "lib.h"
   void *test1(int *flag){//for default cancel test
       while((&flag) == 0){
           syscall_yield();
       }
       writef("son begins\n");
       pthread_testcancel();
       user_panic("son hasn't exit");
   }
   
   void *test2(int *flag){//for disable cancelstate test
       int oldvalue;
       pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldvalue);
       if(oldvalue != PTHREAD_CANCEL_ENABLE)
           user_panic("cancelstate is wrong");
       *flag = 1;
   	pthread_testcancel();
   	writef("test2 success!\n");
   }         
   
   void *test3(int *flag){
   	int oldvalue;
   	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldvalue);
   	if(oldvalue != PTHREAD_CANCEL_DEFERRED)
   		user_panic("canceltype is wrong");
   	*flag = 1;
   	syscall_yield();
   	user_panic("son3 hasn't been canceled");
   }
   void umain(){
       pthread_t son;
       int *value_ptr, r, flag = 0, value = 0;
       value_ptr = &value;
   	//for test1
       pthread_create(&son, NULL, test1, (void *)(&flag));
       writef("create son1!\n");
       if((r = pthread_cancel(son)) < 0) 
           user_panic("pthread_cancel is wrong");
       flag = 1;
       pthread_join(son, &value_ptr);
       if(*value_ptr != 99) 
          	user_panic("return value is wrong!");
       //for test2
       flag = 0;
       pthread_create(&son, NULL, test2, (void *)&flag);
       writef("create son2!\n");
       while(flag == 0){
           syscall_yield();
       }
       if((r = pthread_cancel(son)) == 0) 
           user_panic("pthread_cancel is wrong");
   	//for test3
   	flag = 0;
   	pthread_create(&son, NULL, test3, (void *)&flag);
   	writef("create son3!\n");
   	while(flag == 0) {
   		syscall_yield();
   	}
   	if((r = pthread_cancel(son)) < 0)
   		user_panic("pthread_cancel is wrong");
       pthread_exit(NULL);
   }
   
   ```

   运行结果，省略部分无关输出

   ```
   create son1!
   
   son begins
   
   i am thread no.1, tcbid is 4001, i am killed ... 
   
   create son2!
   
   test2 success!
   
   i am thread no.1, tcbid is 4001, i am killed ... 
   
   create son3!
   
   [00000400] destroying tcb 00004001
   
   [00000400] destroying tcb 00004000
   
   [00000400] free env 00000400
   
   i am thread no.0, tcbid is 4000, i am killed ... 
   ```

4. `pthread_join`功能测试，测试子线程返回值是否等于退出值，`pthread_join`是否可以获得`pthread_exit`的指定退出值。

   ```c
   #include "lib.h"
   const int exitvalue1 = 100;
   const int exitvalue2 = 200;
   void *test1(void *arg){
       return &exitvalue;
   }
   void *test2(void *arg){
      	pthread_exit(&exitvalue2);
   }
   void umain(){
       int* getvalue;
       pthread_t son;
       pthread_create(&son, NULL, test1, NULL);
       pthread_join(son, &getvalue);
       if(*getvalue != exitvalue1){
           user_panic("join test failed!");
       }
       pthread_create(&son, NULL, test2, NULL);
       pthread_join(son, &getvalue);
       if(*getvalue != exitvalue2){
           user_panic("join test failed!");
       }
       writef("join test succeeds!\n");
       pthread_exit(NULL);
   }
   ```

   执行结果：输出`join test succeeds!`

5. ``pthread_detach`功能测试。见文件`mydetachtest.c`。判断detach后线程是否能被join。

   ```c
   #include "lib.h"
   void *test(void *arg){
       writef("son begins\n");
       syscall_yield();
       writef("son ends\n");
   }
   
   void umain(){
       pthread_t son;
       pthread_create(&son, NULL, test, NULL);
       pthread_detach(son);
       int r = pthread_join(son, NULL);
       if(r == 0) user_panic("detach is wrong");
       pthread_exit(NULL);
   }
   ```

   运行结果

   ```
   father ends[00000400] destroying tcb 00004000
   
   i am thread no.0, tcbid is 4000, i am killed ... 
   
   son begins
   
   son ends
   
   [00000400] destroying tcb 00004001
   
   i am thread no.1, tcbid is 4001, i am killed ... 
   
   [00000400] free env 00000400
   ```

### 信号量功能测试

1. `sem_wait`、`sem_trywait`与`sem_post`功能性测试。见`mysemtest1.c`。通过对共享 变量的操作的正确性。

   ```c
   #include "lib.h"
   int data = 100;
   #define E_SEM_EAGAIN 18
   sem_t mutex;
   //test3 is for trywait
   void *test3(void *arg){
   	int b, i, tmp, r;
       while((r = sem_trywait(&mutex)) == -E_SEM_EAGAIN){
   		syscall_yield();
   	}
       tmp = data;
       for(i=0;i<100;i++){
           b = data;
           b = b - 1;
           data = b;
           syscall_yield();
       }
       if(data != tmp - 100)
           user_panic("mutex is wrong");
       sem_post(&mutex);
       writef("test3 succeeds\n");
   }
   //test1 and test2 are for wait and post test
   void *test2(void *arg){
       int b, i, tmp;
       sem_wait(&mutex);
       tmp = data;
       for(i=0;i<100;i++){
           b = data;
           b = b - 1;
           data = b;
   		syscall_yield();
       }
       if(data != tmp - 100)
           user_panic("mutex is wrong");
       sem_post(&mutex);
   	writef("test2 succeeds\n");
   }
   
   void *test1(void *arg){
       int a, i, tmp;
       sem_wait(&mutex);
       tmp = data;
       for(i=0;i<100;i++){
           a = data;
           a = a + 1;
           data = a;
   		syscall_yield();
       }
       if(data != tmp + 100)
           user_panic("mutex is wrong");
       sem_post(&mutex);
   	writef("test1 succeeds\n");
   }
   
   void umain(){
       pthread_t son[15];
       int i, *value;
   	sem_init(&mutex, 0, 1);
       pthread_create(&son[0], NULL, test1, NULL);
       pthread_create(&son[1], NULL, test2, NULL);
   	pthread_join(son[0], NULL);
   	pthread_join(son[1], NULL);
   	for(i=0; i<15; i++){
   		pthread_create(&son[i], NULL, test1, NULL);
   	}
   	for(i=0; i<15; i++){
   		pthread_join(son[i], NULL);
   	}	
   
   	for(i=0; i<15; i++){
   		pthread_create(&son[i], NULL, test3, NULL);
   	}
       pthread_exit(NULL);
   }
   ```
   
   运行结果，未出现`panic`
   
   ```
   test1 succeeds
   test2 succeeds
   ...
   test3 succeeds
   ```
   
2. `sem_getvalue`， `sem_destroy`功能性测试。见`mysemtest2.c`文件

   ```c
   #include "lib.h"
   sem_t signal;
   #define E_SEM_ERROR 16
   void umain(){
       int r, num = 10, value;                           
       sem_init(&signal, 0, num);
       sem_getvalue(&signal, &value);
       if(value != num) user_panic("getvalue is wrong");
       sem_destroy(&signal);
       if((r = sem_wait(&signal)) != -E_SEM_ERROR)
           user_panic("destroy is wrong");
       pthread_exit(NULL);
   }
   ```

​		运行结果：无`panic`

## 三、遇到的问题及解决方案

1. 栈空间的独立和地址空间的共享如何解决？

各线程的栈是互相独立的，栈指针保存在`regs[29]`中。为了方便起见，规定每个线程栈大小为4MB，从`USTACKTOP`开始，按照`env_threads`数组下标的顺序依次分配。

2. 当线程通过`pthread_cancel`取消线程时，如何使线程的退出值的地址为`PTHREAD_CANCELED`？

线程控制块中，设置指针`tcb_exit_ptr`保存退出值的地址，该地址要求必须是全局共享，而不是局部地址。在`entry.S`中，在`.data`下新增变量`pthread_cancel_value`，表示线程被取消时退出的值。

```c
//user/entry.S
	.globl pthread_cancel_value 
pthread_cancel_value:
    .word 99
//user/pthread.c
#define PTHREAD_CANCELED (void *)(&pthread_cancel_value)   
```

3. 当线程执行`pthread_join`加入另一个线程后，如何获取其`exit value`？

由于`exit value`是通过指针传递的，那么需要设置一个指针的指针来获取。因此，在TCB中设置`void **tcb_join_value_ptr`来获取。

```c
//执行pthread_join的进程
//int pthread_join(pthread_t thread, void **value_ptr);
curtcb->tcb_join_value_ptr = value_ptr;

//被执行pthread_join的进程执行syscall_thread_destroy时
if(joinedtcb != 0){
    if(joinedtcb->tcb_join_value_ptr != 0)
        *(joinedtcb->tcb_join_value_ptr) = t->tcb_exit_ptr;
    sys_set_thread_status(0,joinedtcb->tcb_id,ENV_RUNNABLE);
}
```

4. 线程执行程序的返回值如何作为自己的退出值？

子线程未执行`pthread_exit`，正常退出，执行`return `语句返回一个`void *`指针，之后会执行`son_exit`。`son_exit`将程序的返回值以参数的形式传递给`son_exit_final`函数，作为线程的退出值。

```c
//user/son_exit.S
LEAF(son_exit)
    move a0, v0
    j son_exit_final
    nop
END(son_exit)

//user/libos.c
void son_exit_final(void *exit_ptr){
    struct Tcb *t = &env->env_threads[TCBX(syscall_getthreadid())];
    t->tcb_exit_ptr = exit_ptr;
    syscall_thread_destroy(0);
}
```

5. 如何通过线程控制块找到对应的进程控制块？

线程控制块本身包含在进程控制块中，而在`mm/pmap.c`文件的` mips_vm_init`函数中，为1024个进程控制块分配内存，并映射到`UENVS`区域，该区域大小为4MB，因此一个PCB最大为1KB。故此，我们可以把PCB设为1页，16个线程控制块可以通过`ROUNDDOWN(t, BY2PG)`的方式找到PCB的地址

6. 对于其他函数，诸如`fork`、`ipc`等函数，如何修改能保证原有Lab4测试点通过？

本项目的设计是主线程为`env_threads[0]`，因此将`env_threads[0]`当做原来的进程处理即可。



echo hello > motd

cat motd

ls | cat

cat <newmotd \>motd
