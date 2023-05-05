// Name: Jayden Ginoza and Cruise Costin
// Group: Jay and Cruise
// CSE330 Spring 2023
// Project 3: Memory Management

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/pid.h>

// Define our global variables
static int pid = 1000; // User ID
pte_t* ptep, pte;

//variables for the loop + calculations
unsigned long addr;
int rss = 0;
int swaps = 0;
int wss = 0;

//create the structures for task, mm, and vma
struct task_struct* process;
struct mm_struct* mm;
struct vm_area_struct* mmap;

// Define our module parameters
module_param(pid, int, 0644);

//10-second timer
unsigned long timer_interval_ns = 10e9 // 10-second timer
struct hrtimer timer;

//function runs through the five levels to find if pages are present/swapped
int fiveLevel(struct mm_struct* mm, unsigned long addr) {
    pgd_t* pgd;
    p4d_t* p4d;
    pmd_t* pmd;
    pud_t* pud;

    pgd = pgd_offset(mm, addr); // get pgd from mm and the page address
    if (pgd_none(*pgd) || pgd_bad(*pgd)) {
        // check if pgd is bad or does not exist
        return 0;
    }
    p4d = p4d_offset(pgd, addr); //get p4d from from pgd and the page addr
    if (p4d_none(*p4d) || p4d_bad(*p4d)) {
        // check if p4d is bad or does not exist
        return 0;
    }
    pud = pud_offset(p4d, addr); // get pud from from p4d and the page addr
    if (pud_none(*pud) || pud_bad(*pud)) {
        // check if pud is bad or does not exist
        return 0;
    }
    pmd = pmd_offset(pud, addr); // get pmd from from pud and the page addr
    if (pmd_none(*pmd) || pmd_bad(*pmd)) {
        // check if pmd is bad or does not exist
        return 0;
    }
    ptep = pte_offset_map(pmd, addr); // get pte from pmd and the page address
    if (!ptep) {
        // check if pte does not exist
        return 0;
    }
    pte = *ptep;
    if (pte_present(pte)) {
        rss++;
        return 1;
    }
    swaps++;
    return 0;
}


//Test and clear the accessed bit of a given pte entry. 
//vma is the pointer to the memory region
//addr is the address of the page
//ptep is a pointer to a pte. 
//It returns 1 if the pte was accessed, or 0 if not accessed. 

int ptep_test_and_clear_young(struct vm_area_struct* vma, unsigned long addr, pte_t* ptep) {
    int ret = 0;
    if (pte_young(*ptep)) {
        ret = test_and_clear_bit(_PAGE_BIT_ACCESSED, (unsigned long*)&ptep->pte);
    }
    return ret;
}

enum hrtimer_restart timer_callback(struct hrtimer* timer) {
    ktime_t currtime, interval;
    currtime = ktime_get();
    interval = ktime_set(0, timer_interval_ns);
    hrtimer_forward(timer, currtime, interval);

    mmap = process->mm->mmap;       //gets vma from process
    mm = process->mm;               //gets mm struct from process

    //must reset all var before loops
    rss = 0;
    swaps = 0;
    wss = 0;

    //loops all vma and its pages to search for bits and pages for rss, wss, swap
    while (mmap) {
        for (addr = mmap->vm_start; addr < mmap->vm_end; addr += PAGE_SIZE) {
            fiveLevel(mm, addr); //function checks if pages are in physical memory (rss) and if not then were swapped (swap)
            if (ptep_test_and_clear_young(mmap, addr, ptep)) { //if the process page is found, thus in working set (wss)
                wss++;
            }
        }
        mmap = mmap->vm_next;
    }

    //multiplied to match the rss from the expected 
        //think assumes page size = 1024 thus requiring x4 to match 4096 typical page size in KB
    rss *= 4;
    swaps *= 4;
    wss *= 4;
    printk("PID [%d]: RSS=%d KB, SWAP=%d KB, WSS=%d KB\n", pid, rss, swaps, wss);

    return HRTIMER_RESTART;
}

//Memory Management - takes in PID as input
//iterate through all memory regions including mm_struct and vm_area_struct
int memory_manager(void) {

    process = pid_task(find_vpid(pid), PIDTYPE_PID); //task struct of the process using pid

    //timer setup and start for the looping of vma
    hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    timer.function = &timer_callback;
    hrtimer_start(&timer, timer_interval_ns, HRTIMER_MODE_REL);

    return 0;
}

void mm_exit(void) {
    hrtimer_cancel(&timer);     //turn off the timer
    printk("Exiting\n");
}

// initialize module entry as memory_manager function
module_init(memory_manager);
// initialize module exit as memory_exit function 
module_exit(mm_exit);
//set license of the module to GPL
MODULE_LICENSE("GPL");