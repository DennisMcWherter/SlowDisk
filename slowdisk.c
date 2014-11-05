/**
 * slowdisk.c
 *
 * Module to slow-down disk accesses (i.e. reads and writes)
 * artificially. Accesses are close to specified cycle count.
 * That is, testing for loop condition to be completed is not
 * counted in cycle count.
 *
 * Author: Dennis J. McWherter, Jr. (dmcwherter@yahoo-inc.com)
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/random.h>
#include <linux/syscalls.h>
#include <linux/types.h>
#include <asm/cacheflush.h>
#include <asm/paravirt.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>

/** Read/Write function signature */
typedef long(*rw_fn_t)(unsigned int, const char __user*, size_t);

/**
 * Useful variables for kernel module.
 * Includes:
 *  - Wait time ranges
 *  - Original system call addresses
 *  - System call table address
 */
static ulong minWait = 500L;
static ulong maxWait = 1000L;
static ulong* sys_call_table = NULL;

/** System call hook prototypes */
asmlinkage rw_fn_t orig_sys_write;
asmlinkage rw_fn_t orig_sys_read;
asmlinkage long write_hook(unsigned int, const char __user*, size_t);
asmlinkage long read_hook(unsigned int, const char __user*, size_t);

/** Helper prototypes */
void get_syscall_table(void);

/**
 * Initializer for kernel module
 */
static int __init slowdisk_init(void) {
  ulong cr0;

  if(minWait < 0 || minWait > maxWait) {
    printk(KERN_WARNING "Invalid [minWait, maxWait] range provided. Normalizing to: [500, 1000]");
    minWait = 500L;
    maxWait = 1000L;
  } else if(maxWait == 0) {
    printk(KERN_WARNING "Setting maxWait to 1. Interval is now [0,1).");
    maxWait = 1L;
  }

  /** So this is annoying... Let's get the syscall table. */
  get_syscall_table();

  if(sys_call_table == NULL) {
    printk(KERN_WARNING "Could not find the address to sys_call_table!");
    return -1;
  }

  /** We want to write/hook into our syscalls */
  /** Clear bit 16 of the cr0 register to write to read-only pages (x86) */
  orig_sys_read  = (rw_fn_t)sys_call_table[__NR_read];
  orig_sys_write = (rw_fn_t)sys_call_table[__NR_write];
  cr0 = read_cr0();
  write_cr0(cr0 & ~0x10000); /** Write to read-only pages... */
  sys_call_table[__NR_write] = (ulong)write_hook;
  sys_call_table[__NR_read] = (ulong)read_hook;
  write_cr0(cr0); /** Restore state of cr0 register. */

  printk(KERN_INFO "Successfully loaded SlowDisk module with interval [%ld,%ld].", minWait, maxWait);

  maxWait++; /** We want this to be inclusive! We only occur the addition cost once this way. */

  return 0;
}

/**
 * Exit routine/cleanup for kernel module
 */
static void __exit slowdisk_exit(void) {
  ulong cr0;

  /** Restore our system calls to normal. */
  cr0 = read_cr0();
  write_cr0(cr0 & ~0x10000);
  sys_call_table[__NR_write] = (ulong)orig_sys_write;
  sys_call_table[__NR_read]  = (ulong)orig_sys_read;
  write_cr0(cr0);

  printk(KERN_INFO "Successfully unloaded SlowDisk module.");
}

/**
 * Method to choose a random wait time based on provided
 * wait interval. After selecting a wait time, it runs
 * some artificial work to slow down the process.
 *
 * @return  Number of *loop* cycles waited for.
 */
static ulong random_wait(void) {
  ulong waitInterval = maxWait - minWait;
  ulong wait;
  ulong i;
  volatile ulong dumbSum = 0;
  get_random_bytes(&wait, sizeof(ulong));
  wait = (wait % waitInterval) + minWait;
  for(i = 0 ; i < wait ; ++i) {
    /** Artificial work to make this use cycles */
    dumbSum += i;
    dumbSum *= i/2;
  }
  return wait;
}

/**
 * Get the sys_call_table by searching memory for it -.-
 *
 * This method fills in the corresponding sys_call_table global
 * var with the result.
 *
 * NOTE: This is necessary since the Linux kernel does not export
 *       the address of sys_call_table for modules to easily access
 *       :(
 */
void get_syscall_table(void) {
  sys_call_table = (ulong*)PAGE_OFFSET; /** Alignment */
  while((ulong)sys_call_table < ULONG_MAX) {
    /** Our choice of sys_close is because it's address is available to the kernel :) */
    if(sys_call_table[__NR_close] == (ulong)sys_close) {
      return; /** Found the correct offset! */
    }
    sys_call_table++;
  }
  sys_call_table = NULL; /** Couldn't find sys_call_table :S */
}

/**
 * Write hook to introduce artifical delay into writes.
 *
 * @return  Result from actual write syscall.
 */
asmlinkage long write_hook(unsigned int fd, const char __user* buf, size_t count) {
  random_wait();
  return orig_sys_write(fd, buf, count);
}

/**
 * Read hook to introduce artificial delay into reads.
 *
 * @return  Result from actual read system call.
 */
asmlinkage long read_hook(unsigned int fd, const char __user* buf, size_t count) {
  random_wait();
  return orig_sys_read(fd, buf, count);
}

/** Kernel module boilerplate */
module_param(minWait, ulong, 0);
MODULE_PARM_DESC(minWait, "Minimum cycle wait per disk access.");
module_param(maxWait, ulong, 0);
MODULE_PARM_DESC(maxWait, "Maximum cycle wait per disk access.");

module_init(slowdisk_init);
module_exit(slowdisk_exit);

MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
MODULE_DESCRIPTION("Kernel module to slow disk accesses.");
MODULE_AUTHOR("Dennis J. McWherter, Jr.");

