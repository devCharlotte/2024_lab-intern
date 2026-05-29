#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <asm/pgtable.h>

#include <linux/mm.h>
#include <linux/highmem.h>

MODULE_LICENSE("GPL");

static struct dentry *dir, *output;
static struct task_struct *task;

// [jh] app.c packet
struct packet {
	pid_t pid;
        unsigned long vaddr;
        unsigned long paddr;
};

static struct packet pckt;


static ssize_t read_output(struct file *fp,
                        char __user *user_buffer,
                        size_t length,
                        loff_t *position)
{
        // Implement read file operation
	// [jh]	
	
	struct task_struct *task;
    	struct mm_struct *mm;
   	pgd_t *pgd;
    	p4d_t *p4d;
    	pud_t *pud;
    	pmd_t *pmd;
    	pte_t *pte;
    	unsigned long vaddr = pckt.vaddr;
    	unsigned long pfn;
	
	if (*position > 0)
        	return 0;

    // read packet from user
    if (copy_from_user(&pckt, user_buffer, sizeof(pckt)))
        return -EFAULT;

    vaddr = pckt.vaddr;

    // [jh] find task using pid
    rcu_read_lock();
    task = pid_task(find_vpid(pckt.pid), PIDTYPE_PID);
    if (!task) {
        rcu_read_unlock();
        return -EINVAL;
    }

    mm = task->mm;
    rcu_read_unlock();
    if (!mm)
        return -EINVAL;

    // [jh]
    pgd = pgd_offset(mm, vaddr);
    if (pgd_none(*pgd) || pgd_bad(*pgd)) return -EINVAL;

    p4d = p4d_offset(pgd, vaddr);
    if (p4d_none(*p4d) || p4d_bad(*p4d)) return -EINVAL;

    pud = pud_offset(p4d, vaddr);
    if (pud_none(*pud) || pud_bad(*pud)) return -EINVAL;

    pmd = pmd_offset(pud, vaddr);
    if (pmd_none(*pmd) || pmd_bad(*pmd)) return -EINVAL;

    // pte_offset_map은 6.5부터 모듈에서 못 씀 (x86_64는 highmem이 없어서 kernel 버전으로 충분)
    pte = pte_offset_kernel(pmd, vaddr);
    if (!pte) return -EINVAL;

    if (!pte_present(*pte))
        return -EINVAL;

    // [jh] PA
    pfn = pte_pfn(*pte);

    pckt.paddr = (pfn << PAGE_SHIFT) | (vaddr & (PAGE_SIZE - 1));

    // [jh] kernel -> user
    if (copy_to_user(user_buffer, &pckt, sizeof(pckt)))
        return -EFAULT;

    *position = sizeof(pckt);
    return sizeof(pckt);
}




// [jh]
static const struct file_operations fops_output = {
        // Mapping file operations with your functions
        // [jh]
        .read = read_output,
};





static int __init dbfs_module_init(void)
{
        // Implement init module

#if 0
        dir = debugfs_create_dir("paddr", NULL);

        if (!dir) {
                printk("Cannot create paddr dir\n");
                return -1;
        }

        // Fill in the arguments below
        output = debugfs_create_file("output", , , , );
#endif


	// [jh]
	dir = debugfs_create_dir("paddr", NULL);

	debugfs_create_file("output", 0666, dir, NULL, &fops_output);

	printk("dbfs_paddr module initialize done\n");

        return 0;
}



static void __exit dbfs_module_exit(void)
{
        // Implement exit module
	
	// [jh]
	debugfs_remove_recursive(dir);

	printk("dbfs_paddr module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
