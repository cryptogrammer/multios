/*
 * student.c
 * Multithreaded OS Simulation for CS 2200, Project 4
 * Fall 2014
 *
 * This file contains the CPU scheduler for the simulation.
 * Name: Utkarsh Garg
 * GTID: 902904045
 */

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "os-sim.h"


/*
 * current[] is an array of pointers to the currently running processes.
 * There is one array element corresponding to each CPU in the simulation.
 *
 * current[] should be updated by schedule() each time a process is scheduled
 * on a CPU.  Since the current[] array is accessed by multiple threads, you
 * will need to use a mutex to protect it.  current_mutex has been provided
 * for your use.
 */
static pcb_t **current;
static pthread_mutex_t current_mutex;
static pthread_mutex_t readyQueue_mutex;
static pthread_cond_t queueNotEmpty;
static pcb_t *readyQueueHead;
static int roundRobin = 0;
static int staticPriority = 0;
static int numTimeSlice = -1;
static int cpu_count = 0;


/*
 * schedule() is your CPU scheduler.  It should perform the following tasks:
 *
 *   1. Select and remove a runnable process from your ready queue which
 *	you will have to implement with a linked list or something of the sort.
 *
 *   2. Set the process state to RUNNING
 *
 *   3. Call context_switch(), to tell the simulator which process to execute
 *      next on the CPU.  If no process is runnable, call context_switch()
 *      with a pointer to NULL to select the idle process.
 *	The current array (see above) is how you access the currently running process indexed by the cpu id.
 *	See above for full description.
 *	context_switch() is prototyped in os-sim.h. Look there for more information
 *	about it and its parameters.
 */


static int isEmpty(){
    if(readyQueueHead == NULL) return 1;
    return 0;
}

static pcb_t* pop(){
    pcb_t *newPointer;
    if(readyQueueHead == NULL) return NULL;
    newPointer = readyQueueHead;
    readyQueueHead = readyQueueHead->next;
    newPointer->state = PROCESS_RUNNING;
    newPointer->next = NULL;
    return newPointer;
}

static pcb_t* staticPop(){
    pcb_t* newPointer;
    pcb_t* iterativePointer;
    pcb_t* iterativePrevPointer;
    if(readyQueueHead == NULL) return NULL;
    iterativePointer = readyQueueHead;
    iterativePrevPointer = NULL;
    int max = -1;
    while(iterativePointer->next != NULL){
        if(iterativePointer->static_priority > max){
            max = iterativePointer->static_priority;
            newPointer = iterativePointer;
            iterativePrevPointer = readyQueueHead;
            while(iterativePrevPointer->next != iterativePointer && iterativePrevPointer->next !=NULL) iterativePrevPointer = iterativePrevPointer->next;
        }
        iterativePointer = iterativePointer->next;
    }
    if(iterativePrevPointer != NULL) iterativePrevPointer->next = newPointer->next;
    else {
        newPointer = readyQueueHead;
        readyQueueHead = readyQueueHead->next;
    }
    newPointer->state = PROCESS_RUNNING;
    newPointer->next = NULL;
    return newPointer;
}

static void mutexLock(){
    pthread_mutex_lock(&readyQueue_mutex);
    pthread_mutex_lock(&current_mutex);
}

static void mutexUnlock(){
    pthread_mutex_unlock(&readyQueue_mutex);
    pthread_mutex_unlock(&current_mutex);
}

static void schedule(unsigned int cpu_id)
{
    
    if (isEmpty()) context_switch(cpu_id, NULL, numTimeSlice);
    else{
        pcb_t *newPointer;
        mutexLock();
        if(staticPriority == 0) newPointer = pop();
        else newPointer = staticPop();
        current[cpu_id] = newPointer;
        mutexUnlock();
        context_switch(cpu_id, newPointer, numTimeSlice);
        }
}


/*
 * idle() is your idle process.  It is called by the simulator when the idle
 * process is scheduled.
 *
 * This function should block until a process is added to your ready queue.
 * It should then call schedule() to select the process to run on the CPU.
 */
extern void idle(unsigned int cpu_id)
{
    /* FIX ME */
    pthread_mutex_lock(&readyQueue_mutex);
    while (readyQueueHead == NULL) pthread_cond_wait(&queueNotEmpty, &readyQueue_mutex);
    pthread_mutex_unlock(&readyQueue_mutex);
    schedule(cpu_id);
    
    /*
     * REMOVE THE LINE BELOW AFTER IMPLEMENTING IDLE()
     *
     * idle() must block when the ready queue is empty, or else the CPU threads
     * will spin in a loop.  Until a ready queue is implemented, we'll put the
     * thread to sleep to keep it from consuming 100% of the CPU time.  Once
     * you implement a proper idle() function using a condition variable,
     * remove the call to mt_safe_usleep() below.
     */
}


/*
 * preempt() is the handler called by the simulator when a process is
 * preempted due to its timeslice expiring.
 *
 * This function should place the currently running process back in the
 * ready queue, and call schedule() to select a new runnable process.
 */
