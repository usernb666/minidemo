#define _GNU_SOURCE
#include <errno.h>				// �ڴ����¼��е�ĳЩ�⺯��������ʲô�����˴���
#include <signal.h>				// �źŴ���ͷ�ļ�
#include <stdio.h>				// ��׼���������		
#include <string.h>				// �ַ������� 
#include <sys/resource.h>		// 
#include <sys/time.h>			// ʱ��
#include <sys/types.h>			// ����ϵͳ��������
#include <unistd.h>				// �ṩ��ϵͳAPI����
#include <time.h> /* clock_gettime() */
#include <sys/mman.h> /* mlockall() */
#include <sched.h> /* sched_setscheduler() */	
#include <syslog.h>				// ִ��ϵͳ��־��¼���
#include <pthread.h>			// �����߳�
#include <sched.h>				// �������
#include <sys/timerfd.h>		// ��ʱ���ӿ�
#include <stdbool.h>			// 
#include <stdlib.h>				// ��׼��
#include <sys/param.h>			
#include <getopt.h>				// ���������д�����
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
#define MAX_SAFE_STACK (8 * 1024) /* ��֤��ȫ���ʶ����������ϵ�����ջ��СThe maximum stack size which is guranteed safe to access without faulting */

enum {
    DRIVE_TYPE_HOME = 0x06,
    DRIVE_TYPE_CSP = 0x08,
    DRIVE_TYPE_CSV = 0x09,
    DRIVE_TYPE_CST = 0x0A,
};

static LIST_HEAD(timer_list_head);
static void cyclic_task(igh_proc_data * igh_data,unsigned int index);


//  ��վ��ʼ��  PDO���ƫ���� offsets for PDO entries  
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

	/* ֻ֧����ͬ���豸 Just support the same device */
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

// Init pdo entrys  ��ʼ��pdo��Ŀ
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
	// ���һ������ΪNULL The last one must be NULL
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

	ecrt_master_state(master, &ms);		// ��ȡ��ǰ��վ״̬��

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



static void fsm_state_control_motor(void *data)		//�������״̬��
{
//	struct timespec time;
//	long start_time, end_time;
//	static long t = 0, c = 0;
	
	//clock_gettime(CLOCK_MONOTONIC, &time);
	//start_time = time.tv_sec * 1000000000 + time.tv_nsec;
	// receive process data

	igh_proc_data *igh_data = data;

	ecrt_domain_process(igh_data->domain);	//�������ݱ�����������
	for(int i = 1; i <= igh_data->x3e_slave_count; ++i)
		cyclic_task((igh_proc_data *)data, i);
	ecrt_domain_queue(igh_data->domain);	// ����������������������ݱ����Ա�����һ�ε���ecrt_master_send()ʱ���н�����
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
	ecrt_master_sync_reference_clock(igh_data->master);	// ��DC�ο�ʱ��Ư�Ʋ������ݱ��Ŷӷ��͡�
	ecrt_master_sync_slave_clocks(igh_data->master);	// ��DCʱ��Ư�Ʋ������ݱ��Ŷӷ���
	// send process data
	ecrt_master_send(igh_data->master);		// ���Ͷ����е��������ݱ���
}

static void fsm_state_dc_sync(void *data)		//dcͬ��״̬��
{
	igh_proc_data *igh_data = data;
	ecrt_master_sync_reference_clock(igh_data->master);		// DC�ο�ʱ��Ư�Ʋ������ݱ��Ŷӷ��͡�
	ecrt_master_sync_slave_clocks(igh_data->master);		// ��DCʱ��Ư�Ʋ������ݱ��Ŷӷ���
	// send process data
	ecrt_master_send(igh_data->master);			// ���Ͷ����е��������ݱ���
}

/*****************************************************************************/
static int set_cpu_affinity(unsigned int cpu)	//���ú˺�
{
	int cpus = 0;
	cpu_set_t mask;		// ����CPU��λͼ
	cpus = sysconf(_SC_NPROCESSORS_CONF);	// �����õĴ���������
	if(cpu > cpus){
		fprintf(stderr, "set cpu affinity error\n");
		return -1;
	}
	CPU_ZERO(&mask);		//��λͼ�ÿ�
	CPU_SET(cpu, &mask);
	/* Set the CPU affinity for a pid */
    	if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) == -1){
		perror("sched_setaffinity");
		return -1;
   	}
	return 0;
}
/*****************************************************************************/
/* ��������ѡ�� parameter parse options */
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
 *	���ʹ����Ϣ
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
 *�����Ҫ�汾��Ϣ
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

