#ifndef __TLOE_TRANSMITTER_H__
#define __TLOE_TRANSMITTER_H__

#include "tloe_frame.h"
#include "tloe_ether.h"
#include "util/circular_queue.h"
#include "retransmission.h"
#include "flowcontrol.h"

extern CircularQueue *ack_buffer;
extern CircularQueue *retransmit_buffer;
extern FlowControlCredit *fc_credit;
extern int next_tx_seq;
extern int acked_seq;

TloeFrame *TX(TloeFrame *tloeframe, TloeEther *ether);

#endif // __TLOE_TRANSMITTER_H__
