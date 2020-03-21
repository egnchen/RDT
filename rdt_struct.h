/*
 * FILE: rdt_struct.h
 * DESCRIPTION: The header file for basic data structures.
 * NOTE: Do not touch this file!
 */
#ifndef _RDT_STRUCT_H_
#define _RDT_STRUCT_H_

#include <cstdint>
#include <cassert>

#include "rdt_utils.h"

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
 * | ack | seq | len | flg | chk |          payload          |
 * 
 * the whole message(except for checksum itself) is checksumed with crc16.
 * crc-16-ccitt (the one used in redis) is used here, and generator function of
 * which is:
 * x**16 + x**12 + x**5 + 1
 * implementation can be found in crc16.cc
 * 
 */
typedef uint8_t seqn_t;

constexpr int RDT_HEADER_SIZE = 6;
constexpr int RDT_PAYLOAD_MAXSIZE = RDT_PKTSIZE - RDT_HEADER_SIZE;

struct rdt_message {
    seqn_t seq;
    seqn_t ack;
    uint8_t len;
    uint8_t flags;
    uint16_t checksum;
    char payload[RDT_PAYLOAD_MAXSIZE];
    
    static constexpr uint8_t ACK = 0, NAK = 1;
    // higher bits are for checking in internal buffers
    // checksum shouldn't be calculated when these bits are set
    static constexpr uint8_t ACKED = 2;
    static constexpr uint8_t RECEIVED = 4;
    static constexpr uint8_t NAKING = 8;

    rdt_message(): len(0), flags(0) {}

    inline uint16_t get_checksum() {
        assert(len <= RDT_PAYLOAD_MAXSIZE);
        assert((flags & 0xFE) == 0);
        constexpr int real_header_size = RDT_HEADER_SIZE - sizeof(uint16_t);
        uint16_t crc = CRC16::calc((const char *)this, real_header_size);
        crc = CRC16::calc(this->payload, this->len, crc);
        return crc;
    }

    // store checksum in the struct, little endian.
    inline void fill_checksum() {
        this->checksum = get_checksum();
    }

    // check if the packet is corrupted.
    inline bool check() {
        if(len > RDT_PAYLOAD_MAXSIZE)
            return false;
        if((this->flags & 0xFE) != 0)
            return false;
        uint16_t s = get_checksum();
        if(this->checksum != s)
            return false;
        return true;
    }
};

// make sure that internal packet and low level packet are of the same size
static_assert(sizeof(rdt_message) == sizeof(packet));

constexpr seqn_t MAX_SEQ = 255;
constexpr seqn_t WINDOW_SIZE = 16;
constexpr double SENDER_TIMEOUT = 1;
constexpr double NAK_TIMEOUT = 0.2;

// make sure MAX_SEQ is 2**n - 1 and WINDOW_SIZE is 2**n
static_assert((int(MAX_SEQ) & (int(MAX_SEQ) + 1)) == 0);
static_assert((WINDOW_SIZE & (WINDOW_SIZE - 1)) == 0);

// helper functions
inline void inc(uint8_t &s) { ++s; s &= MAX_SEQ; }
inline uint8_t add(uint8_t a, uint8_t b) { return (a + b) & MAX_SEQ; }
inline bool lt(uint8_t a, uint8_t b) { return (int8_t)(a - b) < 0; }
inline bool lte(uint8_t a, uint8_t b) { return (int8_t)(a - b) <= 0; }
inline bool between(uint8_t a, uint8_t b, uint8_t c) { return (lt(a,b) || a==b) && lt(b,c); }

#endif  /* _RDT_STRUCT_H_ */
