#ifndef __X3E_H
#define __X3E_H

#include "ecrt.h"

/* Master 0, Slave 0, "HCFA X3E Servo Driver"
 * Vendor ID:       0x000116c7
 * Product code:    0x003e0402
 * Revision number: 0x00000001
 */


#define VENDOR_ID_X3E	(0x000116c7)
#define PRODUCT_CODE_X3E	(0x003e0402)

ec_pdo_entry_info_t slave_x3e_pdo_entries[] = {
	{0x6040, 0x00, 16}, /* Control Word */
	{0x6060, 0x00, 8}, /* Modes of operation  */
	{0x607a, 0x00, 32}, /* Target position */
	{0x60ff, 0x00, 32}, /*set_velocity*/
	{0x6071, 0x00, 16}, /* set_torque*/

	{0x603f, 0x00, 16}, /* Error Code */
	{0x6041, 0x00, 16}, /* Status Word */
	{0x6061, 0x00, 8}, /* Modes of operation display  */
	{0x6064, 0x00, 32}, /* Position actual value */
	{0x606c, 0x00, 32}, /* Position actual value */
	{0x6077, 0x00, 16}, /*cur_torque */
	{0x213f, 0x00, 16}, /* Servo Error Code */
};

ec_pdo_info_t slave_x3e_pdos[] = {
	{0x1600, 5, slave_x3e_pdo_entries + 0}, /* 1st RxPDO-Mapping */
	{0x1a00, 7, slave_x3e_pdo_entries + 5}, /* 1st TxPDO-Mapping */
};

ec_sync_info_t slave_x3e_syncs[] = {
	{0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
	{1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
	{2, EC_DIR_OUTPUT, 1, slave_x3e_pdos + 0, EC_WD_ENABLE},
	{3, EC_DIR_INPUT, 1, slave_x3e_pdos + 1, EC_WD_DISABLE},
	{0xff}
};

#endif /* __X3E_H */