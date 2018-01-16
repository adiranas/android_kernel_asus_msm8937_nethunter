#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/platform_device.h>   // <asus-olaf20150715+>

//ASUS BSP+++ jason2_zhang add /proc/apid node
static struct proc_dir_entry *project_id_proc_file;
static int project_id_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", g_ASUS_PRJ_ID);
	return 0;
}

static int project_id_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, project_id_proc_read, NULL);
}


static struct file_operations project_id_proc_ops = {
	.open = project_id_proc_open,
	.read = seq_read,
	.release = single_release,
};

static void create_project_id_proc_file(void)
{
    printk("create_project_id_proc_file\n");
    project_id_proc_file = proc_create("apid", 0444,NULL, &project_id_proc_ops);
    if(project_id_proc_file){
        printk("create project_id_proc_file sucessed!\n");
    }else{
		printk("create project_id_proc_file failed!\n");
    }
}
//ASUS BSP--- jason2_zhang add /proc/apid node

//ASUS BSP+++ jason2_zhang add /proc/apsta node
static struct proc_dir_entry *project_stage_proc_file;
static int project_stage_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", g_ASUS_PRJ_STAGE);
	return 0;
}

static int project_stage_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, project_stage_proc_read, NULL);
}


static struct file_operations project_stage_proc_ops = {
	.open = project_stage_proc_open,
	.read = seq_read,
	.release = single_release,
};

static void create_project_stage_proc_file(void)
{
    printk("create_project_stage_proc_file\n");
    project_stage_proc_file = proc_create("apsta", 0444,NULL, &project_stage_proc_ops);
    if(project_stage_proc_file){
        printk("create project_stage_proc_file sucessed!\n");
    }else{
		printk("create project_stage_proc_file failed!\n");
    }
}
//ASUS BSP--- jason2_zhang add /proc/apsta node


//ASUS BSP+++ rick_zhang add /proc/apstas node to show project satge string
static struct proc_dir_entry *project_stage_string_proc_file;
static int project_stage_string_proc_read(struct seq_file *buf, void *v)
{
	const char* stage_info = "UNKNOWN";
	switch(g_ASUS_PRJ_STAGE){
		case STAGE_EVB:
			stage_info = "EVB";
			break;
		case STAGE_SR1:
			stage_info = "SR1";
			break;
		case STAGE_SR2:
			stage_info = "SR2";
			break;
		case STAGE_ER:
			stage_info = "ER1";
			break;
		case STAGE_ER2:
			if(g_ASUS_PRJ_ID == PRJ_SCORPIO_ST)
				stage_info = "ER3";
			else
				stage_info = "ER2";
			break;
		case STAGE_PR:
			stage_info = "PR1";
			break;
		case STAGE_PR2:
			stage_info = "PR2";
			break;
		case STAGE_MP:
			stage_info = "MP";
			break;
		default:
			stage_info = "Unknown";
			break;
	}
	seq_printf(buf, "%s\n", stage_info);
	return 0;
}

static int project_stage_string_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, project_stage_string_proc_read, NULL);
}


static struct file_operations project_stage_string_proc_ops = {
	.open = project_stage_string_proc_open,
	.read = seq_read,
	.release = single_release,
};

static void create_project_stage_string_proc_file(void)
{
    printk("create_project_stage_string_proc_file\n");
    project_stage_string_proc_file = proc_create("apstas", 0444,NULL, &project_stage_string_proc_ops);
    if(project_stage_string_proc_file){
        printk("create project_stage_string_proc_file sucessed!\n");
    }else{
		printk("create project_stage_string_proc_file failed!\n");
    }
}
//ASUS BSP--- rick_zhang add /proc/apstas node  to show project satge string

//ASUS BSP+++ jason2_zhang add /proc/aprf node
static struct proc_dir_entry *project_RFsku_proc_file;
static int project_RFsku_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", g_ASUS_RF_SKU);
	return 0;
}

static int project_RFsku_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, project_RFsku_proc_read, NULL);
}


static struct file_operations project_RFsku_proc_ops = {
	.open = project_RFsku_proc_open,
	.read = seq_read,
	.release = single_release,
};

static void create_project_RFsku_proc_file(void)
{
    printk("create_project_RFsku_proc_file\n");
    project_RFsku_proc_file = proc_create("aprf", 0444,NULL, &project_RFsku_proc_ops);
    if(project_RFsku_proc_file){
        printk("create project_RFsku_proc_file sucessed!\n");
    }else{
		printk("create project_RFsku_proc_file failed!\n");
    }
}
//ASUS BSP--- jason2_zhang add /proc/aprf node

//ASUS BSP+++ jason2_zhang add /proc/apmem node
static struct proc_dir_entry *project_mem_proc_file;
static int project_mem_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", g_ASUS_MEM_ID);
	return 0;
}

static int project_mem_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, project_mem_proc_read, NULL);
}


static struct file_operations project_mem_proc_ops = {
	.open = project_mem_proc_open,
	.read = seq_read,
	.release = single_release,
};

static void create_project_mem_proc_file(void)
{
    printk("create_project_mem_proc_file\n");
    project_mem_proc_file = proc_create("apmem", 0444,NULL, &project_mem_proc_ops);
    if(project_mem_proc_file){
        printk("create project_mem_proc_file sucessed!\n");
    }else{
		printk("create project_mem_proc_file failed!\n");
    }
}
//ASUS BSP--- jason2_zhang add /proc/apmem node

//ASUS BSP+++ jason2_zhang add /proc/apcpu node
static struct proc_dir_entry *project_cpu_type_proc_file;
static int project_cpu_type_proc_read(struct seq_file *buf, void *v)
{
        seq_printf(buf, "%d\n", g_ASUS_PRJ_CPUTYPE);
        return 0;
}

static int project_cpu_type_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, project_cpu_type_proc_read, NULL);
}


static struct file_operations project_cpu_type_proc_ops = {
        .open = project_cpu_type_proc_open,
        .read = seq_read,
        .release = single_release,
};

static void create_project_cpu_type_proc_file(void)
{
    printk("create_project_cpu_type_proc_file\n");
    project_cpu_type_proc_file = proc_create("apcpu", 0444,NULL, &project_cpu_type_proc_ops);
    if(project_cpu_type_proc_file){
        printk("create project_cpu_type_proc_file sucessed!\n");
    }else{
                printk("create project_cpu_type_proc_file failed!\n");
    }
}
//ASUS BSP+++ jason2_zhang add /proc/apcpu node

static int __init proc_asusPRJ_init(void)
{
	create_project_id_proc_file();
	create_project_RFsku_proc_file();
	create_project_stage_proc_file();
	//ASUS BSP+++ rick_zhang add /proc/apstas node to show project satge string
	create_project_stage_string_proc_file();
	//ASUS BSP--- rick_zhang add /proc/apstas node to show project satge string
	create_project_mem_proc_file();
	create_project_cpu_type_proc_file();
	return 0;
}
module_init(proc_asusPRJ_init);
