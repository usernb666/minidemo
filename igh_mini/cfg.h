/*
 * cfg.h
 *
 *  Created on: Jun 28, 2022
 *      Author: user
 */

#ifndef CFG_H_
#define CFG_H_

enum {
	SLAVE_TYPE_UNKNOW	= 0,
	SLAVE_TYPE_X3E	= 1,
	SLAVE_TYPE_Y7	= 2,
};

struct igh_global_cfg {
	uint64_t dc_cycle;			//dc同步周期
	uint64_t motor_cycle;		//电机周期
	uint32_t system_slave_count;//系统从站数量
	bool debug;
	uint8_t type;
};

struct slave_info_t {
	uint16_t alias;
	uint16_t position;
	uint32_t vendor_id;
	uint32_t product_code;
	ec_slave_config_t *sc;
	ec_slave_config_state_t state;
};

struct slave_servo_t {
	unsigned int cur_servo_mode;	//0x6061 控制模式显示
	unsigned int set_servo_mode;	//0x6060 控制模式
	unsigned int statusWord;    	//0x6041 状态字
	unsigned int ctrlWord;      	//0x6040 控制字
	unsigned int cur_position;  	//0x6064 位置反馈
	unsigned int set_position;  	//0x607A 目标位置
	unsigned int set_velocity;  	//0x60ff 目标速度
	unsigned int error_code;    	//0x603f 错误代码
	unsigned int servo_error;   	//0x213f 伺服内部错误码
	ec_sdo_request_t* sdo_request1,sdo_request2;
	struct slave_info_t info;
};

typedef struct {
	ec_master_t *master;
	ec_domain_t *domain;
	uint8_t *domain_pd;
	struct slave_servo_t *slave_servos;
	ec_pdo_entry_reg_t *domain_regs;
	void *data;
	uint32_t x3e_slave_count;
	uint32_t slave_domain_regs_count;
	
}igh_proc_data;



#endif /* CFG_H_ */
