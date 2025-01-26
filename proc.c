#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;


void* adjListPage;  // Pointer to the allocated kernel page for the adjacency list

typedef struct Node {
    int vertex;
    enum nodetype type;
    struct Node* next;
} Node;
struct {
  struct spinlock lock;
  Node* adjList[MAXTHREAD+NRESOURCE];
  int visited[MAXTHREAD+NRESOURCE];
  int recStack[MAXTHREAD+NRESOURCE];
} Graph;
//################ADD Your Implementation Here######################

// Function to initialize the graph
void initializeGraph() {
    // Allocate a kernel page for the adjacency list
    adjListPage = kalloc();
    
    memset(adjListPage, 0, PGSIZE);


    if (adjListPage == 0) {
        panic("initializeGraph: kalloc failed");
    }

    // Initialize the adjacency list and metadata
    for (int i = 0; i < MAXTHREAD + NRESOURCE; i++) {
        Graph.adjList[i] = 0;  // Set all pointers to NULL
        Graph.visited[i] = 0;  // Mark all nodes as unvisited
        Graph.recStack[i] = 0; // Initialize recursion stack
    }

    // Initialize the spinlock
    initlock(&Graph.lock, "graph_lock");
}

void addResourceNode(int resource_id) {
    acquire(&Graph.lock);  // Acquire the graph lock for thread safety

    // Find an available slot in the adjacency list page
    Node* newNode = (Node*)adjListPage;
    while (newNode->vertex != 0) {  // Find an unused Node struct
        newNode++;
    }

    // Initialize the new node
    newNode->vertex = resource_id;
    newNode->type = RESOURCE;
    newNode->next = 0;

    // Add the node to the adjacency list for the resource
    Graph.adjList[resource_id] = 0;

    cprintf("Added resource node for resource %d\n", resource_id);

    release(&Graph.lock);  // Release the graph lock
}

// Function to find a node with a specific vertex in adjListPage
Node* findNode(int vertex) {
    Node* node = (Node*)adjListPage;
    for (int i = 0; i < (PGSIZE / sizeof(Node)); i++) {
        if (node->vertex == vertex) {
            return node;  // Return the node if the vertex is found
        }
        node++;
    }
    return 0;  // Return NULL if the vertex is not found
}

// Function to add a thread node to the graph
void addThreadNode(int tid) {

    acquire(&Graph.lock);  // Acquire the graph lock for thread safety

    // Find an available slot in the adjacency list page
    Node* newNode = (Node*)adjListPage;
    while (newNode->vertex != 0) {  // Find an unused Node struct
        newNode++;
    }

    // Initialize the new node
    newNode->vertex = tid;
    newNode->type = PROCESS;
    newNode->next = 0;

    // Add the node to the end of the linked list for the thread partition
    Graph.adjList[NRESOURCE+tid] = 0;

    release(&Graph.lock);  // Release the graph lock
}

// Function to remove an edge from the graph
void removeEdge(int src, int dest) {
    acquire(&Graph.lock);  // Acquire the graph lock for thread safety

    Node* temp = Graph.adjList[src];
    // Traverse the linked list to find the node to remove
    if (temp != 0) {
        if (temp->vertex == dest) {
            // Remove the node from the linked list
            Graph.adjList[src] = 0;  // Update the head pointer
        }
    }
    release(&Graph.lock);  // Release the graph lock
}


//this func need to be change
void removeThreadNode(int tid) {
    acquire(&Graph.lock);  // Acquire the graph lock for thread safety

    // Remove the thread node from the adjacency list
    
    Node* node = (Node*)adjListPage;
    for (int i = 0; i < (PGSIZE / sizeof(Node)); i++) {
        if (node->vertex == tid) {
            // Mark the node as unused
            node->vertex = 0;  // Reset the vertex ID to indicate the node is unused
            node->next = 0;    // Reset the next pointer

            // Debug print
            cprintf("Removed and marked thread node for thread %d as unused\n", tid);
            break;  // Exit the loop once the node is found and marked
        }
        node++;  // Move to the next node in adjListPage
    }

    Node* temp = Graph.adjList[tid];

    // Traverse the linked list to find the node to remove
    if (temp != 0) {
        // Mark the node as unused in adjListPage
        temp->vertex = 0;  // Reset the vertex ID to indicate the node is unused
        temp->next = 0;    // Reset the next pointer

        // Update the adjacency list for the thread
        Graph.adjList[tid] = 0;  // Set the adjacency list for the thread to NULL
    }

    // Release any resources held by the thread
    for (int i = NRESOURCE; i < MAXTHREAD + NRESOURCE; i++) {
        // Check if the resource is allocated to the thread
        if (Graph.adjList[i] != 0 && Graph.adjList[i]->vertex == tid) {
            // Remove the edge from the resource to the thread
            removeEdge(i, tid);

            // Mark the resource as free
            Resource* resources = (Resource*)(adjListPage + 2048);  // Resource metadata starts at offset 2048
            resources[i - NRESOURCE].acquired = -1;  // Mark the resource as free
        }
    }

    release(&Graph.lock);  // Release the graph lock
}

