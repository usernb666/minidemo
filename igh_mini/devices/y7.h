#ifndef __Y7_H
#define __Y7_H

#include "ecrt.h"

/* Master 0, Slave 1, "HCFA Y7 Servo Driver"
 * Vendor ID:       0x000116c7
 * Product code:    0x007e0402
 * Revision number: 0x00000001
 */

#define VENDOR_ID_Y7	(0x000116c7)
#define PRODUCT_CODE_Y7	(0x007e0402)

ec_pdo_entry_info_t slave_y7_pdo_entries[] = {
    {0x6040, 0x00, 16}, /* Control Word */
    {0x6060, 0x00, 8}, /* Modes of Operation */
    {0x607a, 0x00, 32}, /* Target Position */
    {0x60b8, 0x00, 16}, /* Touch probe function */
    {0x0000, 0x00, 0}, /* Gap */
    {0x0000, 0x00, 0}, /* Gap */
    {0x0000, 0x00, 0}, /* Gap */
    {0x0000, 0x00, 0}, /* Gap */
    {0x0000, 0x00, 0}, /* Gap */
    {0x0000, 0x00, 0}, /* Gap */
    {0x0000, 0x00, 0}, /* Gap */
    {0x0000, 0x00, 0}, /* Gap */
    {0x603f, 0x00, 16}, /* Error Code */
    {0x6041, 0x00, 16}, /* Status Word */
    {0x6061, 0x00, 8}, /* Modes of Operation Display */
    {0x6064, 0x00, 32}, /* Position Actual Value */
    {0x60b9, 0x00, 16}, /* Touch probe status */
    {0x60ba, 0x00, 32}, /* Touch Probe Pos1 Pos Value */
    {0x60f4, 0x00, 32}, /* Following error actual value */
    {0x0000, 0x00, 0}, /* Gap */
    {0x0000, 0x00, 0}, /* Gap */
    {0x0000, 0x00, 0}, /* Gap */
    {0x0000, 0x00, 0}, /* Gap */
    {0x0000, 0x00, 0}, /* Gap */
};

ec_pdo_info_t slave_y7_pdos[] = {
    {0x1600, 12, slave_y7_pdo_entries + 0}, /* 1st RxPdo mapping */
    {0x1a00, 12, slave_y7_pdo_entries + 12}, /* 1st TxPdo mapping */
};

ec_sync_info_t slave_y7_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
    {1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
    {2, EC_DIR_OUTPUT, 1, slave_y7_pdos + 0, EC_WD_ENABLE},
    {3, EC_DIR_INPUT, 1, slave_y7_pdos + 1, EC_WD_DISABLE},
    {0xff}
};


#endif /* __Y7_H */