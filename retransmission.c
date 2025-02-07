#include "retransmission.h"

int retransmit(TloeEther *ether, CircularQueue *retransmit_buffer, int seq_num_nack) {
    int i, n;
    TloeFrame frame;
    // update ACKD_SEQ ????
    // retransmit 
    n = 0;
    for (i=retransmit_buffer->front; i != retransmit_buffer->rear; i = (i + 1) % retransmit_buffer->size){
        RetransmitBufferElement *element = (RetransmitBufferElement *) retransmit_buffer->data[i];
        if (element->tloe_frame.seq_num > seq_num_nack) {

			frame = element->tloe_frame;
			//frame.mask = 1;		// Indicate to normal packet

			printf("Retransmission with num_seq: %d\n", frame.seq_num);
            tloe_ether_send(ether, (char *)&frame, sizeof(TloeFrame));
            element->state = TLOE_RESENT;
        }
    }

    return n;
}

int slide_window(TloeEther *ether, CircularQueue *retransmit_buffer, int seq_num_ack) {
    RetransmitBufferElement *e;
	int acked_seq = 0;

	if (seq_num_ack > 1030)
		while(1);

    // dequeue TLoE frames from the retransmit buffer
    e = (RetransmitBufferElement *) getfront(retransmit_buffer);
    while (e != NULL && e->tloe_frame.seq_num <= seq_num_ack) {
        e = (RetransmitBufferElement *) dequeue(retransmit_buffer);
//        printf("RX: frame.seq_num_ack: %d, element->seq_num: %d\n", seq_num_ack, e->tloe_frame.seq_num);
        acked_seq = e->tloe_frame.seq_num;
        if (e) free(e);
        e = (RetransmitBufferElement *) getfront(retransmit_buffer);
    }

	return acked_seq;
}

bool isTimeout(time_t send_time) {
    return difftime(time(NULL), send_time) >= TIMEOUT_SEC;
}