void addEdge(int src, int dest) {
    acquire(&Graph.lock);  // Acquire the graph lock for thread safety

    // Find the destination node in adjListPage
    Node* destNode = findNode(dest);
    if (destNode == 0) {
        // If the destination node does not exist, return (we cannot add an edge to a non-existent node)
        release(&Graph.lock);
        return;
    }

    // Check the type of the source node
    Node* srcNode = findNode(src);
    if (srcNode == 0) {
        // If the source node does not exist, return (we cannot add an edge from a non-existent node)
        release(&Graph.lock);
        return;
    }

    // If the source node is a RESOURCE, ensure it has no outgoing edges
    if (srcNode->type == RESOURCE) {
        if (Graph.adjList[src] != 0) {
            // Resource already has an outgoing edge, so we cannot add another one
            release(&Graph.lock);
            return;
        }
    }
    if(srcNode->type == RESOURCE)
    {
      // If the edge is from a resource to a thread, mark the resource as acquired
      Resource* resources = (Resource*)(adjListPage + 2048);  // Resource metadata starts at offset 2048
      resources[src].acquired = 1;  // Mark the resource as acquired
      cprintf("Resource %d is marked as acquired (edge from resource to thread)\n", src);
    }

    // Debug print
    // Add the destination node to the adjacency list of the source vertex
    // destNode->next = Graph.adjList[src];  // Insert at the head of the linked list
    Graph.adjList[src] = destNode;        // Update the head pointer

    release(&Graph.lock);  // Release the graph lock
}

// Recursive DFS function to detect cycles
int isCyclicUtil(int v) {
    if (!Graph.visited[v]) {
        Graph.visited[v] = 1;
        Graph.recStack[v] = 1;

        // Traverse the adjacency list of the current node
        Node* temp = Graph.adjList[v];
        while (temp != 0) {
            if (!Graph.visited[temp->vertex] && isCyclicUtil(temp->vertex))
                return 1;  // Cycle detected
            else if (Graph.recStack[temp->vertex])
                return 1;  // Cycle detected
            temp = temp->next;
        }
    }
    Graph.recStack[v] = 0;  // Remove the node from the recursion stack
    return 0;  // No cycle detected
}

// Function to detect deadlocks
int isCyclic() {
    acquire(&Graph.lock);  // Acquire the graph lock for thread safety

    // Reset visited and recStack arrays
    for (int i = 0; i < MAXTHREAD + NRESOURCE; i++) {
        Graph.visited[i] = 0;
        Graph.recStack[i] = 0;
    }

    // Perform DFS on each node
    int result = 0;
    for (int i = 0; i < MAXTHREAD + NRESOURCE; i++) {
        if (isCyclicUtil(i)) {
            result = 1;  // Cycle detected
            break;
        }
    }

    release(&Graph.lock);  // Release the graph lock
    return result;
}

//##################################################################

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }

  sp = p->kstack + KSTACKSIZE;
  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;
  p->Is_Thread=0;
  p->Thread_Num=0;
  p->tstack=0;
  p->tid=0;
  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
//################ADD Your Implementation Here######################


      //Resource page handling and creation
      

    void* page = kalloc();
    Resource* resources = (Resource*) page;
    void* bufferStart = page + 2048;

    for (int i = 0; i < NRESOURCE; i++)
    {
      resources[i].resourceid = i;
      resources[i].name[0] = '0' + i;
      resources[i].acquired = -1;
      resources[i].startaddr = bufferStart + i * (2048 / NRESOURCE);
    }
    initializeGraph();
    for(int i =0 ; i<NRESOURCE ; i++)
    {
      addResourceNode(i);
    }
    

//##################################################################
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
  cprintf("i done this part\n");
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));
  //np->tid=-1;
  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;
  if(curproc->tid == 0 && curproc->Thread_Num!=0) {
    panic("Parent cannot exit before its children");
  }
  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}
