/* See COPYRIGHT for copyright information. */

#ifndef _ENV_H_
#define _ENV_H_

#include "types.h"
#include "queue.h"
#include "trap.h"
#include "mmu.h" 

#define LOG2NENV	10
#define NENV		(1<<LOG2NENV)
#define ENVX(envid)	((envid) & (NENV - 1))
#define GET_ENV_ASID(envid) (((envid)>> 11)<<6)

//for threads
//for pthread_cancel
#define PTHREAD_CANCEL_DISABLE 0
#define PTHREAD_CANCEL_ENABLE 1
#define PTHREAD_CANCEL_DEFERRED 0
#define PTHREAD_CANCEL_ASYNCHRONOUS 1

#define THREAD_MAX 8
#define TCB2ENV(t) ROUNDDOWN(t, BY2PG)
// Values of env_status in struct Env
#define ENV_FREE	0
#define ENV_RUNNABLE		1
#define ENV_NOT_RUNNABLE	2
struct Tcb {
	// basic information
	struct Trapframe tcb_tf;
	u_int tcb_id;
	u_int tcb_status;
	u_int tcb_pri;
	LIST_ENTRY(Tcb) tcb_sched_link;
    
	// pthread_join information
	struct Tcb* tcb_joinedtcb;
	void **tcb_join_value_ptr;//join的进程保存
	u_int tcb_detach;

	// pthread_exit information
	void *tcb_exit_ptr;//保存退出的值，如果是正常退出，则ptr为0
	//int tcb_exit_value;为了保证结果正确性，注释掉这条语句

	// pthread_cancel information
	int tcb_cancelstate;
	int tcb_canceltype;
	u_int tcb_canceled;//是否收到cancel信号

	// keep bytes
	//u_int tcb_nop[10];按最终需要分配
};

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
	u_int env_ipc_dstva;		// va at which to map received page
	u_int env_ipc_perm;		// perm of page mapping received

	// Lab 4 fault handling
	u_int env_pgfault_handler;      // page fault state
	u_int env_xstacktop;            // top of exception stack

	// Lab 6 scheduler counts
	u_int env_runs;			// number of times been env_run'ed

	// Lab 4 challenge
	u_int env_thread_count;

	// keep bytes
	//u_int env_nop[496];                  // align to avoid mul instruction

	// Lab 4 challenge
	struct Tcb env_threads[8];
};

LIST_HEAD(Env_list, Env);
LIST_HEAD(Tcb_list, Tcb);
extern struct Env *envs;		// All environments
extern struct Env *curenv;	        // the current env
extern struct Env_list env_sched_list[2]; // runnable env list

void env_init(void);
int env_alloc(struct Env **e, u_int parent_id);
void env_free(struct Env *);
void env_create_priority(u_char *binary, int size, int priority);
void env_create(u_char *binary, int size);
void env_destroy(struct Env *e);

int envid2env(u_int envid, struct Env **penv, int checkperm);
void env_run(struct Env *e);


// for the grading script
#define ENV_CREATE2(x, y) \
{ \
	extern u_char x[], y[]; \
	env_create(x, (int)y); \
}
#define ENV_CREATE_PRIORITY(x, y) \
{\
        extern u_char binary_##x##_start[]; \
        extern u_int binary_##x##_size;\
        env_create_priority(binary_##x##_start, \
                (u_int)binary_##x##_size, y);\
}
#define ENV_CREATE(x) \
{ \
	extern u_char binary_##x##_start[];\
	extern u_int binary_##x##_size; \
	env_create(binary_##x##_start, \
		(u_int)binary_##x##_size); \
}

#endif // !_ENV_H_
