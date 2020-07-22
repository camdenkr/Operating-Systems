//Camden Kronhaus

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <setjmp.h>
#include "ec440threads.h"

#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <fcntl.h>

#define JB_RBX 0
#define JB_RBP 1
#define JB_R12 2
#define JB_R13 3
#define JB_R14 4
#define JB_R15 5
#define JB_RSP 6 //mangle
#define JB_PC 7  //mangled //RIP

#define MAX_THREADS 128
#define STACK_SIZE 32767

#define READY 0
#define RUNNING 1
#define EXITED 2

typedef struct
{
    pthread_t thread_id; //thread id
    jmp_buf buf;         //state of thread (its registers) //array if 8 64-ints
    void *stack;         //information about stack (pointer to stack area)
    int thread_status;   //information about status (ready 0, running 1, or exited 2)
} TCB;

TCB global_TCB;
TCB TCB_array[MAX_THREADS];
jmp_buf buf;

pthread_t thread_count = 0;
pthread_t current_thread = 0;

bool firstCall = 0;
int schedulerIndex = 0;

//round robin style scheduler
static void scheduler(int signal)
{
    //save context
    if (TCB_array[current_thread].thread_status == RUNNING) //set running thread back to ready if not exited
    {
        TCB_array[current_thread].thread_status = READY;
    }
    int i;
    i = setjmp(TCB_array[current_thread].buf); //save context of current thread //if (i == 0) //find next thread to run, if i != 0 then will return to same thread until next alarm
    //find a thread
    schedulerIndex++; //start with next thread, if it was the last one, loop back to first thread and start from there
    if (schedulerIndex > thread_count)
    {
        schedulerIndex = 0;
    }
    if (i == 0)
    {
        while (schedulerIndex <= thread_count)
        {
            if (TCB_array[schedulerIndex].thread_status == READY) //if ready set to running and run
            {
                TCB_array[schedulerIndex].thread_status = RUNNING;
                current_thread = TCB_array[schedulerIndex].thread_id;
                longjmp(TCB_array[schedulerIndex].buf, 1);
            }
            if (schedulerIndex == thread_count) //return to beginning when end of threads is reached
            {
                schedulerIndex = 0;
                continue;//skip increment after this
            }
            schedulerIndex++;
        }
    }
}

//sets thread to exited status and ends it
void pthread_exit(void *value_ptr)
{
    TCB_array[current_thread].thread_status = EXITED;
    scheduler(0);
    __builtin_unreachable();
}

//returns id of a thread
pthread_t pthread_self(void)
{
    return TCB_array[(int)current_thread].thread_id;
}

//creates a thread
int pthread_create(pthread_t *threads, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg)
{
    //only do the following for the initial thread ceration
    if (!firstCall)
    {
        *threads = thread_count;
        //sigaction is used to change the action of a proccess when a signal is received
        struct sigaction action_struct; //creates a sigaction struct
        action_struct.sa_handler = scheduler; //specifies the action associatied with "signum", points to handling fucntion

        sigemptyset(&action_struct.sa_mask); //intilizes things to NULL - no signals should be masked/blocked

        action_struct.sa_flags = SA_NODEFER; //doesn't prevent signal being received within the signal handler
        sigaction(SIGALRM, &action_struct, NULL); //retruns 0

        setjmp(TCB_array[0].buf);

        //all set up, move to next setting up threads
        firstCall = 1;
        setjmp(TCB_array[0].buf);
        TCB_array[0].thread_id = thread_count;
        TCB_array[0].thread_status = READY;

        TCB_array[0].stack = malloc(STACK_SIZE);
        TCB_array[0].stack += STACK_SIZE - 8;
        *(unsigned long int *)TCB_array[0].stack = (long unsigned int)&pthread_exit;

        //begin an alarm to start a new thread after allotted time
        ualarm(50000, 50000);
    }

    thread_count++;
    *threads = thread_count;

    setjmp(TCB_array[thread_count].buf);  //assign buffer to array

    void *rsp = malloc(STACK_SIZE);
    rsp = rsp + STACK_SIZE - 8;
    *(unsigned long int *)rsp = (long unsigned int)&pthread_exit;
    TCB_array[thread_count].stack = rsp;    

    TCB_array[thread_count].buf[0].__jmpbuf[JB_R13] = (unsigned long int)arg;
    TCB_array[thread_count].buf[0].__jmpbuf[JB_R12] = (long unsigned int)start_routine;
    TCB_array[thread_count].buf[0].__jmpbuf[JB_RSP] = ptr_mangle((unsigned long int)rsp);
    TCB_array[thread_count].buf[0].__jmpbuf[JB_PC] = ptr_mangle((unsigned long int)start_thunk);

    TCB_array[thread_count].thread_id = thread_count;
    TCB_array[thread_count].thread_status = READY;

    return 0;
}
