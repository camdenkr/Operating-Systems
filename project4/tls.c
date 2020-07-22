//Camden Kronhaus

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>

#include "tls.h"

#define HASH_SIZE 128

typedef struct thread_local_storage
{
    pthread_t tid;
    unsigned int size;     //size in bytes
    unsigned int page_num; //number of pages
    struct page **pages;   //array of pointers to pages
} TLS;

struct page
{
    unsigned long int address; //start address of page
    int ref_count;             //counter for shared pages
};

struct hash_element
{
    pthread_t tid;
    TLS *tls;
    struct hash_element *next;
};

//global list of hash_elements
struct hash_element *hash_table[HASH_SIZE];
int initialized = 0;
int page_size;
char *src;
char *dst;

void tls_protect(struct page *p)
{
    if (mprotect((void *)p->address, page_size, 0))
    {
        fprintf(stderr, "tls_protect: could not protect page\n");
        exit(1);
    }
}

void tls_unprotect(struct page *p)
{
    if (mprotect((void *)p->address, page_size, PROT_READ | PROT_WRITE))
    {
        fprintf(stderr, "tls_unprotect: could not unprotect page\n");
        exit(1);
    }
}

void tls_handle_page_fault(int sig, siginfo_t *si, void *context)
{
    pthread_t current_thread = pthread_self();
    int i;
    for (i = 0; i < HASH_SIZE; i++)
    {
        if (hash_table[i]->tid == current_thread)
        {
            break;
        }
    }

    page_size = getpagesize();
    unsigned int p_fault = ((unsigned long int)si->si_addr) & ~(page_size - 1);

    int j;
    for (j = 0; j < page_size; j++)
    {
        if (hash_table[j]->tls->pages[j]->address == p_fault)
        {
            pthread_exit(NULL);
        }
    }

    signal(SIGSEGV, SIG_DFL);
    signal(SIGBUS, SIG_DFL);
    raise(sig);
}

void tls_init()
{
    struct sigaction sigact;

    //get size of page
    page_size = getpagesize();

    //install the signal handler for page faults (SIGSEGV, SIGBUS)
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_SIGINFO; //use extended signal , distinguished between page fault and seg fault
    sigact.sa_sigaction = tls_handle_page_fault;

    sigaction(SIGBUS, &sigact, NULL);
    sigaction(SIGSEGV, &sigact, NULL);

    int i;
    for (i = 0; i < HASH_SIZE; i++)
    {
        hash_table[i] = calloc(1, sizeof(struct hash_element));
    }
    for (i = 0; i < HASH_SIZE; i++)
    {
        hash_table[i]->tid = 0;
    }

    initialized = 1;
}

int tls_create(unsigned int size)
{
    if (size <= 0)
    {
        return -1;
    }
    if (!initialized) //int if not done already
    {
        tls_init();
    }
    pthread_t current_thread = pthread_self(); //check current tid
    int k;
    //check hash table if there is a matching tid already, if so, LSA already exists, return -1
    int open_spot = 130;
    for (k = 0; k < HASH_SIZE; k++)
    {
        if (hash_table[k]->tid == current_thread)
        {
            return -1;
        }
        if (open_spot == 130) //only change once
        {
            if (hash_table[k]->tid == 0) //save to index into available space
            {
                open_spot = k;
            }
        }
    }

    TLS *tls;
    tls = (TLS *)calloc(1, sizeof(TLS));
    tls->tid = pthread_self();
    tls->size = size;
    tls->page_num = (size - 1) / page_size;
    tls->pages = calloc((tls->page_num)+1, sizeof(struct page));
    int i;
    for (i = 0; i <= tls->page_num; i++)
    {
        struct page *p;
        p = (struct page *)calloc(1, sizeof(struct page));
        p->address = (uintptr_t)mmap(0, page_size, 0, MAP_ANON | MAP_PRIVATE, 0, 0);
        p->ref_count = 1;
        tls->pages[i] = p;
    }
    hash_table[open_spot]->tid = current_thread;
    hash_table[open_spot]->tls = tls;
    return 0;
}

int tls_read(unsigned int offset, unsigned int length, char *buffer)
{
    if (!initialized)
    {
        return -1;
    }
    bool found = 0;
    unsigned int idx;
    pthread_t current_thread = pthread_self();
    int i;
    for (i = 0; i < HASH_SIZE; i++)
    {
        if (hash_table[i]->tid == current_thread)
        {
            found = 1;
            break;
        }
    }
    if (!found)
    {
        return -1;
    }

    if (offset + length > hash_table[i]->tls->size)
    {
        return -1;
    }
    int j;
    for (j = 0; j <= hash_table[i]->tls->page_num; j++)
    {
        tls_unprotect(hash_table[i]->tls->pages[j]);
    }
    int cnt;
    for (cnt = 0, idx = offset; idx < (offset + length); ++cnt, ++idx)
    {
        struct page *p;
        unsigned int pn, poff;
        pn = idx / page_size;
        poff = idx % page_size;
        p = hash_table[i]->tls->pages[pn];
        src = ((char *)p->address) + poff;
        buffer[cnt] = *src;
    }
    for (j = 0; j <= hash_table[i]->tls->page_num; j++)
    {
        tls_protect(hash_table[i]->tls->pages[j]);
    }

    return 0;
}

