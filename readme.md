# Reliable Data Transport Protocol

> Author 陈奕君 517021910387 cyj205@sjtu.edu.cn

This repository implements a reliable data transfer protocol in a simulated environment. It mainly uses **slinding window** and **selective repeat** strategy to keep packets intact and in order. A **CRC16 checksum** algorithm is implemented to deal with data corruption.

Files included:

* `rdt_sender.cc` Logic of sender, which is the main part. A timer queue is implemented here.
* `rdt_receiver.cc` Logic of receiver, only a small amount compared to that of the sender.
* `rdt_utils.h`, `rdt_utils.cc` Define logging utilities, checksum and packet format definition.

Packet format can be found in `rdt_utils.h`, here are the explanations:

```cpp
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
```

The implementation themselves are filled with useful notes, if you want the details you should check those out. Here's the gist:

* **Sender buffer**: A ring buffer is used store information from upper layer. Packets within the sliding window are filled carelessly, and those outside the window are guaranteed to be filled up. When the ring buffer is full, incoming data are filled into an external queue buffer.
* **Timeout**: Every sent packet have a timeout interval. Once ths time is drained and no ACK is received, the packet will be resent and timer will be restarted with the same timeout interval as before. A simple linked-list-based timer queue is implemented to realize this.
* **Checksumming**: CRC16-CCITT, table-lookup method. Packet header and payload are both included.s
* **NAK policy**:
    * The receiver will response NAK repeatedly when there's a hole in the sliding window.
    * Once the sender receives the NAK, it will send back that packet immediately if it has never been resent before. Pursuing NAK responses will be ignored.
    * The resent packet will then timeout, be resent like regular packets, except that its interval will be much shorter than that of regular ones.
    
    This decision is made since there's no timer for the receiving side.