#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#include <linux/sched.h> // [jh] task_struct, parent
#include <linux/pid.h> // [jh] pid_task(), find_vpid()


MODULE_LICENSE("GPL");

static struct dentry *dir, *inputdir, *ptreedir;   // [jh] dir, file pointer
static struct task_struct *curr;  

static pid_t stored_pid = -1;  // [jh] user input pid

static char output_buf[4096]; // [jh] buffer 
  
static size_t output_size = 0; 

// [jh] output ordering
#define MAX_DEPTH 32
static struct task_struct *task_list[MAX_DEPTH];
static int task_count = 0;


// [jh] write -> input file 
// user input pid -> create process tree 
static ssize_t write_pid_to_input(struct file *fp, 
                                const char __user *user_buffer, 
                                size_t length, 
                                loff_t *position)
{
        pid_t input_pid;

        //sscanf(user_buffer, "%u", &input_pid);
        //curr = // Find task_struct using input_pid. Hint: pid_task   
	// [jh] >> linux/pid.h (task_struct, parent), sched.h (pid_task, find_vpid) 

        // Tracing process tree from input_pid to init(1) process

        // Make Output Format string: process_command (process_id)


	char buf[32];
	int i;

	// [jh] get pid from user space
	if (length >= sizeof(buf))
		return -EINVAL;
	if (copy_from_user(buf, user_buffer, length))
        return -EFAULT;
    	buf[length] = '\0';
    

	// [jh] str -> int pid
	if (kstrtoint(buf, 10, &input_pid) < 0)
        return -EINVAL;

    	stored_pid = input_pid;


	// [jh] initialization
	memset(output_buf, 0, sizeof(output_buf));
    	output_size = 0;
	
	// [jh] find task_struct using pid
	// find_vpid -> pid_task (rcu 안에서)
	rcu_read_lock();
	struct pid *pid_struct = find_vpid(stored_pid);
    	struct task_struct *task = pid_task(pid_struct, PIDTYPE_PID);

    	if (!task) {
		rcu_read_unlock();
		task_count = 0;
        	output_size = scnprintf(output_buf, sizeof(output_buf),
                 	               "Invalid PID: %d\n", stored_pid);
        	return length;
    	}


	// [jh] output ordering
	task_count = 0;

    	while (task && task_count < MAX_DEPTH) {
        	task_list[task_count++] = task;

       		if (task->pid == 1)
           		break;
		
        	task = task->parent;
    	}

	// [jh] init부터 출력되게 거꾸로, task가 사라질 수 있으니 여기서 문자열로 만들어 둠
	for (i = task_count - 1; i >= 0; i--) {
		output_size += scnprintf(output_buf + output_size,
					 sizeof(output_buf) - output_size,
					 "%s (%d)\n",
					 task_list[i]->comm, task_list[i]->pid);
	}
	rcu_read_unlock();

   	return length;
}
	


// [jh] read ptree
static ssize_t read_ptree(struct file *fp,
                          char __user *user_buffer,
                          size_t length,
                          loff_t *position)
{
	static const char no_data[] = "Invalid PID or No data\n";

    	if (output_size == 0)
		return simple_read_from_buffer(user_buffer, length, position,
					       no_data, sizeof(no_data) - 1);

    	// [jh] write에서 만들어 둔 결과 그대로 전달
	return simple_read_from_buffer(user_buffer, length, position,
				       output_buf, output_size);
}



static const struct file_operations dbfs_fops_input = {   // [jh]
        .write = write_pid_to_input,
};

// [jh]
static const struct file_operations dbfs_fops_ptree = {
    .read = read_ptree,
};


// [jh] kernel module initialization ... create debugfs dir, file
static int __init dbfs_module_init(void)
{
        // Implement init module code

#if 0
        dir = debugfs_create_dir("ptree", NULL);
        
        if (!dir) {
                printk("Cannot create ptree dir\n");
                return -1;
        }

        inputdir = debugfs_create_file("input", , , , );
        ptreedir = debugfs_create_("ptree", , , ); // Find suitable debugfs API
#endif

	// [jh]
	dir = debugfs_create_dir("ptree", NULL);
	if (!dir) {
        	printk("Cannot create dir /sys/kernel/debug/ptree\n");
        	return -1;
    	}

    	inputdir = debugfs_create_file("input",
        	                           0222,     // write only
                	                   dir,
                        	           NULL,
                                	   &dbfs_fops_input);

    	ptreedir = debugfs_create_file("ptree",
        	                           0444,     // read only
                	                   dir,
                        	           NULL,
                                	   &dbfs_fops_ptree);

	
	printk("dbfs_ptree module initialize done\n");

        return 0;
}



// [jh] remove module  
static void __exit dbfs_module_exit(void)
{
        // Implement exit module code
	
	debugfs_remove_recursive(dir); // [jh]

	printk("dbfs_ptree module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
