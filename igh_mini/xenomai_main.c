#define _GNU_SOURCE
#include <errno.h>				// 在错误事件中的某些库函数表明了什么发生了错误
#include <signal.h>				// 信号处理头文件
#include <stdio.h>				// 标准输入输出库		
#include <string.h>				// 字符操作库 
#include <sys/resource.h>		// 
#include <sys/time.h>			// 时间
#include <sys/types.h>			// 基本系统数据类型
#include <unistd.h>				// 提供对系统API访问
#include <time.h> /* clock_gettime() */
#include <sys/mman.h> /* mlockall() */
#include <sched.h> /* sched_setscheduler() */	
#include <syslog.h>				// 执行系统日志记录活动。
#include <pthread.h>			// 创建线程
#include <sched.h>				// 任务调度
#include <sys/timerfd.h>		// 定时器接口
#include <stdbool.h>			// 
#include <stdlib.h>				// 标准库
#include <sys/param.h>			
#include <getopt.h>				// 减轻命令行处理负担
#include "list.h"
#include "timer.h"
/****************************************************************************/
#include "ecrt.h"
#include "cfg.h"
#include "devices/local.h"
#include "devices/y7.h"
#include "devices/x3e.h"
/****************************************************************************/

/** Task period in ns. */
#define NSEC_PER_SEC (1000000000)
#define TIMESPEC2NS(T) ((uint64_t) (T).tv_sec * NSEC_PER_SEC + (T).tv_nsec)
#define MAX_SAFE_STACK (8 * 1024) /* 保证安全访问而不发生故障的最大堆栈大小The maximum stack size which is guranteed safe to access without faulting */

enum {
    DRIVE_TYPE_HOME = 0x06,
    DRIVE_TYPE_CSP = 0x08,
    DRIVE_TYPE_CSV = 0x09,
    DRIVE_TYPE_CST = 0x0A,
};

static LIST_HEAD(timer_list_head);
static void cyclic_task(igh_proc_data * igh_data,unsigned int index);


//  从站初始化  PDO项的偏移量 offsets for PDO entries  
static void init_slaves(struct igh_global_cfg *cfg, struct slave_servo_t *slaves, const unsigned int count)
{
	if (NULL == slaves || NULL == cfg)
		return ;

	uint32_t vendor_id, product_code;

	// Init slave0, the local LAN9252
	slaves[0].info.alias = 0;
	slaves[0].info.position = 0;
	slaves[0].info.vendor_id = VENDOR_ID_LOCAL; 
	slaves[0].info.product_code = PRODUCT_CODE_LOCAL; 

	/* 只支持相同的设备 Just support the same device */
	switch (cfg->type) {
	case SLAVE_TYPE_X3E:
		vendor_id = VENDOR_ID_X3E;
		product_code = PRODUCT_CODE_X3E;
		break;
	case SLAVE_TYPE_Y7:
		vendor_id = VENDOR_ID_Y7;
		product_code = PRODUCT_CODE_Y7;
		break;
	default:
		vendor_id = VENDOR_ID_X3E;
		product_code = PRODUCT_CODE_X3E;
		break;
	}

	// Init the X3E slaves
	// Only support X3e
	for (int i = 1; i < count; ++i) {
		slaves[i].info.alias = 0;
		slaves[i].info.position = i;
		slaves[i].info.vendor_id = vendor_id;
		slaves[i].info.product_code = product_code; 	
	}

	for (int i = 0; i < count; ++i)
		fprintf(stdout, "slave%d: %d, %d, 0x%08x, 0x%08x\n", i, slaves[i].info.alias, 
				slaves[i].info.position,
			       	slaves[i].info.vendor_id, 
				slaves[i].info.product_code);
}