#define STATUSWORLD_SWITCH_ON_DISABLE 		(0x0040)		/**< ״̬�ֽ��ÿ��� */
#define STATUSWORLD_SWITCH_ON_DISABLE_MASK 	(0x004f)		//״̬�ֽ�����
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

	if (STATUSWORLD_ENABLE_OPERATION == (status_world & STATUSWORLD_ENABLE_OPERATION_MASK))		// ��ʹ��
	{
		// EC_WRITE_U32(domain_pd + slave_servos[index].set_position, EC_READ_U32(domain_pd + slave_servos[index].cur_position) + 0x10000);
	} 
	else {
		// set the X3E in the CSP mode 
		EC_WRITE_U8(igh_data->domain_pd + igh_data->slave_servos[index].set_servo_mode, DRIVE_TYPE_CSV); //���Ŀ���ģʽ
		
		if (STATUSWORLD_SWITCH_ON_DISABLE == (status_world & STATUSWORLD_SWITCH_ON_DISABLE_MASK))	//������ʹ���ŷ�
			
			if (CTRLWORLD_SHUTDOWM != ctrl_world) {
				EC_WRITE_U16(igh_data->domain_pd + igh_data->slave_servos[index].ctrlWord, CTRLWORLD_SHUTDOWM);	//����ʹ�ܿ�����6
			//	EC_WRITE_U32(igh_data->domain_pd + igh_data->slave_servos[index].set_position, EC_READ_U32(igh_data->domain_pd + igh_data->slave_servos[index].cur_position));
			}

		if (STATUSWORLD_READY_SWITCH_ON == (status_world & STATUSWORLD_READY_SWITCH_ON_MASK))	// 
			// set to switch on
			if (CTRLWORLD_SWITCH_ON != ctrl_world) 
				EC_WRITE_U16(igh_data->domain_pd + igh_data->slave_servos[index].ctrlWord, CTRLWORLD_SWITCH_ON);	// ����ʹ�ܿ�����7

		if (STATUSWORLD_SWITCH_ON == (status_world & STATUSWORLD_SWITCH_ON_MASK))
			// set to enable operation
			if (CTRLWORLD_ENABLE_OPERATION != ctrl_world)
				EC_WRITE_U16(igh_data->domain_pd + igh_data->slave_servos[index].ctrlWord, CTRLWORLD_ENABLE_OPERATION);	//����ʹ�ܿ�����15

		if (STATUSWORLD_FAULT == (status_world & STATUSWORLD_FAULT_MASK))			//����ŷ�����
			EC_WRITE_U16(igh_data->domain_pd + igh_data->slave_servos[index].ctrlWord, CTRLWORLD_FAULT);		//���ÿ�����0x80�����ϸ�λ
	}

	// Check all slaves, if all in enable operation, start... ������д�վ������������ò�����������
	unsigned int status, i = 0;
	for (i = 1; i <= igh_data->x3e_slave_count; ++i) {
		 status = EC_READ_U8(igh_data->domain_pd + igh_data->slave_servos[i].statusWord);
		 if (STATUSWORLD_ENABLE_OPERATION != (status & STATUSWORLD_ENABLE_OPERATION_MASK))	//����Ƿ�ʹ��
			 break;
	}

	if (i >= (igh_data->x3e_slave_count + 1)) {								//���ʹ��
		for (i = 1; i <= igh_data->x3e_slave_count; ++i)
		//	EC_WRITE_U32(igh_data->domain_pd + igh_data->slave_servos[i].set_position,
		//			EC_READ_U32(igh_data->domain_pd + igh_data->slave_servos[i].cur_position) + 0x10000);  //����Ŀ��λ��
			EC_WRITE_U32(igh_data->domain_pd + igh_data->slave_servos[i].set_velocity, 0x10000);  //����Ŀ��λ��
	}
}

/****************************************************************************/

/** ��ȡ���������벢���������Ϣ */
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

