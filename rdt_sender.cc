/*
 * FILE: rdt_sender.cc
 * DESCRIPTION: Reliable data transfer sender.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <queue>
#include <algorithm>

#include "rdt_sender.h"
#include "rdt_utils.h"

// packet ring buffer
static rdt_message out_buf[MAX_SEQ + 1];
static std::queue<rdt_message> external_buffer;
// parameters
static seqn_t window_start;
static seqn_t next_seq_number;
static seqn_t to_send;

/*
 * Timer queue implementation
 */

struct TimerItem {
    int id;
    double time;
    TimerItem *next;
};

static TimerItem _prehead = {-1, 0, nullptr};
static TimerItem *prehead = &_prehead;

// add timeout item into timer queue
static void Timer_AddTimeout(int id, double timeout) {
    TimerItem *cur = prehead;
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

// remove timeout item from timer queue
// this will only remove the most recent timeout item
static void Timer_CancelTimeout(int id) {
    TimerItem *cur = prehead;
    while(cur->next && cur->next->id != id) cur = cur->next;
    if(cur->next == nullptr) {
        SENDER_ERROR("%d not found in timer queue.", id);
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

static void Timer_Timeout(int);
// system timer event handler
void Sender_Timeout()
{
    constexpr double epsilon = 5e-3;    // 5ms
    if(prehead->next == nullptr) {
        SENDER_ERROR("Clock time out and timer queue is empty.");
        return;
    }
    while(prehead->next && GetSimulationTime() >=
        prehead->next->time - epsilon) {
        auto item = prehead->next;
        prehead->next = prehead->next->next;
        int id = item->id;
        delete item;
        Timer_Timeout(id);
    } 
    // restart timer for next event
    if(prehead->next)
        Sender_StartTimer(prehead->next->time - GetSimulationTime());
}

// timeout handler
static void Timer_Timeout(int id) {
    // there're two types of timeout, ACK timeout and NAK timeout
    // we need to resend that packet & restart timer either way
    bool is_nak = bool(out_buf[id].flags & rdt_message::NAKING);
    SENDER_INFO("Packet timeout, resending packet seq = %d, isnak = %d",
        out_buf[id].seq, is_nak);
    if(is_nak) out_buf[id].flags &= ~rdt_message::NAKING;
    Sender_ToLowerLayer((packet *)(out_buf + id));
    if(is_nak) out_buf[id].flags |= rdt_message::NAKING;
    if(is_nak) Timer_AddTimeout(id, NAK_TIMEOUT);
    else Timer_AddTimeout(id, SENDER_TIMEOUT);
}

// Advance sliding window
// Fetch buffer content from external buffer if necessary.
void Sender_AdvanceWindow() {
    if(!external_buffer.empty()) {
        out_buf[next_seq_number] = external_buffer.front();
        external_buffer.pop();
        out_buf[next_seq_number].seq = next_seq_number;
        SENDER_INFO("Retrieving from buffer(%ld), seq=%d", external_buffer.size(), window_start);
        inc(next_seq_number);
    } else {
        // invalidate buffer
        out_buf[window_start].len = 0;
    }
    inc(window_start);
}

/* sender initialization, called once at the very beginning */
void Sender_Init()
{
    SENDER_INFO("Initializing...");
    window_start = 0;
    next_seq_number = 1;
}

/* sender finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to take this opportunity to release some 
   memory you allocated in Sender_init(). */
void Sender_Final()
{
    SENDER_INFO("Finalizing...");
}

// send out all packets ready to be sent in current sliding window
static void Sender_SendPackets() {
    uint8_t window_end = add(window_start, WINDOW_SIZE);
    if(between(window_start, next_seq_number, window_end))
        window_end = next_seq_number;
    while(between(window_start, to_send, window_end)) {
        rdt_message *buffer = out_buf + to_send;
        // not a duplex protocol, ack doesn't matter here
        buffer->ack = 0;
        // outgoing packet have no flags
        buffer->flags = 0;
        buffer->fill_checksum();
        // add timer
        Timer_AddTimeout(buffer->seq, SENDER_TIMEOUT);
        SENDER_INFO( 
            "--> packet seq = %03d, len = %03d, window = %03d - %03d",
            buffer->seq, buffer->len, window_start, window_end);
        Sender_ToLowerLayer((packet *)buffer);
        inc(to_send);
    }
}

/* event handler, called when a message is passed from the upper layer at the 
   sender */
void Sender_FromUpperLayer(struct message *msg)
{
    // calculate current window range
    uint8_t window_end = add(window_start, WINDOW_SIZE);
    if(between(window_start, next_seq_number, window_end))
        window_end = next_seq_number;
    int cursor = 0; // points to the first unsent byte in the message
    // split the message and put it into buffer
    while (cursor < msg->size) {
        rdt_message *buffer;
        seqn_t before_next = minus(next_seq_number, 1);
        // note that next_seq_number == window_start iff there's nothing more to transfer
        if(add(next_seq_number, 1) == window_start) {
            // ring buffer is full, append to external buffer queue
            if(external_buffer.empty() || external_buffer.back().len == RDT_PAYLOAD_MAXSIZE)
                external_buffer.emplace();
            buffer = &(external_buffer.back());
            SENDER_INFO("Appending to queue(%ld)", external_buffer.size());
        } else if(lt(window_end, before_next) && out_buf[before_next].len < RDT_PAYLOAD_MAXSIZE) {
            // outside the sliding window, and the last buffer is still not full
            // fillout this buffer first
            buffer = out_buf + before_next;
        } else {
            // inside the sliding window
            // or outside the sliding window and last buffer is full
            // append to next buffer item
            buffer = out_buf + next_seq_number;
            buffer->seq = next_seq_number;
            buffer->len = 0;
            inc(next_seq_number);
        }
        // write content
        int delta = std::min(RDT_PAYLOAD_MAXSIZE - buffer->len, msg->size - cursor);
        memcpy(buffer->payload + buffer->len, msg->data + cursor, delta);
        buffer->len += delta;
        cursor += delta;  // move the cursor
    }
    SENDER_INFO("Added new content, next sequence number = %d", next_seq_number);
    Sender_SendPackets();
}

/* event handler, called when a packet is passed from the lower layer at the 
   sender */
void Sender_FromLowerLayer(struct packet *pkt)
{
    rdt_message *rdtmsg = (rdt_message *)pkt;
    // check validity
    if(rdtmsg->check() == false) {
        SENDER_INFO("x<- Packet corrupted.");
        return;
    }
    if(rdtmsg->flags == rdt_message::ACK) {
        // received ack, advance window position
        SENDER_INFO("o<- ack = %d", rdtmsg->ack);
        while(lte(window_start, rdtmsg->ack)) {
            Timer_CancelTimeout(out_buf[window_start].seq);
            Sender_AdvanceWindow();
        }
        Sender_SendPackets();
    } else if(rdtmsg->flags == rdt_message::NAK) {
        // received nak, check & resend requested packet
        SENDER_INFO("o<- nak = %d", rdtmsg->ack);
        seqn_t seq = rdtmsg->ack;
        if(lt(seq, window_start)) {
            // nak is less than ack, packet reordered.
            SENDER_INFO("Ignoring nak since ack = %d", window_start);
        } else {
            // resend that particular package
            // nak of the same packet may come back multiple times in a row
            // set a timeout for it between retrying to avoid useless transfer
            if(!(out_buf[seq].flags & rdt_message::NAKING)) {
                Timer_CancelTimeout(seq);
                SENDER_INFO("--> Resending packet seq = %d len = %d", seq, out_buf[seq].len);
                Timer_AddTimeout(seq, NAK_TIMEOUT);
                Sender_ToLowerLayer((packet *)(out_buf + seq));
                out_buf[seq].flags |= rdt_message::NAKING;
            }
        }
    }
}