// Init pdo entrys  初始化pdo条目
//void init_domain_regs(ec_pdo_entry_reg_t *domain_regs, const unsigned int regs_count, struct slave_servo_t *slaves_servo, const unsigned int count)
void init_domain_regs(struct igh_global_cfg *cfg, igh_proc_data *igh_data)
{
	if (NULL == igh_data->domain_regs || NULL == igh_data->slave_servos)
		return ;

	uint16_t index, errcode;
	unsigned int *offset;
	uint32_t vendor_id, product_code;

	fprintf(stdout, "regs count: %d, slave count: %d\n", igh_data->slave_domain_regs_count, igh_data->x3e_slave_count);

	/* Just support the same device */
	switch (cfg->type) {
	case SLAVE_TYPE_X3E:
		vendor_id = VENDOR_ID_X3E;
		product_code = PRODUCT_CODE_X3E;
		errcode = 0x213f;
		break;
	case SLAVE_TYPE_Y7:
		vendor_id = VENDOR_ID_Y7;
		product_code = PRODUCT_CODE_Y7;
		errcode = 0x60f4;
		break;
	default:
		vendor_id = VENDOR_ID_X3E;
		product_code = PRODUCT_CODE_X3E;
		errcode = 0x213f;
		break;
	}

	for (int i = 0; i < igh_data->x3e_slave_count; ++i) {
		for (int j = 0; j < 8; ++j) {
			igh_data->domain_regs[i * 8 + j].alias = 0;
			igh_data->domain_regs[i * 8 + j].position = i + 1;
			igh_data->domain_regs[i * 8 + j].vendor_id = vendor_id;
			igh_data->domain_regs[i * 8 + j].product_code = product_code;
			igh_data->domain_regs[i * 8 + j].subindex = 0;
			switch (j) {
			case 0: index = 0x6040; offset = &igh_data->slave_servos[i + 1].ctrlWord; break;
			case 1: index = 0x6060; offset = &igh_data->slave_servos[i + 1].set_servo_mode; break;
			//case 2: index = 0x607a; offset = &igh_data->slave_servos[i + 1].set_position; break;
			case 2: index = 0x60ff; offset = &igh_data->slave_servos[i + 1].set_velocity; break;
			case 3: index = 0x603f; offset = &igh_data->slave_servos[i + 1].error_code; break;
			case 4: index = 0x6041; offset = &igh_data->slave_servos[i + 1].statusWord; break;
			case 5: index = 0x6061; offset = &igh_data->slave_servos[i + 1].cur_servo_mode; break;
			case 6: index = 0x6064; offset = &igh_data->slave_servos[i + 1].cur_position; break;
			case 7: index = errcode; offset = &igh_data->slave_servos[i + 1].servo_error; break;
			}
			igh_data->domain_regs[i * 8 + j].index = index;
			igh_data->domain_regs[i * 8 + j].offset = offset;
		}
	}
	// 最后一个必须为NULL The last one must be NULL
	igh_data->domain_regs[igh_data->slave_domain_regs_count - 1].alias = 0;
	igh_data->domain_regs[igh_data->slave_domain_regs_count - 1].position = 0;
	igh_data->domain_regs[igh_data->slave_domain_regs_count - 1].vendor_id = 0;
	igh_data->domain_regs[igh_data->slave_domain_regs_count - 1].product_code = 0;
	igh_data->domain_regs[igh_data->slave_domain_regs_count - 1].subindex = 0;
	igh_data->domain_regs[igh_data->slave_domain_regs_count - 1].index = 0;
	igh_data->domain_regs[igh_data->slave_domain_regs_count - 1].offset = 0;

	for (int i = 0; i < igh_data->slave_domain_regs_count; ++i) {
		fprintf(stdout, "Slave%d: %d, %d, 0x%08x, 0x%08x, %d, 0x%04x, %p\n", (i / 8) + 1,
				igh_data->domain_regs[i].alias, igh_data->domain_regs[i].position,
				igh_data->domain_regs[i].vendor_id, igh_data->domain_regs[i].product_code,
				igh_data->domain_regs[i].subindex, igh_data->domain_regs[i].index,
				igh_data->domain_regs[i].offset);

		if (i != 0 && 7 == (i % 8))
			fprintf(stdout, "\n");
	}
}

static __attribute__((unused)) void check_domain_state(ec_domain_t *domain)
{
	ec_domain_state_t ds;
	static ec_domain_state_t domain_state;
	ecrt_domain_state(domain, &ds);

	if (ds.working_counter != domain_state.working_counter)
		fprintf(stdout, "Domain: WC %u.\n", ds.working_counter);

	if (ds.wc_state != domain_state.wc_state)
		fprintf(stdout, "Domain: State %u.\n", ds.wc_state);


	domain_state = ds;
}

/*****************************************************************************/

