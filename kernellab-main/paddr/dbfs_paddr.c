#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/sched/task.h>
#include <linux/pid.h>
#include <asm/pgtable.h>

MODULE_LICENSE("GPL");

static struct dentry *dir, *output;

// [jh] app.c 랑 같은 구조체
struct packet {
	pid_t pid;
	unsigned long vaddr;
	unsigned long paddr;
};

// [jh] mm의 page table을 pgd -> p4d -> pud -> pmd -> pte 순서로 따라감
// huge page(1G / 2M)면 중간 단계에서 바로 계산, mmap lock 잡은 상태에서 호출
static int vaddr_to_paddr(struct mm_struct *mm, unsigned long vaddr,
			  unsigned long *paddr)
{
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	pgd = pgd_offset(mm, vaddr);
	if (pgd_none(*pgd) || pgd_bad(*pgd))
		return -EFAULT;

	p4d = p4d_offset(pgd, vaddr);
	if (p4d_none(*p4d) || p4d_bad(*p4d))
		return -EFAULT;

	pud = pud_offset(p4d, vaddr);
	if (pud_none(*pud))
		return -EFAULT;
	if (pud_leaf(*pud)) {
		*paddr = (pud_pfn(*pud) << PAGE_SHIFT) | (vaddr & ~PUD_MASK);
		return 0;
	}
	if (pud_bad(*pud))
		return -EFAULT;

	pmd = pmd_offset(pud, vaddr);
	if (pmd_none(*pmd))
		return -EFAULT;
	if (pmd_leaf(*pmd)) {
		*paddr = (pmd_pfn(*pmd) << PAGE_SHIFT) | (vaddr & ~PMD_MASK);
		return 0;
	}
	if (pmd_bad(*pmd))
		return -EFAULT;

	// pte_offset_map은 6.5부터 모듈에서 못 씀, x86_64는 highmem이 없어서 kernel 쪽으로 충분
	pte = pte_offset_kernel(pmd, vaddr);
	if (!pte_present(*pte))
		return -EFAULT;

	*paddr = (pte_pfn(*pte) << PAGE_SHIFT) | (vaddr & ~PAGE_MASK);
	return 0;
}

// [jh] user가 넘긴 packet(pid, vaddr)에 paddr 채워서 돌려줌
static ssize_t read_output(struct file *fp,
			   char __user *user_buffer,
			   size_t length,
			   loff_t *position)
{
	struct packet pckt;
	struct task_struct *task;
	struct mm_struct *mm;
	int ret;

	if (length < sizeof(pckt))
		return -EINVAL;
	if (copy_from_user(&pckt, user_buffer, sizeof(pckt)))
		return -EFAULT;

	// pid -> task_struct -> mm (참조 잡아두고 씀)
	rcu_read_lock();
	task = pid_task(find_vpid(pckt.pid), PIDTYPE_PID);
	if (task)
		get_task_struct(task);
	rcu_read_unlock();
	if (!task)
		return -ESRCH;

	mm = get_task_mm(task);
	put_task_struct(task);
	if (!mm)
		return -EINVAL;     // 커널 스레드

	mmap_read_lock(mm);
	ret = vaddr_to_paddr(mm, pckt.vaddr, &pckt.paddr);
	mmap_read_unlock(mm);
	mmput(mm);
	if (ret)
		return ret;

	if (copy_to_user(user_buffer, &pckt, sizeof(pckt)))
		return -EFAULT;
	return sizeof(pckt);
}

static const struct file_operations fops_output = {
	.owner = THIS_MODULE,
	.read = read_output,
};

static int __init dbfs_module_init(void)
{
	dir = debugfs_create_dir("paddr", NULL);
	if (IS_ERR_OR_NULL(dir)) {
		pr_err("Cannot create paddr dir\n");
		return -ENODEV;
	}

	output = debugfs_create_file("output", 0644, dir, NULL, &fops_output);
	if (IS_ERR_OR_NULL(output)) {
		pr_err("Cannot create paddr/output\n");
		debugfs_remove_recursive(dir);
		return -ENODEV;
	}

	pr_info("dbfs_paddr module initialize done\n");
	return 0;
}

static void __exit dbfs_module_exit(void)
{
	debugfs_remove_recursive(dir);
	pr_info("dbfs_paddr module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