extern void preempt(unsigned int cpu_id)
{
    pcb_t *currentProcess;
    pcb_t *findTail;
    mutexLock();
    currentProcess = current[cpu_id];
    findTail = readyQueueHead;
    if (findTail == NULL) readyQueueHead = currentProcess;
    else {
        while (findTail->next != NULL) findTail = findTail->next;
        findTail->next = currentProcess;
    }
    pthread_cond_signal(&queueNotEmpty);
    mutexUnlock();
    schedule(cpu_id);
    
}


/*
 * yield() is the handler called by the simulator when a process yields the
 * CPU to perform an I/O request.
 *
 * It should mark the process as WAITING, then call schedule() to select
 * a new process for the CPU.
 */
extern void yield(unsigned int cpu_id)
{
    pcb_t *currentProcess;
    pthread_mutex_lock(&current_mutex);
    currentProcess = current[cpu_id];
    currentProcess->state = PROCESS_WAITING;
    pthread_mutex_unlock(&current_mutex);
    schedule(cpu_id);
}


/*
 * terminate() is the handler called by the simulator when a process completes.
 * It should mark the process as terminated, then call schedule() to select
 * a new process for the CPU.
 */
extern void terminate(unsigned int cpu_id)
{
    pcb_t *currentProcess;
    pthread_mutex_lock(&current_mutex);
    currentProcess = current[cpu_id];
    currentProcess->state = PROCESS_TERMINATED;
    pthread_mutex_unlock(&current_mutex);
    schedule(cpu_id);
}


/*
 * wake_up() is the handler called by the simulator when a process's I/O
 * request completes.  It should perform the following tasks:
 *
 *   1. Mark the process as READY, and insert it into the ready queue.
 *
 *   2. If the scheduling algorithm is static priority, wake_up() may need
 *      to preempt the CPU with the lowest priority process to allow it to
 *      execute the process which just woke up.  However, if any CPU is
 *      currently running idle, or all of the CPUs are running processes
 *      with a higher priority than the one which just woke up, wake_up()
 *      should not preempt any CPUs.
 *	To preempt a process, use force_preempt(). Look in os-sim.h for
 * 	its prototype and the parameters it takes in.
 */
extern void wake_up(pcb_t *process)
{
    printf("%s","ENTERED WAKE UP");
    pcb_t *findTail;
    pthread_mutex_lock(&readyQueue_mutex);
    if (process != NULL) {
        process->state = PROCESS_READY;
        process->next = NULL;
        findTail = readyQueueHead;
        if (findTail == NULL)
            readyQueueHead = process;
        else {
            while (findTail->next != NULL) findTail = findTail->next;
            findTail->next = process;
            findTail = process;
            findTail->next = NULL;
        }
        pthread_cond_signal(&queueNotEmpty);
    }
    pthread_mutex_unlock(&readyQueue_mutex);
    //printf("%s","STATIC PRIORITY\n");
    //printf("%i", staticPriority);
    
    if(staticPriority == 1) {
        //printf("%s","STATIC PRIORITY IS 1!!!!!!!!!!!!!!!!!!!!!!");
        int i,lowest,low_id;
        pthread_mutex_lock(&current_mutex);
        i = 0;
        low_id = -1;
        lowest = 10;
        while(i<cpu_count && current[i]!=NULL){
            if(current[i]->static_priority < lowest) {
                lowest = current[i]->static_priority;
                low_id = i;
            }
            ++i;
        }
        pthread_mutex_unlock(&current_mutex);
        if(low_id != -1 && lowest < process->static_priority) {
            force_preempt(low_id);
        }
    }
}


/*
 * main() simply parses command line arguments, then calls start_simulator().
 * You will need to modify it to support the -r and -p command-line parameters.
 */
int main(int argc, char *argv[])
{
    /* Parse command-line arguments */
    if (argc < 2 || argc > 4)
    {
        fprintf(stderr, "CS 2200 Project 4 -- Multithreaded OS Simulator\n"
                "Usage: ./os-sim <# CPUs> [ -r <time slice> | -p ]\n"
                "    Default : FIFO Scheduler\n"
                "         -r : Round-Robin Scheduler\n"
                "         -p : Static Priority Scheduler\n\n");
        return -1;
    }
    cpu_count = atoi(argv[1]);
    if(cpu_count==0) exit(1);
    /* FIX ME - Add support for -r and -p parameters*/
    
    if(argc == 4){
        if(strcmp(argv[2],"-r")==0){
            //printf("%s", "ROUND ROBIN IF STATEMENT\n");
            roundRobin = 1;
            numTimeSlice = atoi(argv[3]);
            printf("%i",numTimeSlice);
        }
        else exit(1);
    }
    else if(argc == 3){
        if(strcmp(argv[2],"-p")==0){
            staticPriority = 1;
        }
        else exit(1);
    }
    
    /* Allocate the current[] array and its mutex */
    current = malloc(sizeof(pcb_t*) * cpu_count);
    assert(current != NULL);
    pthread_mutex_init(&current_mutex, NULL);
    pthread_mutex_init(&readyQueue_mutex, NULL);
    pthread_cond_init(&queueNotEmpty, NULL);
    
    /* Start the simulator in the library */
    start_simulator(cpu_count);
    
    return 0;
}