static __attribute__((unused)) void check_master_state(ec_master_t *master)
{
	ec_master_state_t ms;
	static ec_master_state_t master_state = {};

	ecrt_master_state(master, &ms);		// 读取当前主站状态。

	if (ms.slaves_responding != master_state.slaves_responding)
		fprintf(stdout, "%u slave(s).\n", ms.slaves_responding);

	if (ms.al_states != master_state.al_states)
		fprintf(stdout, "AL states: 0x%02X.\n", ms.al_states);

	if (ms.link_up != master_state.link_up)
		fprintf(stdout, "Link is %s.\n", ms.link_up ? "up" : "down");

	master_state = ms;
}

/*****************************************************************************/

static __attribute__((unused)) void check_slave_config_states(struct slave_servo_t *slave_servos, unsigned int index)
{
	ec_slave_config_state_t s;
	ecrt_slave_config_state(slave_servos[index].info.sc, &s);
	if (s.al_state != slave_servos[index].info.state.al_state)
		fprintf(stdout, "slave%d: State 0x%02X.\n", index, s.al_state);
		
	if (s.online != slave_servos[index].info.state.online)
		fprintf(stdout, "slave%d: %s.\n", index, s.online ? "online" : "offline");
		
	if (s.operational != slave_servos[index].info.state.operational)
		fprintf(stdout, "slave%d: %soperational.\n", index, s.operational ? "" : "Not ");

	slave_servos[index].info.state = s;
}
/*****************************************************************************/



static void fsm_state_control_motor(void *data)		//电机控制状态机
{
//	struct timespec time;
//	long start_time, end_time;
//	static long t = 0, c = 0;
	
	//clock_gettime(CLOCK_MONOTONIC, &time);
	//start_time = time.tv_sec * 1000000000 + time.tv_nsec;
	// receive process data

	igh_proc_data *igh_data = data;

	ecrt_domain_process(igh_data->domain);	//计算数据报工作计数器
	for(int i = 1; i <= igh_data->x3e_slave_count; ++i)
		cyclic_task((igh_proc_data *)data, i);
	ecrt_domain_queue(igh_data->domain);	// 调用这个函数来标记域的数据报，以便在下一次调用ecrt_master_send()时进行交换。
#if 0
	clock_gettime(CLOCK_MONOTONIC, &time);

	end_time = time.tv_sec * 1000000000 + time.tv_nsec;
	c++;
	t += (end_time - start_time);
	if (c >= 30 * (NSEC_PER_SEC / fsm->motor_cycle_time)) {
		fprintf(stdout, "deal time: %ldns\n", t / c);
		t = c = 0;
	}
#endif
	ecrt_master_sync_reference_clock(igh_data->master);	// 将DC参考时钟漂移补偿数据报排队发送。
	ecrt_master_sync_slave_clocks(igh_data->master);	// 将DC时钟漂移补偿数据报排队发送
	// send process data
	ecrt_master_send(igh_data->master);		// 发送队列中的所有数据报。
}

static void fsm_state_dc_sync(void *data)		//dc同步状态机
{
	igh_proc_data *igh_data = data;
	ecrt_master_sync_reference_clock(igh_data->master);		// DC参考时钟漂移补偿数据报排队发送。
	ecrt_master_sync_slave_clocks(igh_data->master);		// 将DC时钟漂移补偿数据报排队发送
	// send process data
	ecrt_master_send(igh_data->master);			// 发送队列中的所有数据报。
}

/*****************************************************************************/
static int set_cpu_affinity(unsigned int cpu)	//配置核号
{
	int cpus = 0;
	cpu_set_t mask;		// 创建CPU核位图
	cpus = sysconf(_SC_NPROCESSORS_CONF);	// 可配置的处理器个数
	if(cpu > cpus){
		fprintf(stderr, "set cpu affinity error\n");
		return -1;
	}
	CPU_ZERO(&mask);		//将位图置空
	CPU_SET(cpu, &mask);
	/* Set the CPU affinity for a pid */
    	if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) == -1){
		perror("sched_setaffinity");
		return -1;
   	}
	return 0;
}
/*****************************************************************************/
/* 参数解析选项 parameter parse options */
static const struct option long_options[] = {
	{"version", no_argument, NULL, 'v'},
	{"help", no_argument, NULL, 'h'},
	{"debug", no_argument, NULL, 'd'},
	{"dc", required_argument, NULL, 'c'},
	{"motor", required_argument, NULL, 'm'},
	{"slave_num", required_argument, NULL, 'n'},
	{"type", required_argument, NULL, 't'},
	{NULL, 0, NULL, 0}
};

/**
 * @brief printf use help message
 *	输出使用信息
 * @param argv : process name
 */
