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

static rdt_message out_buf[MAX_SEQ + 1];
static seqn_t window_start;
static seqn_t next_seq_number;
static seqn_t to_send;

struct TimerItem {
    int id;
    double time;
    TimerItem *next;
};

static TimerItem _prehead = {-1, 0, nullptr};
static TimerItem *prehead = &_prehead;

static void Timer_AddTimeout(int id, double timeout) {
    TimerItem *cur = prehead;
    SENDER_INFO("Timer: setting %d", id);
    double dest = GetSimulationTime() + timeout;
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
    SENDER_INFO("Timer: cancelling %d", id);
    while(cur->next && cur->next->id != id) cur = cur->next;
    if(cur->next == nullptr) {
        SENDER_WARNING("%d not found in timer queue.", id);
        return;
    }
    // remove this entry from linked list
    TimerItem *target = cur->next;
    cur->next = target->next;
    delete target;
    if(cur == prehead) {
        // stop timer
        Sender_StopTimer();
        if(prehead->next != nullptr)
            Sender_StartTimer(prehead->next->time - GetSimulationTime());
    }
}

// timeout handler
static void Timer_Timeout(int id) {
    // resend that particular one
    assert(between(window_start, id, (window_start + WINDOW_SIZE) & MAX_SEQ));
    Timer_AddTimeout(id, SENDER_TIMEOUT);
    SENDER_INFO("Packet timeout, resending packet seq = %d", out_buf[id].seq);
    Sender_ToLowerLayer((packet *)(out_buf + id));
}

void Sender_Timeout()
{
    constexpr double epsilon = 5e-3;    // 5ms
    if(prehead->next == nullptr) {
        SENDER_ERROR("Clock time out and timer queue is empty.");
        return;
    }
    // check time for accuracy
    if(GetSimulationTime() < prehead->next->time - epsilon) {
        SENDER_ERROR("Warning: Clock timeout but not there yet. "
                    "Current time = %.2f, expected time = %.2f",
                    GetSimulationTime(), prehead->next->time);
    } else {
        // pop list
        do {
            auto item = prehead->next;
            prehead->next = prehead->next->next;
            int id = item->id;
            delete item;
            Timer_Timeout(id);
        } while(prehead->next &&
            GetSimulationTime() >= prehead->next->time - epsilon);
    }
    // restart timer for next event
    if(prehead->next)
        Sender_StartTimer(prehead->next->time - GetSimulationTime());
}

/* sender initialization, called once at the very beginning */
void Sender_Init()
{
    fprintf(stdout, "At %.2fs: sender initializing ...\n", GetSimulationTime());
    window_start = 0;
    next_seq_number = 1;
}

/* sender finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to take this opportunity to release some 
   memory you allocated in Sender_init(). */
void Sender_Final()
{
    fprintf(stdout, "At %.2fs: sender finalizing ...\n", GetSimulationTime());
}


/* send out all packets ready to be sent in current sliding window */
static void Sender_SendPackets() {
    uint8_t window_end = next_seq_number;
    if(lt((window_start + WINDOW_SIZE) & MAX_SEQ, window_end))
        window_end = (window_start + WINDOW_SIZE) & MAX_SEQ;
    while(between(window_start, to_send, window_end)) {
        rdt_message *buffer = out_buf + to_send;
        // outgoing packet have no flags
        buffer->flags = 0;
        buffer->fill_checksum();
        // add timer
        Timer_AddTimeout(buffer->seq, SENDER_TIMEOUT);
        SENDER_INFO( 
            "--> packet seq = %03d, len = %03d, checksum = %04x, window = %03d - %03d",
            buffer->seq, buffer->len, buffer->checksum, window_start, window_end);
        Sender_ToLowerLayer((packet *)buffer);
        inc(to_send);
    }
}

/* event handler, called when a message is passed from the upper layer at the 
   sender */
void Sender_FromUpperLayer(struct message *msg)
{
    // split the message and put it into buffer
    int cursor = 0; // points to the first unsent byte in the message
    while (cursor < msg->size) {
        rdt_message *buffer = out_buf + next_seq_number;
        buffer->len = std::min(RDT_PAYLOAD_MAXSIZE, msg->size - cursor);
        memcpy(buffer->payload, msg->data + cursor, buffer->len);
        // write seq
        buffer->seq = next_seq_number;
        // this is not a duplex protocol, so ack doesn't really matter here
        buffer->ack = 0;
        /* move the cursor */
        cursor += buffer->len;
        // update sequence number
        inc(next_seq_number);
        if(next_seq_number == window_start) {
            SENDER_ERROR("buffer pool is full, next_seq == window_start == %d", next_seq_number);
        }
    }
    SENDER_INFO("Token in packets, max sequence number = %d", seqn_t(next_seq_number - 1));
    Sender_SendPackets();
}

/* event handler, called when a packet is passed from the lower layer at the 
   sender */
void Sender_FromLowerLayer(struct packet *pkt)
{
    rdt_message *rdtmsg = (rdt_message *)pkt;
    // sender receive acks
    // check validity
    if(rdtmsg->check() == false) {
        SENDER_INFO("Received a corrupted packet.");
        return;
    }
    if(rdtmsg->flags == rdt_message::ACK) {
        SENDER_INFO("Received ack = %d, window = %d", rdtmsg->ack, window_start);
        while(lte(window_start, rdtmsg->ack)) {
            Timer_CancelTimeout(out_buf[window_start].seq);
            inc(window_start);
        }
        Sender_SendPackets();
    } else if(rdtmsg->flags == rdt_message::NAK) {
        SENDER_INFO("Sender received nak = %d, window = %d", rdtmsg->ack, window_start);
        if(lt(rdtmsg->ack, window_start)) {
            // nak is less than ack, packet reordered.
            SENDER_INFO("Ignoring nak since ack = %d", window_start);
        } else {
            // resend that particular package
            Timer_CancelTimeout(rdtmsg->ack);
            Timer_AddTimeout(rdtmsg->ack, SENDER_TIMEOUT);
            SENDER_INFO("--> Resending packet seq = %d len =% d checksum = %x",
                rdtmsg->ack, out_buf[rdtmsg->ack].len, out_buf[rdtmsg->ack].checksum);
            Sender_ToLowerLayer((packet *)(out_buf + rdtmsg->ack));
        }
    }
}
