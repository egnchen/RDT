/*
 * Defines debug info utilities, checksum library and packet format.
 */
#include <cstdint>
#include <cassert>

#include "rdt_struct.h"

// output macros
#define SENDER_INFO(format, ...) \
    fprintf(stdout, "[%.2fs][INFO][ sender ]", GetSimulationTime());\
    fprintf(stdout, format "\n", ##__VA_ARGS__);

#define SENDER_WARNING(format, ...) \
    fprintf(stdout, "[%.2fs][WARN][ sender ]", GetSimulationTime());\
    fprintf(stdout, format "\n", ##__VA_ARGS__);

#define SENDER_ERROR(format, ...) \
    fprintf(stderr, "[%.2fs][EROR][ sender ]", GetSimulationTime());\
    fprintf(stderr, format "\n", ##__VA_ARGS__);

#define RECEIVER_INFO(format, ...) \
    fprintf(stdout, "[%.2fs][INFO][receiver]", GetSimulationTime());\
    fprintf(stdout, format "\n", ##__VA_ARGS__);
    
#define RECEIVER_WARNING(format, ...) \
    fprintf(stdout, "[%.2fs][WARN][receiver]", GetSimulationTime());\
    fprintf(stdout, format "\n", ##__VA_ARGS__);

#define RECEIVER_ERROR(format, ...) \
    fprintf(stderr, "[%.2fs][EROR][receiver]", GetSimulationTime());\
    fprintf(stderr, format "\n", ##__VA_ARGS__);

// CRC16 checksum library
// CRC16-ccitt (the one used in redis) is used here.
// Generator function of which is: x**16 + x**12 + x**5 + 1
// implementation can be found in rdt_utils.cc
class CRC16 {
private:
    static const uint16_t crc16tab[];
public:
    static uint16_t calc(const char *buf, int len, uint16_t crc = 0);
    static bool check(const char *buf, int len);
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