static void usage(char *argv)
{
	printf("USAGE: %s [options] <dc cycle> <motor cycle>\n", argv);
	printf("igh-ethercat demo\n");
	printf("\n");
	printf("eg:%s -c 1000000 -m 4000000 -n 2 -t x3e/y7\n", argv);
	printf("eg:%s -dc_cycle 1000000(unused) -motor_cycle 4000000 -slave_num 2 -type x3e/y7\n", argv);
	printf("default use cycle nanosecond and must is 125 multiple slave number >= 1\n");
	printf("\n");
	printf("Other Options:\n");
	printf(" -h, --help           display this help\n");
	printf(" -v, --version        display version\n");
	printf(" -d, --debug          printf debug message\n");
	printf(" -t, --type           slave device type\n");
	printf("                      Current support x3e / y7\n");
	printf("\n");
}

#define VERSION ("2.0.0")

/**
 * @brief printf version message
 *输出简要版本信息
 * @param argv : preocess
 */
static void version(char *argv)
{
	printf("%s (compiled %s)\n", argv, __DATE__);
	printf("VERSION:%s\n", VERSION);
}
/*****************************************************************************/
void cyclic_task(igh_proc_data * igh_data, unsigned int index)
{
	unsigned int status_world = 0, ctrl_world = 0;
	
	status_world = EC_READ_U8(igh_data->domain_pd + igh_data->slave_servos[index].statusWord);
	ctrl_world = EC_READ_U16(igh_data->domain_pd + igh_data->slave_servos[index].ctrlWord);

#define STATUSWORLD_SWITCH_ON_DISABLE 		(0x0040)		/**< 状态字禁用开关 */
#define STATUSWORLD_SWITCH_ON_DISABLE_MASK 	(0x004f)		//状态字禁用码
#define STATUSWORLD_READY_SWITCH_ON		(0x0021)			// 0010 0001
#define STATUSWORLD_READY_SWITCH_ON_MASK 	(0x006f)
#define STATUSWORLD_SWITCH_ON			(0x0023)
#define STATUSWORLD_SWITCH_ON_MASK 		(0x006f)
#define STATUSWORLD_ENABLE_OPERATION		(0x0027)
#define STATUSWORLD_ENABLE_OPERATION_MASK 	(0x006f)
#define STATUSWORLD_FAULT			(0x0008)
#define STATUSWORLD_FAULT_MASK 			(0x004f)
#define CTRLWORLD_SHUTDOWM 			(0x0006)
#define CTRLWORLD_SWITCH_ON 			(0x0007)
#define CTRLWORLD_ENABLE_OPERATION 		(0x000f)
#define CTRLWORLD_FAULT		 		(0x0080)

	if (STATUSWORLD_ENABLE_OPERATION == (status_world & STATUSWORLD_ENABLE_OPERATION_MASK))		// 已使能
	{
		// EC_WRITE_U32(domain_pd + slave_servos[index].set_position, EC_READ_U32(domain_pd + slave_servos[index].cur_position) + 0x10000);
	} 
	else {
		// set the X3E in the CSP mode 
		EC_WRITE_U8(igh_data->domain_pd + igh_data->slave_servos[index].set_servo_mode, DRIVE_TYPE_CSV); //更改控制模式
		
		if (STATUSWORLD_SWITCH_ON_DISABLE == (status_world & STATUSWORLD_SWITCH_ON_DISABLE_MASK))	//不可以使能伺服
			
			if (CTRLWORLD_SHUTDOWM != ctrl_world) {
				EC_WRITE_U16(igh_data->domain_pd + igh_data->slave_servos[index].ctrlWord, CTRLWORLD_SHUTDOWM);	//设置使能控制字6
			//	EC_WRITE_U32(igh_data->domain_pd + igh_data->slave_servos[index].set_position, EC_READ_U32(igh_data->domain_pd + igh_data->slave_servos[index].cur_position));
			}

		if (STATUSWORLD_READY_SWITCH_ON == (status_world & STATUSWORLD_READY_SWITCH_ON_MASK))	// 
			// set to switch on
			if (CTRLWORLD_SWITCH_ON != ctrl_world) 
				EC_WRITE_U16(igh_data->domain_pd + igh_data->slave_servos[index].ctrlWord, CTRLWORLD_SWITCH_ON);	// 设置使能控制字7

		if (STATUSWORLD_SWITCH_ON == (status_world & STATUSWORLD_SWITCH_ON_MASK))
			// set to enable operation
			if (CTRLWORLD_ENABLE_OPERATION != ctrl_world)
				EC_WRITE_U16(igh_data->domain_pd + igh_data->slave_servos[index].ctrlWord, CTRLWORLD_ENABLE_OPERATION);	//设置使能控制字15

		if (STATUSWORLD_FAULT == (status_world & STATUSWORLD_FAULT_MASK))			//如果伺服故障
			EC_WRITE_U16(igh_data->domain_pd + igh_data->slave_servos[index].ctrlWord, CTRLWORLD_FAULT);		//设置控制字0x80，故障复位
	}

	// Check all slaves, if all in enable operation, start... 检查所有从站，如果都在启用操作，启动…
	unsigned int status, i = 0;
	for (i = 1; i <= igh_data->x3e_slave_count; ++i) {
		 status = EC_READ_U8(igh_data->domain_pd + igh_data->slave_servos[i].statusWord);
		 if (STATUSWORLD_ENABLE_OPERATION != (status & STATUSWORLD_ENABLE_OPERATION_MASK))	//检查是否使能
			 break;
	}

	if (i >= (igh_data->x3e_slave_count + 1)) {								//如果使能
		for (i = 1; i <= igh_data->x3e_slave_count; ++i)
		//	EC_WRITE_U32(igh_data->domain_pd + igh_data->slave_servos[i].set_position,
		//			EC_READ_U32(igh_data->domain_pd + igh_data->slave_servos[i].cur_position) + 0x10000);  //给定目标位置
			EC_WRITE_U32(igh_data->domain_pd + igh_data->slave_servos[i].set_velocity, 0x10000);  //给定目标位置
	}
}