int clone(void (*worker)(void*,void*),void* arg1,void* arg2,void* stack)
{
  //int i, pid;
  struct proc *New_Thread;
  struct proc *curproc = myproc();
  uint sp,HandCrafted_Stack[3];
  // Allocate process.
  if((New_Thread = allocproc()) == 0){
    return -1;
  }
  if(curproc->tid!=0){
      kfree(New_Thread->kstack);
      New_Thread->kstack = 0;
      New_Thread->state = UNUSED;
      cprintf("Clone called by a thread\n");
      return -1;
  }
  //The new thread parent would be curproc
  New_Thread->pid=curproc->pid;
  New_Thread->sz=curproc->sz;

  //The tid of the thread will be determined by Number of current threads 
  //of a process
  curproc->Thread_Num++;
  New_Thread->tid=curproc->Thread_Num;
  New_Thread->Is_Thread=1;
  //The parent of thread will be the process calling clone
  New_Thread->parent=curproc;

  //Sharing the same virtual address space
  New_Thread->pgdir=curproc->pgdir;
  if(!stack){
      kfree(New_Thread->kstack);
      New_Thread->kstack = 0;
      New_Thread->state = UNUSED;
      curproc->Thread_Num--;
      New_Thread->tid=0;
      New_Thread->Is_Thread=0;
      cprintf("Child process wasn't allocated a stack\n");    
  }
  //Assuming that child_stack has been allocated by malloc
  New_Thread->tstack=(char*)stack;
  //Thread has the same trapframe as its parent
  *New_Thread->tf=*curproc->tf;

  HandCrafted_Stack[0]=(uint)0xfffeefff;
  HandCrafted_Stack[1]=(uint)arg1;
  HandCrafted_Stack[2]=(uint)arg2;
  
  sp=(uint)New_Thread->tstack;
  sp-=3*4;
  if(copyout(New_Thread->pgdir, sp,HandCrafted_Stack, 3 * sizeof(uint)) == -1){
      kfree(New_Thread->kstack);
      New_Thread->kstack = 0;
      New_Thread->state = UNUSED;
      curproc->Thread_Num--;
      New_Thread->tid=0;
      New_Thread->Is_Thread=0;      
      return -1;
  }
  New_Thread->tf->esp=sp;
  New_Thread->tf->eip=(uint)worker;
  //Duplicate all the file descriptors for the new thread
  for(uint i = 0; i < NOFILE; i++){
    if(curproc->ofile[i])
      New_Thread->ofile[i] = filedup(curproc->ofile[i]);
  }
  New_Thread->cwd = idup(curproc->cwd);
  safestrcpy(New_Thread->name, curproc->name, sizeof(curproc->name));
  acquire(&ptable.lock);
  New_Thread->state=RUNNABLE;
  release(&ptable.lock);
  //add node of new thread to graph
  cprintf("i am here in the clone \n");
  addThreadNode(New_Thread->pid);

  //cprintf("process running Clone has  %d threads\n",curproc->Thread_Num);  
  return New_Thread->tid;
}
int join(int Thread_id) {
    struct proc *p, *curproc = myproc();
    int Join_Thread_Exit = 0, jtid;
    if (Thread_id == 0)
        return -1;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->tid == Thread_id && p->parent == curproc) {
            Join_Thread_Exit = 1;
            break;
        }
    }
    if (!Join_Thread_Exit || curproc->killed) {
        return -1;
    }
    acquire(&ptable.lock);
    for (;;) {
        if (curproc->killed) {
            release(&ptable.lock);
            return -1;
        }
        if (p->state == ZOMBIE) {
            //remove the related node of thread in the graph
            cprintf("i am in the join part to test \n");
            removeThreadNode(Thread_id);
            curproc->Thread_Num--;
            jtid = p->tid;
            kfree(p->kstack);
            p->kstack = 0;
            p->pgdir = 0;
            p->pid = 0;
            p->tid = 0;
            p->tstack = 0;
            p->parent = 0;
            p->name[0] = 0;
            p->killed = 0;
            p->state = UNUSED;
            release(&ptable.lock);
            return jtid;
        }
        sleep(curproc, &ptable.lock);
    }
    return 0;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

int requestresource(int Resource_ID) {
    struct proc* curproc = myproc();
    int tid = curproc->tid;

    // Acquire the graph lock to ensure thread-safe access
    acquire(&Graph.lock);

    // Add an edge from the thread to the resource (request edge)
    addEdge(tid, Resource_ID);

    // Check for deadlock using DFS
    if (isCyclic()) {
        // Deadlock detected, remove the edge and return an error
        removeEdge(tid, Resource_ID);
        release(&Graph.lock);
        cprintf("Thread %d failed to acquire resource %d (deadlock detected)\n", tid, Resource_ID);
        return -1; // Deadlock detected
    }

    // No deadlock detected, allow the resource request
    release(&Graph.lock);
    cprintf("Thread %d successfully acquired resource %d\n", tid, Resource_ID);
    return 0; // Resource requested successfully
}


int releaseresource(int Resource_ID) {
    struct proc* curproc = myproc();
    int tid = curproc->tid;

    // Acquire the graph lock to ensure thread-safe access
    acquire(&Graph.lock);

    // Remove the edge from the thread to the resource
    removeEdge(tid, Resource_ID);

    // Release the graph lock
    release(&Graph.lock);

    cprintf("Thread %d successfully released resource %d\n", tid, Resource_ID);
    return 0; // Resource released successfully
}


int writeresource(int Resource_ID,void* buffer,int offset, int size)
{
//################ADD Your Implementation Here######################

//##################################################################
  return -1;
}
int readresource(int Resource_ID,int offset, int size,void* buffer)
{
//################ADD Your Implementation Here######################

//##################################################################
  return -1;
}