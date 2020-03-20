#include "rdt_utils.h"

/*
 * CRC16 implemented in table-lookup
 * this code is derived from redis
 */
 

/*
 * Calculate CRC-16 checksum
 */
uint16_t calc_crc16(const char *buf, int len, uint16_t crc) {
    for (int counter = 0; counter < len; counter++)
        crc = (crc<<8) ^ crc16tab[((crc>>8) ^ *buf++)&0x00FF];
    return crc;
}

/*
 * Check whether a buffer containing a CRC-16 checksum is valid.
 */
bool check_crc16(const char *buf, int len) {
    return calc_crc16(buf, len) == 0;
}