/****************************************************************************/

/** 读取命令行输入并输出错误信息 */
static int parse_cmdline(int argc, char ** argv, struct igh_global_cfg *cfg)
{
	const char *opt_string ="n:m:c:t:hvd";
	int c;

	/* Default device type is x3e */
	cfg->type = SLAVE_TYPE_X3E;

	while ((c = getopt_long(argc, argv, opt_string, long_options, NULL)) != EOF) {
		if (optarg && *optarg == '-' && c != 'c' && c != 'h' && \
			c != 'v' && c != 'd' && c != 'm' && c != 'n' && c != 't')
			c = '?';

		switch (c) {
		case 'c':
			cfg->dc_cycle = strtoul(optarg, NULL, 10);
			if((cfg->dc_cycle % 125000) || (cfg->dc_cycle < 125000)){
				fprintf(stderr, "dc cycle must is 125 multiple, and >= 125000 please check!\n");
				usage(argv[0]);
				return -1;
			}
			break;

		case 'm':
			cfg->motor_cycle = strtoul(optarg, NULL, 10);
			if((cfg->motor_cycle % 125000) || (cfg->motor_cycle < 125000)){
				fprintf(stderr, "motor cycle must is 125 multiple, and >= 125000 please check!\n");
				usage(argv[0]);
				return -1;
			}
			break;

		case 'n':
			cfg->system_slave_count = strtoul(optarg, NULL, 10);
			break;

		case 'd':
			cfg->debug = true;
			break;

		case 'v':
			version(argv[0]);
			return 0;

		case 'h':
			usage(argv[0]);
			return 0;

		case 't': {
			if (0 == strcmp(optarg, "x3e"))
				cfg->type = SLAVE_TYPE_X3E;
			else if (0 == strcmp(optarg, "y7"))
				cfg->type = SLAVE_TYPE_Y7;
			else {
				usage(argv[0]);
				return -1;
			}
		} break;

		case '?':
			usage(argv[0]);
			return -1;
			break;
		}
	}
	return 0;
}

