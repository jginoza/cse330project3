// Name: Jayden Ginoza and Cruise Costin
// Group: Jay and Cruise
// CSE330 Spring 2023
// Project 3: Memory Management

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/moduleparam.h>
#include <linux/timekeeping.h>
#include <linux/mm.h>
#include <linux/mmtypes.h>

static int pageSize = 4096;

// Define our global variables
static int pid = 1000; // User ID

// Pointers to task
//struct task_struct* task

// Define our module parameters
module_param(pid, int, 0);

//10-second timer
unsigned long timer_interval_ns = 10e9 // 10-second timer

enum hrtimer_restart no_restart_callback(struct hrtimer* timer)
{
    ktime_t currtime, interval;
    currtime = ktime_get();
    static int interval = ktime_set(0, timer_interval_ns);
    hrtimer_forward(timer, currtime, interval);
    // Do the measurement

    //rss - how many process pages in physical memory
    //swap - how many processes have been swapped to disk
    //wss - how many pages in working set
    //calculations done in memory manager

    //printk(KERN_INFO "PID %d: RSS=%lu KB, SWAP=%lu KB, WSS=%lu KB\n", pid, rss, swap, wss);

    return HRTIMER_NORESTART;
}

//Test and clear the accessed bit of a given pte entry. 
//vma is the pointer to the memory region
//addr is the address of the page
//ptep is a pointer to a pte. 
//It returns 1 if the pte was accessed, or 0 if not accessed. 

int ptep_test_and_clear_young(struct vm_area_struct* vma, unsigned long addr, pte_t* ptep)
{
    int ret = 0;
    if (pte_young(*ptep)) {
        ret = test_and_clear_bit(_PAGE_BIT_ACCESSED, (unsigned long*)&ptep->pte);
    }
    return ret;
}

//Memory Management - takes in PID as input
//iterate through all memory regions including mm_struct and vm_area_struct
int memory_manager(void) {
    //create the structures for task, mm, and vma
    struct task_struct* task;
    struct mm_struct* mm;
    struct vm_area_struct* vma;

    //set variables for iterating through all levels of memory
    pgd_t* pgd;
    p4d_t* p4d;
    pud_t* pud;
    pmd_t* pmd;
    pte_t* pte;

    printk(KERN_INFO "process pid=%d loaded\n", pid);

    /*
    task = pid_task(find_vpid(pid), PIDTYPE_PID); // get the task_struct of the process with the given pid
    */

    mm = task->mm; //set mm from mm_struct of process

    //timer setup and start for the looping of vma + 
    hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL); //HRTIMER_MODE_REL used for real time
    timer.function = &no_restart_callback;                  //set timer in regards to not restarting
    hrtimer_start(&timer, interval, HRTIMER_MODE_REL);      //begin timer with 

    //variables for the loop
    long start, end, address;
    int rss = 0; int swap = 0; int wss = 0;

    // loop for all vma
    for (vma = mm->mmap; vma; vma = vma->vm_next) {
        // loop for all pages in all vma
        for (address = vma->vm_start; address < vma->vm_end; address += pageSize) {
            pgd = pgd_offset(mm, address); //get pgd from mm + page addr
            if (pgd_none(*pgd) || pgd_bad(*pgd)) {
                return;
            }
            p4d = p4d_offset(pgd, address); //get p4d from pgd + page addr
            if (p4d_none(*p4d) || p4d_bad(*p4d)) {
                return;
            }
            pud = pud_offset(p4d, address); //get p4d from pgd + page addr
            if (pud_none(*pud) || pud_bad(*pud)) {
                return;
            }
            pmd = pmd_offset(pud, address); //get pmd from pud + page addr
            if (pmd_none(*pmd) || pmd_bad(*pmd)) {
                return;
            }
            pte = pte_offset_map(pmd, address); //get pte from pmd + page addr
            if (!ptep) {
                return;
            }
            //ret

            if (pte_present(*pte)) { //if the page is present in physical memory
                rss++;
                if (pte_pfn(*pte) == 0) { //if the page number is 0, then this is a new page (thus has been swapped)
                    swap++;
                }
            }

            //if the process page is in working set
            if (pte_young(*pte)) {
                wss++;
            }
            ptep_test_and_clear_young(&vma, address, &ptep); //following being found, clear pte     //may have to move, idk if it belongs here
        }

        //multiply by pageSize to make sure its in KB
        rss *= pageSize;
        wss *= pageSize;
        swap *= pageSize;

        printk(KERN_INFO "PID %d: RSS=%lu KB, SWAP=%lu KB, WSS=%lu KB\n", pid, rss, swap, wss);

        return 0; //show full execution
    }
}

void memory_exit(void) {
    hrtimer_cancel(&timer);     //turn off timer
}

// initialize module entry as memory_manager function
module_init(memory_manager);
// initialize module exit as memory_exit function 
module_exit(memory_exit);
//set license of the module to GPL
MODULE_LICENSE("GPL");