#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

#include <linux/sched.h> // [jh] task_struct, real_parent
#include <linux/pid.h> // [jh] pid_task(), find_vpid()

MODULE_LICENSE("GPL");

#define MAX_DEPTH 64
#define BUF_SIZE 4096

static struct dentry *dir, *inputdir, *ptreedir;

// [jh] input에 쓰면 결과를 여기 만들어두고 ptree 읽을 때 그대로 돌려줌
static DEFINE_MUTEX(ptree_lock);
static char output_buf[BUF_SIZE];
static size_t output_size;

// [jh] pid를 받아서 init(1)까지 부모를 따라 올라간 뒤
// init -> ... -> pid 순서로 "comm (pid)" 한 줄씩
static ssize_t write_pid_to_input(struct file *fp,
				  const char __user *user_buffer,
				  size_t length,
				  loff_t *position)
{
	struct task_struct *task_list[MAX_DEPTH];
	struct task_struct *task;
	char buf[32];
	pid_t input_pid;
	int count = 0;
	int i;

	if (length == 0 || length >= sizeof(buf))
		return -EINVAL;
	if (copy_from_user(buf, user_buffer, length))
		return -EFAULT;
	buf[length] = '\0';

	if (kstrtoint(buf, 10, &input_pid) < 0)     // echo 끝의 \n은 kstrtoint가 알아서 무시
		return -EINVAL;

	mutex_lock(&ptree_lock);
	output_size = 0;

	rcu_read_lock();
	task = pid_task(find_vpid(input_pid), PIDTYPE_PID);
	if (!task) {
		rcu_read_unlock();
		output_size = scnprintf(output_buf, BUF_SIZE, "Invalid PID: %d\n", input_pid);
		mutex_unlock(&ptree_lock);
		return length;
	}

	// ptrace 중이면 parent가 바뀌므로 real_parent 기준
	while (count < MAX_DEPTH) {
		task_list[count++] = task;
		if (task->pid <= 1 || task->real_parent == task)
			break;
		task = task->real_parent;
	}

	for (i = count - 1; i >= 0; i--) {
		output_size += scnprintf(output_buf + output_size, BUF_SIZE - output_size,
					 "%s (%d)\n", task_list[i]->comm, task_list[i]->pid);
	}
	rcu_read_unlock();

	mutex_unlock(&ptree_lock);
	return length;
}

// [jh] read ptree
static ssize_t read_ptree(struct file *fp,
			  char __user *user_buffer,
			  size_t length,
			  loff_t *position)
{
	static const char no_data[] = "No data (write pid to input first)\n";
	ssize_t ret;

	mutex_lock(&ptree_lock);
	if (output_size == 0)
		ret = simple_read_from_buffer(user_buffer, length, position,
					      no_data, sizeof(no_data) - 1);
	else
		ret = simple_read_from_buffer(user_buffer, length, position,
					      output_buf, output_size);
	mutex_unlock(&ptree_lock);

	return ret;
}

static const struct file_operations dbfs_fops_input = {
	.owner = THIS_MODULE,
	.write = write_pid_to_input,
};

static const struct file_operations dbfs_fops_ptree = {
	.owner = THIS_MODULE,
	.read = read_ptree,
};

static int __init dbfs_module_init(void)
{
	dir = debugfs_create_dir("ptree", NULL);
	if (IS_ERR_OR_NULL(dir)) {
		pr_err("Cannot create ptree dir\n");
		return -ENODEV;
	}

	inputdir = debugfs_create_file("input", 0222, dir, NULL, &dbfs_fops_input);
	ptreedir = debugfs_create_file("ptree", 0444, dir, NULL, &dbfs_fops_ptree);
	if (IS_ERR_OR_NULL(inputdir) || IS_ERR_OR_NULL(ptreedir)) {
		pr_err("Cannot create ptree files\n");
		debugfs_remove_recursive(dir);
		return -ENODEV;
	}

	pr_info("dbfs_ptree module initialize done\n");
	return 0;
}

static void __exit dbfs_module_exit(void)
{
	debugfs_remove_recursive(dir);
	pr_info("dbfs_ptree module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