/**等待主站就绪*/
static int wait_master_ready(igh_proc_data *igh_data)
{
	ec_master_info_t master_info;

	igh_data->master = ecrt_request_master(0);  // 请求一个EtherCAT主站进行实时操作。
	if (!igh_data->master) {
		fprintf(stderr, "request master failed\n");	 // 如果未就绪，输出返回信息
		return -1;
	}

	// Get the master infomation 获取主站信息
	if (ecrt_master(igh_data->master, &master_info) < 0) {		//如果获取主站信息失败
		fprintf(stderr, "Get master information failed\n");
		return -1;
	}

	// The master scan busy, waittiong...
	while (master_info.scan_busy) {			//主站忙，等待
		sleep(1);
	}

	return 0;
}
/**等待从站就绪*/
static void * wait_slaves_ready(struct igh_global_cfg *cfg, uint32_t system_slave_count, igh_proc_data *igh_data)
{
	struct slave_servo_t *pslaves;
	ec_sync_info_t *sync_info;

	if (NULL == cfg || NULL == igh_data)
		return NULL;

	pslaves= (struct slave_servo_t *)malloc(sizeof(struct slave_servo_t) * system_slave_count);	//分配所需内存
	if (NULL == pslaves) {
		fprintf(stderr, "Failed to create slave servos: %s\n", strerror(errno));
		goto err_exit;
	}

	init_slaves(cfg, pslaves, system_slave_count);		// 初始化从站

	for (int i = 0; i < system_slave_count; ++i)
		if (!(pslaves[i].info.sc = ecrt_master_slave_config(igh_data->master, pslaves[i].info.alias, pslaves[i].info.position,		// 获取从站配置。
						pslaves[i].info.vendor_id, pslaves[i].info.product_code))) {
			fprintf(stderr, "Failed to get slave configuration.\n");
			goto err;
		}	
	
	switch (cfg->type) {
	case SLAVE_TYPE_X3E:
		sync_info = slave_x3e_syncs;
		break;
	case SLAVE_TYPE_Y7:
		sync_info = slave_y7_syncs;
		break;
	default:
		sync_info = slave_x3e_syncs;
		break;
	}

	// Slave0 is LAN9252 local
	for (int i = 1; i <= igh_data->x3e_slave_count; ++i)
	{
		if (ecrt_slave_config_pdos(pslaves[i].info.sc, EC_END, sync_info)) {		//配置从站pdo
			fprintf(stderr, "Failed to configure PDOs.\n");
			goto err;
		}
	}
	return pslaves;

err:
	free(pslaves);		//清空结构体内存
err_exit:
	return NULL;
}


static int wait_master_domain_ready(struct igh_global_cfg *cfg, igh_proc_data *igh_data)
{
	if (NULL == cfg || NULL == igh_data)
		return -1;

	igh_data->domain = ecrt_master_create_domain(igh_data->master);	// 创建一个新的流程数据域
	if (!igh_data->domain) {
		fprintf(stderr, "create domain failed\n");
		return -1;
	}

	// The slave0 not use this
	// But the last one must be NULL
	igh_data->domain_regs = (ec_pdo_entry_reg_t *)malloc(sizeof(ec_pdo_entry_reg_t) * igh_data->slave_domain_regs_count);
	if (NULL == igh_data->domain_regs) {
		fprintf(stderr, "Failed to create domain regs: %s\n", strerror(errno));
		return -1;
	}

//	init_domain_regs(igh_data->domain_regs, igh_data->slave_domain_regs_count, igh_data->slave_servos, igh_data->x3e_slave_count);
	init_domain_regs(cfg, igh_data);

	if (ecrt_domain_reg_pdo_entry_list(igh_data->domain, igh_data->domain_regs)) {		// 为一个域注册PDO条目。
		fprintf(stderr, "PDO entry registration failed!\n");
		return -1;
	}

	return 0;
}

static int wait_dc_clk_ready(struct igh_global_cfg cfg, igh_proc_data *igh_data)
{
		// configure SYNC signals for this slave  为这个从站配置同步信号
	printf("Config DC enable!\n");
	for (int i = 0; i < cfg.system_slave_count; ++i)
		ecrt_slave_config_dc(igh_data->slave_servos[i].info.sc, 0x0300, cfg.motor_cycle, 0, 0, 0); /* 1ms */	//配置分布式时钟。

	if (ecrt_master_select_reference_clock(igh_data->master, igh_data->slave_servos[0].info.sc)) {		// 为分布式时钟选择参考时钟。
		fprintf(stderr, "Select reference clock failed\n");
		return 1;
	}

	return 0;
}

static int config_process()
{
	/* Set priority设置优先级 */
	struct sched_param param = {};
	param.sched_priority = 80;

	printf("Using priority %i.", param.sched_priority);
	if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {		// 改变进程的调度策略及进程的实时优先级。
		perror("sched_setscheduler failed");
		return -1;
	}

	if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {		// 内存上锁。防止出现内存交换
		fprintf(stderr, "Warning: Failed to lock memory: %s\n",
				strerror(errno));
		return -1;
	}

	return 0;
}