/**�ȴ���վ����*/
static int wait_master_ready(igh_proc_data *igh_data)
{
	ec_master_info_t master_info;

	igh_data->master = ecrt_request_master(0);  // ����һ��EtherCAT��վ����ʵʱ������
	if (!igh_data->master) {
		fprintf(stderr, "request master failed\n");	 // ���δ���������������Ϣ
		return -1;
	}

	// Get the master infomation ��ȡ��վ��Ϣ
	if (ecrt_master(igh_data->master, &master_info) < 0) {		//�����ȡ��վ��Ϣʧ��
		fprintf(stderr, "Get master information failed\n");
		return -1;
	}

	// The master scan busy, waittiong...
	while (master_info.scan_busy) {			//��վæ���ȴ�
		sleep(1);
	}

	return 0;
}
/**�ȴ���վ����*/
static void * wait_slaves_ready(struct igh_global_cfg *cfg, uint32_t system_slave_count, igh_proc_data *igh_data)
{
	struct slave_servo_t *pslaves;
	ec_sync_info_t *sync_info;

	if (NULL == cfg || NULL == igh_data)
		return NULL;

	pslaves= (struct slave_servo_t *)malloc(sizeof(struct slave_servo_t) * system_slave_count);	//���������ڴ�
	if (NULL == pslaves) {
		fprintf(stderr, "Failed to create slave servos: %s\n", strerror(errno));
		goto err_exit;
	}

	init_slaves(cfg, pslaves, system_slave_count);		// ��ʼ����վ

	for (int i = 0; i < system_slave_count; ++i)
		if (!(pslaves[i].info.sc = ecrt_master_slave_config(igh_data->master, pslaves[i].info.alias, pslaves[i].info.position,		// ��ȡ��վ���á�
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
		if (ecrt_slave_config_pdos(pslaves[i].info.sc, EC_END, sync_info)) {		//���ô�վpdo
			fprintf(stderr, "Failed to configure PDOs.\n");
			goto err;
		}
	}
	return pslaves;

err:
	free(pslaves);		//��սṹ���ڴ�
err_exit:
	return NULL;
}


static int wait_master_domain_ready(struct igh_global_cfg *cfg, igh_proc_data *igh_data)
{
	if (NULL == cfg || NULL == igh_data)
		return -1;

	igh_data->domain = ecrt_master_create_domain(igh_data->master);	// ����һ���µ�����������
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

	if (ecrt_domain_reg_pdo_entry_list(igh_data->domain, igh_data->domain_regs)) {		// Ϊһ����ע��PDO��Ŀ��
		fprintf(stderr, "PDO entry registration failed!\n");
		return -1;
	}

	return 0;
}

static int wait_dc_clk_ready(struct igh_global_cfg cfg, igh_proc_data *igh_data)
{
		// configure SYNC signals for this slave  Ϊ�����վ����ͬ���ź�
	printf("Config DC enable!\n");
	for (int i = 0; i < cfg.system_slave_count; ++i)
		ecrt_slave_config_dc(igh_data->slave_servos[i].info.sc, 0x0300, cfg.motor_cycle, 0, 0, 0); /* 1ms */	//���÷ֲ�ʽʱ�ӡ�

	if (ecrt_master_select_reference_clock(igh_data->master, igh_data->slave_servos[0].info.sc)) {		// Ϊ�ֲ�ʽʱ��ѡ��ο�ʱ�ӡ�
		fprintf(stderr, "Select reference clock failed\n");
		return 1;
	}

	return 0;
}

static int config_process()
{
	/* Set priority�������ȼ� */
	struct sched_param param = {};
	param.sched_priority = 80;

	printf("Using priority %i.", param.sched_priority);
	if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {		// �ı���̵ĵ��Ȳ��Լ����̵�ʵʱ���ȼ���
		perror("sched_setscheduler failed");
		return -1;
	}

	if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {		// �ڴ���������ֹ�����ڴ潻��
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
		if (sdo_request[i-1] = ecrt_slave_config_create_sdo_request(igh_data->slave_servos[i].info.sc, 0x2115, 0x34, 2) == NULL ) {		//����SDO
			fprintf(stderr, "Failed to configure SDOs %i\n",i);
			return NULL;
		}
		requet = ecrt_slave_config_create_sdo_request(igh_data->slave_servos[i].info.sc, 0x2115, 0x34, 2);
		sdo_request = requet;


		ecrt_sdo_request_timeout(sdo_request[i - 1], 500); // ms
	}
	return NULL;
}

static void read_sdo(ec_sdo_request_t* sdo, int * i)	// ��sdo
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

static void write_sdo(ec_sdo_request_t* sdo, int data, int* i)	// дsdo
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
	if (parse_cmdline(argc, argv, &igh_cfg))		//��ȡ����������
		return -1;

	if (!igh_cfg.dc_cycle || !igh_cfg.motor_cycle || !igh_cfg.system_slave_count) {
		usage(argv[0]);		//�������
		return -1;
	}

	igh_proc.x3e_slave_count = igh_cfg.system_slave_count - 1;
	igh_proc.slave_domain_regs_count = igh_proc.x3e_slave_count * 8 + 1;

	if (set_cpu_affinity(1)) {					// ����cpu
		printf("set cpu affinity error\n");
		return -1;
	}

	if (wait_master_ready(&igh_proc) ) {		// �ȴ���վ����
		return -1;
	}

	if ( (igh_proc.slave_servos = wait_slaves_ready(&igh_cfg, igh_cfg.system_slave_count, &igh_proc) ) == NULL) {	//�ȴ���վ����
		ret = -1;
		goto err_slave_servos;
	}

	if ( (ret = wait_master_domain_ready(&igh_cfg, &igh_proc) ) != 0) {			//�ȴ������
		goto err_slave_servos;
	}

	wait_dc_clk_ready(igh_cfg, &igh_proc);			//�ȴ�dcʱ��ͬ��

	// wait_sdo_ready(&igh_proc, sdo_request);
	

	for (int i = 1; i <= igh_proc.x3e_slave_count; ++i)
	{
		igh_proc.slave_servos[i].sdo_request = ecrt_slave_config_create_sdo_request(igh_proc.slave_servos[i].info.sc, 0x2115, 0x34, 2);
		ecrt_sdo_request_timeout(igh_proc.slave_servos[i].sdo_request, 500); // ms
	}


	printf("Activating master...\n");
	if ( (ret = ecrt_master_activate(igh_proc.master) ) != 0) {		// ������վ���ý׶��Ѿ���ɣ�ʵʱ��������ʼ��
		fprintf(stderr, "Master activate failed!\n");
		goto err_domain_regs;
	}

	igh_proc.domain_pd = ecrt_domain_data(igh_proc.domain);			// ������Ľ�������
	if (NULL == igh_proc.domain_pd) {
       		fprintf(stderr, "Get domain data failed!\n");
       		ret = -1;
       		goto err_domain_regs;
	}

	config_process();			//�������ȼ�

	printf("Starting RT task with dc_cycle=%lld(unused) motor_cycle=%lld ns.\n", igh_cfg.dc_cycle, igh_cfg.motor_cycle);

	static unsigned int sync_ref_counter = 0;
   	// get current time
	clock_gettime(CLOCK_REALTIME, &wakeupTime);			//��ȡϵͳʱ��
	cycle.tv_sec = 0;
	cycle.tv_nsec = igh_cfg.motor_cycle;				// �������
	while(1) {
	    wakeupTime = timespec_add(wakeupTime, cycle);	//ϵͳʱ��+�������
	    clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &wakeupTime, NULL);		// ˯��

	    // Write application time to master
	    // ������ʱ��д��վ
	    // It is a good idea to use the target time (not the measured time) as
	    // application time, because it is more stable.
	    // ʹ��Ŀ��ʱ��(�����ǲ���ʱ��)��ΪӦ��ʱ����һ�������⣬��Ϊ�����ȶ���
	    ecrt_master_application_time(igh_proc.master, TIMESPEC2NS(wakeupTime));	//����Ӧ��ʱ��
		
		//��SDO
		read_sdo(igh_proc.slave_servos[slaveconut].sdo_request,&slaveconut);
		//read_sdo(igh_proc.slave_servos[slaveconut].sdo_request2, &slaveconut);
		//write_sdo(igh_proc.slave_servos[slaveconut].sdo_request, 0x08, &slaveconut);
		(slaveconut <= 1) ? slaveconut = igh_proc.x3e_slave_count : slaveconut;

	    // receive process data
	    ecrt_master_receive(igh_proc.master);		// ��Ӳ���л�ȡ���յ���֡���������ݱ���
	    ecrt_domain_process(igh_proc.domain);		// ȷ��������ݱ�״̬

	    for(int i = 1; i <= igh_proc.x3e_slave_count; ++i)
			cyclic_task(&igh_proc, i);				// �˶����

	    if (sync_ref_counter) {
	        sync_ref_counter--;
	    } else {
	        sync_ref_counter = 1; // sync every cycle ͬ��ÿ������

	        clock_gettime(CLOCK_REALTIME, &time);		//��ȡϵͳʱ��
	        ecrt_master_sync_reference_clock_to(igh_proc.master, TIMESPEC2NS(time));
	    }
	    ecrt_master_sync_slave_clocks(igh_proc.master);		// ��DCʱ��Ư�Ʋ������ݱ��Ŷӷ��͡����д�վͬ�����ο�ʱ�ӡ�

	    // send process data
	    ecrt_domain_queue(igh_proc.domain);			// ����վ�����ݱ��������Ŷ����е������ݱ���
	    ecrt_master_send(igh_proc.master);			// ���Ͷ����е��������ݱ���
	}

err_domain_regs:
	free(igh_proc.domain_regs);		//�ͷ��ڴ�
err_slave_servos:
	free(igh_proc.slave_servos);	//�ͷ��ڴ�
	return ret;
}


/****************************************************************************/

