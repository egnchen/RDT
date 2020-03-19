/*
 * FILE: rdt_sender.cc
 * DESCRIPTION: Reliable data transfer sender.
 * NOTE: This implementation assumes there is no packet loss, corruption, or 
 *       reordering.  You will need to enhance it to deal with all these 
 *       situations.  In this implementation, the packet format is laid out as 
 *       the following:
 *       
 *       |<-  1 byte  ->|<-             the rest            ->|
 *       | payload size |<-             payload             ->|
 *
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */


#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>

#include "rdt_struct.h"
#include "rdt_sender.h"
#include "rdt_utils.h"

using namespace utils;

const int MAX_SEQ = 255;
const int WINDOW_SIZE = 16;
static packet in_buf[WINDOW_SIZE];
static packet out_buf[WINDOW_SIZE];

struct TimerItem {
    int id;
    double time;
    TimerItem *next;
};

static TimerItem _prehead = {0, 0, nullptr};
static TimerItem *prehead = &_prehead;

static void Timer_AddTimeout(int id, double timeout) {
    TimerItem *cur = prehead;
    time_t dest = GetSimulationTime() + timeout;
    while(cur->next && cur->next->time < dest) cur = cur->next;
    TimerItem *new_item = new TimerItem{id, dest, cur->next};
    cur->next = new_item;
    if(cur == prehead) {
        // reset the timer
        if(Sender_isTimerSet())
            Sender_StopTimer();
        Sender_StartTimer(dest - GetSimulationTime());
    }
}

static void Timer_CancelTimeout(int id) {
    TimerItem *cur = prehead;
    while(cur->next && cur->next->id != id) cur = cur->next;
    if(cur->next == nullptr) {
        fprintf(stderr, "Error: %d not found in timer queue.\n", id);
        return;
    }
    // remove this entry from linked list
    TimerItem *target = cur->next;
    cur->next = target->next;
    delete target;
}


void Sender_Timeout()
{
    constexpr double epsilon = 5e-3;    // 5ms
    if(prehead->next == nullptr) {
        fprintf(stderr, "Error: Clock time out and timer queue is empty.\n");
        return;
    }
    // check time for accuracy
    if(GetSimulationTime() < prehead->next->time - epsilon) {
        fprintf(stdout, "Warning: Clock time out but not there yet."
                        "Current time = %f, expected time = %f\n",
                        GetSimulationTime(), prehead->next->time);
    } else {
        // pop list
        TimerItem *item = prehead->next;
        prehead->next = prehead->next->next;
        delete item;
    }
    // restart timer for next event
    if(prehead->next)
        Sender_StartTimer(prehead->next->time - GetSimulationTime());
}

template <typename T>
static inline bool between(T a, T b, T c) {
    return a < c ? (a <= b && b < c) : (a <= b || b < c);
}

/* sender initialization, called once at the very beginning */
void Sender_Init()
{
    fprintf(stdout, "At %.2fs: sender initializing ...\n", GetSimulationTime());
}

/* sender finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to take this opportunity to release some 
   memory you allocated in Sender_init(). */
void Sender_Final()
{
    fprintf(stdout, "At %.2fs: sender finalizing ...\n", GetSimulationTime());
}

/* event handler, called when a message is passed from the upper layer at the 
   sender */
void Sender_FromUpperLayer(struct message *msg)
{
    /* split the message if it is too big */

    /* reuse the same packet data structure */
    rdt_message buffer;

    int cursor = 0; // points to the first unsent byte in the message

    while (cursor < msg->size) {
        buffer.len = std::max(RDT_PAYLOAD_MAXSIZE, msg->size - cursor);
        memcpy(buffer.payload, msg->data + cursor, buffer.len);

        // calculate checksum
        int crc = CRC16::calc((const char *)&buffer, 4 * sizeof(uint8_t));
        crc = CRC16::calc(buffer.payload, buffer.len, crc);
        
        // write the checksum, big-endian
        buffer.checksum = (crc >> 8) + (crc << 8) & 0xff;
        /* send it out through the lower layer */
        Sender_ToLowerLayer((packet *)&buffer);

        /* move the cursor */
        cursor += buffer.len;
    }
}

/* event handler, called when a packet is passed from the lower layer at the 
   sender */
void Sender_FromLowerLayer(struct packet *pkt)
{
}