int tls_write(unsigned int offset, unsigned int length, char *buffer)
{
    //check if inititalized
    if (!initialized)
    {
        return -1;
    }
    unsigned int idx;
    pthread_t current_thread = pthread_self();
    int i;
    bool found = 0;
    //find lsa of current thread
    for (i = 0; i < HASH_SIZE; i++)
    {
        if (hash_table[i]->tid == current_thread) //lsa for current thread was found
        {
            found = 1;
            break;
        }
    }
    if (!found)
    {
        return -1;
    }

    int current_thread_location = i;

    //check to make sure offset+length can be put into lsa
    if (offset + length > hash_table[current_thread_location]->tls->size)
    {
        return -1;
    }

    //unprotect all pages belonging to thread's TLS
    int j;
    for (j = 0; j <= hash_table[current_thread_location]->tls->page_num; j++)
    {
        tls_unprotect(hash_table[current_thread_location]->tls->pages[j]);
    }
    /* perform the write operation */
    int cnt;
    for (cnt = 0, idx = offset; idx < (offset + length); ++cnt, ++idx)
    {
        struct page *p, *copy;
        unsigned int pn, poff;
        pn = idx / page_size;
        poff = idx % page_size;
        p = hash_table[current_thread_location]->tls->pages[pn];
        if (p->ref_count > 1)
        {
            copy = (struct page *)calloc(1, sizeof(struct page));
            copy->address = (uintptr_t)mmap(0, page_size, PROT_WRITE, MAP_ANON | MAP_PRIVATE, 0, 0);
            memcpy((void*)copy->address, (void*)p->address, page_size);
            copy->ref_count = 1;
            hash_table[current_thread_location]->tls->pages[pn] = copy;
            /* update original page */ 
            p->ref_count--;
            tls_protect(p);
            p = copy;
        }
        dst = ((char *)p->address) + poff;
        *dst = buffer[cnt];
    }

    for (j = 0; j <= hash_table[current_thread_location]->tls->page_num; j++)
    {
        tls_protect(hash_table[current_thread_location]->tls->pages[j]);
    }

    return 0;
}

int tls_destroy()
{
    if (!initialized)
    {
        return -1;
    }
    bool found = 0;
    pthread_t current_thread = pthread_self();
    int i;
    for (i = 0; i < HASH_SIZE; i++)
    {
        if (hash_table[i]->tid == current_thread)
        {
            found = 1;
            break;
        }
    }

    if (!found)
    {
        return -1;
    }

    int j;
    for (j = 0; j <= hash_table[i]->tls->page_num; j++)
    {
        if (hash_table[i]->tls->pages[j]->ref_count == 1)
        {
            free(hash_table[i]->tls->pages[j]);
        }
    }
    free(hash_table[i]->tls);
    free(hash_table[i]);

    //reinitialize area in hash_table
    hash_table[i] = calloc(1, sizeof(struct hash_element));
    hash_table[i]->tid = 0;

    return 0;
}

int tls_clone(pthread_t tid)
{
    if (!initialized)
    {
        return -1;
    }
    bool found = 0;
    pthread_t current_thread = pthread_self();
    //CURRENT THREAD
    //check if current thread has an LSA already - if so return -1
    int open_spot = 130;
    int i;
    for (i = 0; i < HASH_SIZE; i++)
    {
        if (open_spot == 130 && hash_table[i]->tid == 0)
        {
            open_spot = i;
        }

        if (hash_table[i]->tid == current_thread)
        {
            return -1;
        }
    }
    //TARGET THREAD
    //check if target thread has an LSA - if not return -1
    for (i = 0; i < HASH_SIZE; i++)
    {
        if (hash_table[i]->tid == tid)
        {
            found = 1;
            break;
        }
    }
    if (!found)
    {
        return -1;
    }
    int target_thread_location = i;

    //repeat of tls_create()
    TLS *tls;
    tls = (TLS *)calloc(1, sizeof(TLS));
    tls->tid = current_thread;
    tls->size = hash_table[target_thread_location]->tls->size;
    tls->page_num = hash_table[target_thread_location]->tls->page_num;
    tls->pages = calloc((tls->page_num)+1, sizeof(struct page));
    for (i = 0; i <= tls->page_num; i++)
    {
        struct page *p;
        p = (struct page *)calloc(1, sizeof(struct page));
        p->address = (uintptr_t)mmap(0, page_size, 0, MAP_ANON | MAP_PRIVATE, 0, 0);
        p->ref_count = 0;
        tls->pages[i] = p;
    }
    hash_table[open_spot]->tid = current_thread;
    hash_table[open_spot]->tls = tls;


    int k;
    for (k = 0; k <= hash_table[target_thread_location]->tls->page_num; k++)
    {
        hash_table[open_spot]->tls->pages[k] = hash_table[target_thread_location]->tls->pages[k];
        //hash_table[open_spot]->tls->pages[k]->address = hash_table[target_thread_location]->tls->pages[k]->address;
        (hash_table[open_spot]->tls->pages[k]->ref_count)++;
    }
    return 0;
}