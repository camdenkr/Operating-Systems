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

#include <semaphore.h>

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
#define BLOCKED 3
#define WAITING 4

typedef struct
{
    pthread_t thread_id; //thread id
    jmp_buf buf;         //state of thread (its registers) //array if 8 64-ints
    void *stack;         //information about stack (pointer to stack area)
    int thread_status;   //information about status (ready 0, running 1, or exited 2)
    void *value_ptr;     //saves return value from pthread_exit
    pthread_t targetThread;
} TCB;

TCB TCB_array[128];
jmp_buf buf;

jmp_buf scheduler_buf;
pthread_t thread_count = 0;
pthread_t current_thread = 0;

bool firstCall = 0;
int schedulerIndex = 0;

sigset_t blockSignal;
//sigset add changes

struct Node
{
    pthread_t pthread_id;
    struct Node *next;
};

struct sema_struct
{
    struct Node *head;
    int count;
    bool init;
};

int SemaCheck = 0;

//the current value
//a pointer to a head for threads that are waiting
//a flag that indicates whether the semaphore is initialized

void lock()
{ //when called a thread can no longer be interrupted
    // int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
    sigaddset(&blockSignal, SIGALRM);
    sigprocmask(SIG_BLOCK, &blockSignal, NULL);
}

void unlock()
{ //when called, a locked thread resumes normal activity
    sigprocmask(SIG_UNBLOCK, &blockSignal, NULL);
}

static void scheduler(int signal)
{
    lock();
    if (TCB_array[current_thread].thread_status == RUNNING) //set running thread back to ready if not exited
    {
        TCB_array[current_thread].thread_status = READY;
    }
    schedulerIndex++; //start with next thread, if it was the last one, loop back to first thread and start from there
    if (schedulerIndex > thread_count)
    {
        schedulerIndex = 0;
    }
    unlock();
    if (!setjmp(TCB_array[current_thread].buf))
    {
        lock();
        int i = 0;
        if (!SemaCheck) //sema shoudl take priority to be scheduled rn
        {
            while (i <= thread_count) //check for blocks first
            {
                if (TCB_array[i].thread_status == BLOCKED && TCB_array[TCB_array[i].targetThread].thread_status == EXITED)
                {
                    TCB_array[i].thread_status = RUNNING;
                    current_thread = TCB_array[i].thread_id;
                    unlock();
                    longjmp(TCB_array[i].buf, 1);
                }
                i++;
            }
        }
        while (1) //schedulerIndex <= thread_count
        {
            SemaCheck = 0;
            if (TCB_array[schedulerIndex].thread_status == BLOCKED && TCB_array[TCB_array[schedulerIndex].targetThread].thread_status == EXITED)
            {
                TCB_array[schedulerIndex].thread_status = RUNNING;
                current_thread = TCB_array[schedulerIndex].thread_id;
                unlock();
                longjmp(TCB_array[schedulerIndex].buf, 1);
            }
            if (TCB_array[schedulerIndex].thread_status == READY)
            {
                TCB_array[schedulerIndex].thread_status = RUNNING;
                current_thread = TCB_array[schedulerIndex].thread_id;
                unlock();
                longjmp(TCB_array[schedulerIndex].buf, 1);
            }
            if (schedulerIndex >= thread_count)
            {
                schedulerIndex = 0;
                continue; //skip increment after this
            }
            schedulerIndex++;
        }
    }
    SemaCheck = 0;
}

int sem_init(sem_t *sem, int pshared, unsigned value)
{
    lock();
    struct sema_struct *sema = (struct sema_struct *)malloc(sizeof(struct sema_struct));
    sema->count = (int)value;
    sema->head = NULL; //can't initialize size
    sema->init = 1;
    sem->__align = (long int)sema;
    unlock();
    return 0;
}

int sem_wait(sem_t *sem)
{
    lock();
    struct sema_struct *sema = (struct sema_struct *)sem->__align;
    if (sema->count > 0) //decrement if > 0
    {
        (sema->count)--; //sema positive
        unlock();
        return 0;
    }
    else if (sema->count == 0) //sema can't be negative, wait until it can be reduced, add it to the queue
    {
        struct Node *newNode = (struct Node *)malloc(sizeof(struct Node)); //create new Node and init values
        newNode->pthread_id = current_thread;
        newNode->next = NULL;
        struct Node *curr = sema->head; //set temporary current pointer to traverse head
        if (curr == NULL)               //sema doesn't have a head, queue empty
        {
            sema->head = newNode;
        }
        else
        { //add to last
            while (curr->next != NULL)
            {
                curr = curr->next;
            }
            curr->next = newNode; //curr->next is null, assign to new node
        }
        TCB_array[current_thread].thread_status = WAITING;
        unlock();
        scheduler(0);
        lock();
        (sema->count)--;
        unlock();
        return 0;
    }
    else //error
    {
        unlock();
        return -1;
    }
}

