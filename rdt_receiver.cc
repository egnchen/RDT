/*
 * FILE: rdt_receiver.cc
 * DESCRIPTION: Reliable data transfer receiver.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rdt_struct.h"
#include "rdt_receiver.h"

static seqn_t window_start;
static seqn_t received_last;
static rdt_message in_buf[MAX_SEQ + 1];

/* receiver initialization, called once at the very beginning */
void Receiver_Init()
{
    fprintf(stdout, "At %.2fs: receiver initializing ...\n", GetSimulationTime());
    window_start = 0;
    received_last = 0;
}

/* receiver finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to use this opportunity to release some 
   memory you allocated in Receiver_init(). */
void Receiver_Final()
{
    fprintf(stdout, "At %.2fs: receiver finalizing ...\n", GetSimulationTime());
}

/* event handler, called when a packet is passed from the lower layer at the 
   receiver */
void Receiver_FromLowerLayer(struct packet *pkt)
{
    static rdt_message buffer;
    rdt_message *rdtmsg = (rdt_message *)pkt;

    RECEIVER_INFO("Received a package, seq = %d, window = %d", rdtmsg->seq, window_start);
    if(!rdtmsg->check()) {
        RECEIVER_INFO("Package corrupted, seq = %d?", rdtmsg->seq);
        return;
    }
    
    // if this packet's sequence number is within our range
    if(!lt(rdtmsg->seq, window_start)) {
        // update the lastest received packet number
        if(lt(received_last, rdtmsg->seq))
            received_last = rdtmsg->seq;
        // copy to our internal buffer
        memcpy(in_buf + rdtmsg->seq, rdtmsg, sizeof(rdt_message));
        rdtmsg = in_buf + rdtmsg->seq;
        // set received flag
        rdtmsg->flags |= rdt_message::RECEIVED;
        rdtmsg->flags &= ~rdt_message::NAKED;

        while(in_buf[window_start].flags & rdt_message::RECEIVED) {
            message msg = message{
                int(in_buf[window_start].len), in_buf[window_start].payload};
            Receiver_ToUpperLayer(&msg);
            // invalidate this buffer by unsetting RECEIVED
            in_buf[window_start].flags &= ~rdt_message::RECEIVED;
            inc(window_start);
        }

        // the next frame has yet not been received, send nak
        // we don't have timer on receiver side, so we have no way to timeout
        // nak here. If nak is lost, this method will deteriorate to GBN.
        if(lt(window_start, rdtmsg->seq) && lt(rdtmsg->seq, received_last)) {
            if((in_buf[window_start].flags & rdt_message::NAKED) == 0) {
                in_buf[window_start].flags |= rdt_message::NAKED;
                buffer.seq = 0;
                buffer.ack = window_start;
                buffer.flags = rdt_message::NAK;
                buffer.len = 0;
                buffer.fill_checksum();
                RECEIVER_INFO("<-- nak = %d", window_start);
                Receiver_ToLowerLayer((packet *)&buffer);
                return;
            }
        }
    } else {
        RECEIVER_INFO("Packet seq less than window number, not saved.");
    }
    // send back ack
    buffer.seq = 0; // not a duplex protocol
    buffer.ack = (window_start - 1) & MAX_SEQ;
    buffer.flags = rdt_message::ACK;
    buffer.len = 0;
    buffer.fill_checksum();
    RECEIVER_INFO("<-- ack = %d", buffer.ack);
    Receiver_ToLowerLayer((packet *)&buffer);
}
