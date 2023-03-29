#ifndef __LOCAL_H
#define __LOCAL_H

/* Master 0, Slave 0, "dgio_16bit_in_16bit_out_demo" 
 * Vendor ID:       0xf0000001
 * Product code:    0x00001616
 * Revision number: 0x00000001
 */

#define VENDOR_ID_LOCAL	(0xf0000001)
#define PRODUCT_CODE_LOCAL	(0x00001616)
 
ec_pdo_entry_info_t slave_local_pdo_entries[] = {                                                                                        
	{0x3101, 0x01, 8}, /* Output */
	{0x3101, 0x02, 8}, /* Output */
	{0x3001, 0x01, 8}, /* Input */
	{0x3001, 0x02, 8}, /* Input */
};

ec_pdo_info_t slave_local_pdos[] = {
	{0x1600, 1, slave_local_pdo_entries + 0}, /* Outputs 1 */
	{0x1601, 1, slave_local_pdo_entries + 1}, /* Outputs 2 */
	{0x1a00, 1, slave_local_pdo_entries + 2}, /* Inputs 1 */
	{0x1a01, 1, slave_local_pdo_entries + 3}, /* Inputs 2 */
};

ec_sync_info_t slave_local_syncs[] = {
	{0, EC_DIR_OUTPUT, 1, slave_local_pdos + 0, EC_WD_ENABLE},
	{1, EC_DIR_OUTPUT, 1, slave_local_pdos + 1, EC_WD_ENABLE},
	{2, EC_DIR_INPUT, 2, slave_local_pdos + 2, EC_WD_DISABLE},
	{0xff}
};

#endif /* __LOCAL_H */