int sem_post(sem_t *sem)
{
    lock();
    struct sema_struct *sema = (struct sema_struct *)sem->__align;
    if (sema->count > 0)
    {
        (sema->count)++;
        unlock();
        return 0;
    } //else == 0
    else if (sema->count == 0)
    {
        if (sema->head == NULL) //nothing in head, just increment, shouldn't reach this
        {
            (sema->count)++;
            unlock();
            return 0;
        }
        struct Node *currHead = sema->head;
        schedulerIndex = (currHead->pthread_id) - 1;             //we want scheduler to schedule this thread first
        current_thread = currHead->pthread_id;
        TCB_array[currHead->pthread_id].thread_status = READY; //thread can now be scheduled
        //get rid of head of head
        if (sema->head->next != NULL) //head is the only one so free head only
        {
            sema->head = sema->head->next;
            free(currHead);
        }
        else //only a head in the queue
        {
            free(currHead);
        }
        SemaCheck = 1;
        (sema->count)++;
        unlock();
        scheduler(0);
        //unlock();
        return 0;
    }
    else //error
    {
        unlock();
        return -1;
    }
}

int sem_destroy(sem_t *sem)
{
    lock();
    struct sema_struct *sema = (struct sema_struct *)sem->__align;
    struct Node *curr = sema->head;
    struct Node *temp = sema->head;
    if (curr == NULL)
    {
        free(sema);
        unlock();
        return 0;
    }
    if (curr->next == NULL)
    {
        free(curr);
        free(sema);
        unlock();
        return 0;
    }
    else
    {
        while (curr->next != NULL)
        {
            temp = curr->next;
            free(curr);
            curr = temp;
        }
        free(curr);
        free(sema);
        unlock();
        return 0;
    }
}

//This function will initialize an unnamed semaphore referred to by sem.
//The pshared argument always equals to 0, which means that the semaphore pointed to by sem is shared between threads of the process.
//Attempting to initialize an already initialized semaphore results in undefined behavior

int pthread_join(pthread_t thread, void **value_ptr)
{
    if (TCB_array[thread].thread_status != EXITED) //target thread hasn't exited
    {
        lock();
        TCB_array[current_thread].thread_status = BLOCKED; //current thread becomes blocked
        TCB_array[current_thread].targetThread = thread;   //target thread saved in TCB
        unlock();
        scheduler(0);
    }
    if (value_ptr != NULL)
    {
        lock();
        *value_ptr = TCB_array[thread].value_ptr;
        unlock();
    }
    return 0;
}

void pthread_exit(void *value_ptr)
{
    lock();
    TCB_array[current_thread].thread_status = EXITED;
    TCB_array[current_thread].value_ptr = value_ptr;
    unlock();
    scheduler(0);
    __builtin_unreachable();
}

void pthread_exit_wrapper()
{
    unsigned long int res;
    asm("movq %%rax, %0\n"
        : "=r"(res));
    pthread_exit((void *)res);
}

int pthread_create(pthread_t *threads, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg)
{
    lock();
    if (!firstCall)
    {
        *threads = thread_count;
        struct sigaction action_struct;
        action_struct.sa_handler = scheduler;

        sigemptyset(&action_struct.sa_mask);

        action_struct.sa_flags = SA_NODEFER;
        sigaction(SIGALRM, &action_struct, NULL);

        setjmp(TCB_array[0].buf);

        //all set up, move to next setting up threads
        firstCall = 1;
        setjmp(TCB_array[0].buf);
        //TCB_array[0].thread_id = thread_count;
        TCB_array[0].thread_status = READY;
        TCB_array[0].targetThread = 999;

        ualarm(50000, 50000);
    }
    thread_count++; //now threadcount is 1+
    *threads = thread_count;

    setjmp(TCB_array[thread_count].buf); //assign buffer to array

    void *rsp = malloc(STACK_SIZE);
    rsp = rsp + STACK_SIZE - 8;
    *(unsigned long int *)rsp = (long unsigned int)&pthread_exit_wrapper;
    TCB_array[thread_count].stack = rsp;

    TCB_array[thread_count].targetThread = 999;

    TCB_array[thread_count].buf[0].__jmpbuf[JB_R13] = (unsigned long int)arg;
    TCB_array[thread_count].buf[0].__jmpbuf[JB_R12] = (long unsigned int)start_routine;
    //TCB_array[thread_count].stack
    TCB_array[thread_count].buf[0].__jmpbuf[JB_RSP] = ptr_mangle((unsigned long int)TCB_array[thread_count].stack);
    TCB_array[thread_count].buf[0].__jmpbuf[JB_PC] = ptr_mangle((unsigned long int)start_thunk);

    TCB_array[thread_count].thread_id = thread_count;
    TCB_array[thread_count].thread_status = READY;
    unlock();
    return 0;
}

pthread_t pthread_self(void)
{
    return TCB_array[(int)current_thread].thread_id;
}