static void wait_sdo_ready(igh_proc_data* igh_data , ec_sdo_request_t * sdo_request[])
{
	ec_sdo_request_t* requet;
	for (int i = 1; i <= igh_data->x3e_slave_count; ++i)
	{
		if (sdo_request[i-1] = ecrt_slave_config_create_sdo_request(igh_data->slave_servos[i].info.sc, 0x2115, 0x34, 2) == NULL ) {		//配置SDO
			fprintf(stderr, "Failed to configure SDOs %i\n",i);
			return NULL;
		}
		requet = ecrt_slave_config_create_sdo_request(igh_data->slave_servos[i].info.sc, 0x2115, 0x34, 2);
		sdo_request = requet;


		ecrt_sdo_request_timeout(sdo_request[i - 1], 500); // ms
	}
	return NULL;
}

static void read_sdo(ec_sdo_request_t* sdo, int * i)	// 读sdo
{
	switch (ecrt_sdo_request_state(sdo)) {
	case EC_REQUEST_UNUSED: // request was not used yet
		ecrt_sdo_request_read(sdo); // trigger first read
		break;
	case EC_REQUEST_BUSY:
		syslog(LOG_INFO, "Slaves %d ,Still busy...\n",*i);
		break;
	case EC_REQUEST_SUCCESS:
		syslog(LOG_INFO, "Slaves %d SDO value: 0x%04X\n", *i,
			EC_READ_U16(ecrt_sdo_request_data(sdo)));
		ecrt_sdo_request_read(sdo); // trigger next read
		-- *i;
		break;
	case EC_REQUEST_ERROR:
		syslog(LOG_INFO, "Slaves%d Failed to read SDO!\n", *i);
		ecrt_sdo_request_read(sdo); // retry reading
		break;
	}
	return NULL;
}

static void write_sdo(ec_sdo_request_t* sdo, int data, int* i)	// 写sdo
{
	switch (ecrt_sdo_request_state(sdo)) {
	case EC_REQUEST_UNUSED: // request was not used yet
		EC_WRITE_U32(ecrt_sdo_request_data(sdo), data);
		ecrt_sdo_request_write(sdo);
		break;
	case EC_REQUEST_BUSY:
		syslog(LOG_INFO, "Slaves %d ,SDO write busy...\n", *i);
		break;
	case EC_REQUEST_SUCCESS:
		syslog(LOG_INFO, "Slaves %d SDO write success\n", *i);
		ecrt_sdo_request_write(sdo);
		++* i;
		break;
	case EC_REQUEST_ERROR:
		syslog(LOG_INFO, "Slaves %d Failed to write SDO!\n", *i);
		ecrt_sdo_request_write(sdo);
		break;
	}
	return NULL;
}

struct timespec timespec_add(struct timespec time1, struct timespec time2)
{
    struct timespec result;

    if ((time1.tv_nsec + time2.tv_nsec) >= NSEC_PER_SEC) {
        result.tv_sec = time1.tv_sec + time2.tv_sec + 1;
        result.tv_nsec = time1.tv_nsec + time2.tv_nsec - NSEC_PER_SEC;
    } else {
        result.tv_sec = time1.tv_sec + time2.tv_sec;
        result.tv_nsec = time1.tv_nsec + time2.tv_nsec;
    }

    return result;
}

