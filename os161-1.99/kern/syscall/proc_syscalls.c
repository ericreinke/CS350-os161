#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <mips/trapframe.h>
#include <synch.h>
#include <vfs.h>
#include <vm.h>
#include <kern/fcntl.h> // vfs open flags 
#include "opt-A2.h"

int sys_execv(const char * program, char ** uargs){
  struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;
  int arg_count;
  arg_count=0;

  for(int i = 0; uargs[i]!=NULL; i++){
    arg_count++;
  }

  char ** kargs = kmalloc((arg_count + 1) * sizeof(char *));//kargs is null terminated, hence +1;
  for(int i = 0; i < arg_count; i++){
    int arg_len = (strlen(uargs[i]) + 1);
    int arg_size = arg_len * sizeof(char);
    kargs[i] = kmalloc(arg_size);
    copyinstr((const_userptr_t)uargs[i], (void *)kargs[i], arg_len, NULL);
  }
  kargs[arg_count] = NULL;

	/* Open the file. */
  char * fname_temp;
  fname_temp= kstrdup(program);
	result = vfs_open(fname_temp, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}
  //kprintf(fname_temp);
  kfree(fname_temp);

	/* Create a new address space. */
	as = as_create();
	if (as ==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
	curproc_setas(as);
	as_activate();

	/* Load the executable. */
	result = load_elf(v, &entrypoint); // modifies entrypoint to be correct
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

  int result2 = as_define_stack(as, &stackptr); // modifies stackptr to be correct
  if(result2) return result2; // if result is not 0 then error


  vaddr_t * user_stack_args = kmalloc((arg_count + 1) * sizeof(vaddr_t));

  user_stack_args[arg_count] = (vaddr_t) NULL;
  for(int i = arg_count -1; i >=0; i--){ // goes through each karg and copyout to user and stores pointer in user_stack_args
    size_t arg_len = ROUNDUP(strlen(kargs[i])+1, 4);
    size_t arg_size = arg_len * (sizeof(char));
    stackptr -= arg_size;
    copyoutstr((void *) kargs[i], (userptr_t) stackptr, arg_len, NULL);
    user_stack_args[i] = stackptr;
  }

  stackptr-=8; // don't ask

  for (int i = arg_count -1; i >=0; i--){ // now we need to copyout the addresses of the arguments
    size_t arg_pointer_size = sizeof(userptr_t);
    stackptr -= arg_pointer_size;
    copyout((void*) &user_stack_args[i], (userptr_t) stackptr, arg_pointer_size);
  }

  enter_new_process(arg_count,(userptr_t) stackptr, ROUNDUP(stackptr,8), entrypoint); 
  //should not return from this.

  return EINVAL; //invalid argument.  enter_new_process returned and it shouldn't have.

}


int sys_fork(struct trapframe *tf, pid_t * retval){

  // Create a process structure for child process (same name)
  struct proc *child = proc_create_runprogram("child");
  if(child==NULL){
    panic("Could not create child process structure");
  }

  //Create and copy the address space
  int copy_ret = as_copy(curproc_getas(), &(child->p_addrspace));
  if(copy_ret!=0){
    panic("Could not copy address space to child");
  }

  // Child PID is initialized in proc_create

  // The child thread needs to put the trap frame onto its stack and modify it so 
  // that it returns the correct value.  Modification to tf is done in entered_forked_process

  child->tf = kmalloc(sizeof(struct trapframe));
  memcpy(child->tf, tf, sizeof(struct trapframe));

  //populate children_proc_info
  child->parent = curproc;
  struct child_proc_info * new_proc_info = kmalloc(sizeof(struct child_proc_info));
  new_proc_info->pid = child->pid;
  new_proc_info->exitcode = -1;
  new_proc_info->the_proc = child;
  array_add(curproc->children_proc_info, new_proc_info, NULL); 

  // Create a thread for the child process
  int thread_ret = thread_fork(child->p_name, child, (void *)&enter_forked_process,child->tf,child->pid);
  if(thread_ret!=0){
    panic("Error creating thread for child process");
  }

 *retval = child->pid;
 return 0;
}

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() andaitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;

  #if OPT_A2
  // we need to update the parent arry of info, telling it what our exit code is
  // iterate and check all through the info array
  struct child_proc_info * my_info;
  if(curproc->parent != NULL){ // the first proc does not have a parent
    lock_acquire(curproc->parent->children_lock);
    for(unsigned int i = 0; i < array_num(curproc->parent->children_proc_info); i ++){
      my_info = array_get(curproc->parent->children_proc_info, i);
      if(my_info->pid == curproc->pid){
        // we found ourselves in our parent's tracking array
        my_info->exitcode = exitcode;
      }
    }
    lock_release(curproc->parent->children_lock);
  }
  

  #endif

  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  #if OPT_A2
  *retval = curproc->pid;
  #else
  *retval = 1;
  #endif
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
  #if OPT_A2
  if(status == NULL){
    return EFAULT;
  }
  bool found = false;
  lock_acquire(curproc->children_lock);
  for(unsigned int i = 0; i < array_num(curproc->children_proc_info); i++){
    struct child_proc_info * my_child = array_get(curproc->children_proc_info, i);
    if(pid == my_child->pid){ // we found the corresponding child
      found = true;
      while(my_child->exitcode == -1){ // remember for loop needed for cv.  need to check again it has exited.
        cv_wait(my_child->the_proc->has_died, curproc-> children_lock);
      }
      exitstatus = _MKWAIT_EXIT(my_child->exitcode);//set exit status
    }
  }
  if(!found){
    *retval = -1; //pid
    lock_release(curproc->children_lock); //we are exiting
    return(ESRCH); //no such process
  }

  lock_release(curproc->children_lock);
  #endif

  if (options != 0) {
    return(EINVAL);

  }
  
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

