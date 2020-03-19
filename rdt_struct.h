/*
 * FILE: rdt_struct.h
 * DESCRIPTION: The header file for basic data structures.
 * NOTE: Do not touch this file!
 */


#ifndef _RDT_STRUCT_H_
#define _RDT_STRUCT_H_

#include <cstdint>

/* sanity check utility */
#define ASSERT(x) \
    if (!(x)) { \
        fprintf(stdout, "## at file %s line %d: assertion fails\n", __FILE__, __LINE__); \
        exit(-1); \
    }

/* a message is a data unit passed between the upper layer and the rdt layer at 
   the sender */
struct message {
    int size;
    char *data;
};

/* a packet is a data unit passed between rdt layer and the lower layer, each 
   packet has a fixed size */
#define RDT_PKTSIZE 128

struct packet {
    char data[RDT_PKTSIZE];
};

/* the internal packet structure of rdt protocol. */
/*
 *
 * Quick note about my implementation
 * packet format:
 * |  1  |  1  |  1  |  1  |  2  |       the rest(len)       |
 * | ack | syn | len | rsv | chk |          payload          |
 * 
 * the whole message(except for checksum itself) is checksumed with crc16.
 * crc-16-ccitt (the one used in redis) is used here, and generator function of
 * which is:
 * x**16 + x**12 + x**5 + 1
 * implementation can be found in crc16.cc
 * 
 */
const int RDT_HEADER_SIZE = 6;
const int RDT_PAYLOAD_MAXSIZE = RDT_PKTSIZE - RDT_HEADER_SIZE;

struct rdt_message {
    uint8_t syn;
    uint8_t ack;
    uint8_t len;
    uint8_t reserved;
    uint16_t checksum;
    char payload[RDT_PAYLOAD_MAXSIZE];

    rdt_message(): reserved(0) {}
};

// make sure that internal packet and low level packet are of the same size
static_assert(sizeof(rdt_message) == sizeof(packet));

#endif  /* _RDT_STRUCT_H_ */