int main(int argc, char **argv)
{
	int ret;
	int slaveconut = 1;
	struct igh_global_cfg igh_cfg;
	struct timer_entry *timer, timer_dc, timer_motor;
	igh_proc_data igh_proc = {};
	struct timespec wakeupTime, time, cycle;


	memset(&igh_cfg, 0, sizeof(struct igh_global_cfg));
	if (parse_cmdline(argc, argv, &igh_cfg))		//读取命令行输入
		return -1;

	if (!igh_cfg.dc_cycle || !igh_cfg.motor_cycle || !igh_cfg.system_slave_count) {
		usage(argv[0]);		//输出帮助
		return -1;
	}

	igh_proc.x3e_slave_count = igh_cfg.system_slave_count - 1;
	igh_proc.slave_domain_regs_count = igh_proc.x3e_slave_count * 8 + 1;

	if (set_cpu_affinity(1)) {					// 分配cpu
		printf("set cpu affinity error\n");
		return -1;
	}

	if (wait_master_ready(&igh_proc) ) {		// 等待主站就绪
		return -1;
	}

	if ( (igh_proc.slave_servos = wait_slaves_ready(&igh_cfg, igh_cfg.system_slave_count, &igh_proc) ) == NULL) {	//等待从站就绪
		ret = -1;
		goto err_slave_servos;
	}

	if ( (ret = wait_master_domain_ready(&igh_cfg, &igh_proc) ) != 0) {			//等待域就绪
		goto err_slave_servos;
	}

	wait_dc_clk_ready(igh_cfg, &igh_proc);			//等待dc时钟同步

	// wait_sdo_ready(&igh_proc, sdo_request);
	

	for (int i = 1; i <= igh_proc.x3e_slave_count; ++i)
	{
		igh_proc.slave_servos[i].sdo_request = ecrt_slave_config_create_sdo_request(igh_proc.slave_servos[i].info.sc, 0x2115, 0x34, 2);
		ecrt_sdo_request_timeout(igh_proc.slave_servos[i].sdo_request, 500); // ms
	}


	printf("Activating master...\n");
	if ( (ret = ecrt_master_activate(igh_proc.master) ) != 0) {		// 告诉主站配置阶段已经完成，实时操作将开始。
		fprintf(stderr, "Master activate failed!\n");
		goto err_domain_regs;
	}

	igh_proc.domain_pd = ecrt_domain_data(igh_proc.domain);			// 返回域的进程数据
	if (NULL == igh_proc.domain_pd) {
       		fprintf(stderr, "Get domain data failed!\n");
       		ret = -1;
       		goto err_domain_regs;
	}

	config_process();			//设置优先级

	printf("Starting RT task with dc_cycle=%lld(unused) motor_cycle=%lld ns.\n", igh_cfg.dc_cycle, igh_cfg.motor_cycle);

	static unsigned int sync_ref_counter = 0;
   	// get current time
	clock_gettime(CLOCK_REALTIME, &wakeupTime);			//获取系统时间
	cycle.tv_sec = 0;
	cycle.tv_nsec = igh_cfg.motor_cycle;				// 电机周期
	while(1) {
	    wakeupTime = timespec_add(wakeupTime, cycle);	//系统时间+电机周期
	    clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &wakeupTime, NULL);		// 睡眠

	    // Write application time to master
	    // 把申请时间写主站
	    // It is a good idea to use the target time (not the measured time) as
	    // application time, because it is more stable.
	    // 使用目标时间(而不是测量时间)作为应用时间是一个好主意，因为它更稳定。
	    ecrt_master_application_time(igh_proc.master, TIMESPEC2NS(wakeupTime));	//设置应用时间
		
		//读SDO
		read_sdo(igh_proc.slave_servos[slaveconut].sdo_request,&slaveconut);
		//read_sdo(igh_proc.slave_servos[slaveconut].sdo_request2, &slaveconut);
		//write_sdo(igh_proc.slave_servos[slaveconut].sdo_request, 0x08, &slaveconut);
		(slaveconut <= 1) ? slaveconut = igh_proc.x3e_slave_count : slaveconut;

	    // receive process data
	    ecrt_master_receive(igh_proc.master);		// 从硬件中获取接收到的帧并处理数据报。
	    ecrt_domain_process(igh_proc.domain);		// 确定域的数据报状态

	    for(int i = 1; i <= igh_proc.x3e_slave_count; ++i)
			cyclic_task(&igh_proc, i);				// 运动相关

	    if (sync_ref_counter) {
	        sync_ref_counter--;
	    } else {
	        sync_ref_counter = 1; // sync every cycle 同步每个周期

	        clock_gettime(CLOCK_REALTIME, &time);		//获取系统时间
	        ecrt_master_sync_reference_clock_to(igh_proc.master, TIMESPEC2NS(time));
	    }
	    ecrt_master_sync_slave_clocks(igh_proc.master);		// 将DC时钟漂移补偿数据报排队发送。所有从站同步到参考时钟。

	    // send process data
	    ecrt_domain_queue(igh_proc.domain);			// 在主站的数据报队列中排队所有的域数据报。
	    ecrt_master_send(igh_proc.master);			// 发送队列中的所有数据报。
	}

err_domain_regs:
	free(igh_proc.domain_regs);		//释放内存
err_slave_servos:
	free(igh_proc.slave_servos);	//释放内存
	return ret;
}


/****************************************************************************/

