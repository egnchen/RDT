/*
 * FILE: rdt_struct.h
 * DESCRIPTION: The header file for basic data structures.
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
 * packet format:
 * |  1  |  1  |  1  |  1  |  2  |       the rest(len)       |
 * | ack | seq | len | flg | chk |          payload          |
 * 
 * ack: Acknowledge number, indicating receiver's sliding window start.
 * seq: Current packet's sequence number.
 * len: Length of the payload.
 * flg: Flags. Currently only the LSB is used to indicate ACK or NAK.
 *      In buffer implementations higher bits in this field can be used to
 *      log useful states, but these higher bits **must** be set to zero
 *      when it's checksumed.
 * chk: Checksum of the whole packet(excluding checksum itself) with CRC16.
 *      CRC16-ccitt (the one used in redis) is used here.
 *      Generator function of which is: x**16 + x**12 + x**5 + 1
 *      implementation can be found in rdt_utils.cc
 * 
 * In a unidirectional protocol:
 * * Sender can only set the seq number. Ack number doesn't mean anything.
 * * Receiver can only set the ack number. Seq number doesn't mean anything.
 * 
 * All other fields are compulsory. Note that even if some values doesn't have
 * meaning, they will be checksumed anyway.
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

// shared parameters
const seqn_t MAX_SEQ = 255;
const seqn_t WINDOW_SIZE = 8;
const double SENDER_TIMEOUT = 1;
const double NAK_TIMEOUT = 0.3;

// make sure MAX_SEQ is 2**n - 1 and WINDOW_SIZE is 2**n
static_assert((int(MAX_SEQ) & (int(MAX_SEQ) + 1)) == 0);
static_assert((WINDOW_SIZE & (WINDOW_SIZE - 1)) == 0);

// helper functions to calculate sequence numbers
inline void inc(seqn_t &s) { ++s; s &= MAX_SEQ; }
inline seqn_t add(seqn_t a, seqn_t b) { return (a + b) & MAX_SEQ; }
inline seqn_t minus(seqn_t a, seqn_t b) { return (a - b) & MAX_SEQ; }
inline bool lt(seqn_t a, seqn_t b) { return (int8_t)(a - b) < 0; }
inline bool lte(seqn_t a, seqn_t b) { return (int8_t)(a - b) <= 0; }
inline bool between(seqn_t a, seqn_t b, seqn_t c) { return (lt(a,b) || a==b) && lt(b,c); }

#endif  /* _RDT_STRUCT_H_ */
