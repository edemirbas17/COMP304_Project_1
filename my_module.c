#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched/task.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eray Sozer, Eren Berke Demirbas");
MODULE_DESCRIPTION("Pstraverse");

static int my_pid = 1;
static int my_mode = 1;

module_param(my_pid,int,0);
module_param(my_mode,int,0);

void dfs(struct task_struct *task){

	struct task_struct *next_task;
	struct list_head *list;

	list_for_each(list, &task->children){

		next_task = list_entry(list,struct task_struct, sibling);
		printk(KERN_INFO "pid: %d\tpname: %s \n",next_task->pid, next_task->comm);
		dfs(next_task);	

	}

}

void bfs(struct task_struct *task){
       
       	struct task_struct *next_task;
        struct list_head *list;

        list_for_each(list, &task->children){

                next_task = list_entry(list,struct task_struct, sibling);
                printk(KERN_INFO "pid: %d\tpname: %s \n",next_task->pid, next_task->comm);
                // dfs(next_task);

   	}

        list_for_each(list, &task->children){

                next_task = list_entry(list,struct task_struct, sibling);
                // printk(KERN_INFO "pid: %d\tpname: %s \n",next_task->pid, next_task->comm);
                bfs(next_task);

        }




}
int our_module_init(void){
	printk("Our module is initialized!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! \n");
	struct pid *pid_struct = find_get_pid(my_pid);
	struct task_struct *task = pid_task(pid_struct,PIDTYPE_PID);
	
	printk("My pid: %d, my mode: %d \n",my_pid,my_mode);
	if(my_mode == 0){
		printk("Dfs:------------------------------------- \n");
		dfs(task);
	}
	else if(my_mode == 1){
		printk("Bfs:------------------------------------- \n");
		bfs(task);
	}
	//printk("My pid: %d, my mode: %d \n",my_pid,my_mode);
	return 0;
}

void our_module_exit(void){
	printk("Exited !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! \n");

}
module_init(our_module_init);
module_exit(our_module_exit);
