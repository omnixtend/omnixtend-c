#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdlib.h>
#include "tloe_receiver.h"
#include "tloe_frame.h"
#include "tloe_endpoint.h"
#include "retransmission.h"
#include "tilelink_handler.h"

static int ack_cnt = 0;
static int tlmsg_drop_cnt = 0;

void RX(TloeEther *ether) {
    int size;
    TloeFrame *tloeframe = (TloeFrame *)malloc(sizeof(TloeFrame));

//  if ((time(NULL) - last_ack_time) >= ACK_TIMEOUT_SEC;

    // Receive a frame from the Ethernet layer
    size = tloe_ether_recv(ether, (char *)tloeframe);
    if (size == -1 && errno == EAGAIN) {
        // No data available, return without processing
        //printf("File: %s line: %d: tloe_ether_recv error\n", __FILE__, __LINE__);
        free(tloeframe);
        return;
    }

  //  printf("RX: Received packet with seq_num: %d, seq_num_ack: %d, ack: %d\n", tloeframe->seq_num, tloeframe->seq_num_ack, tloeframe->ack);

    int diff_seq = next_rx_seq - tloeframe->seq_num;
    if (diff_seq < 0)
        diff_seq += MAX_SEQ_NUM + 1;

    // If the received frame has the expected sequence number
    if (diff_seq == 0) {
        if (is_ack_msg(tloeframe)) {
            // ACK/NAK (zero-TileLink)
            acked_seq = slide_window(ether, retransmit_buffer, tloeframe->seq_num_ack);

            // Handle ACK/NAK and finish processing
            if (tloeframe->ack == 0) {  // NACK
                retransmit(ether, retransmit_buffer, tloeframe->seq_num_ack);
            }

			// Increase Credits for flow control
			//inc_credit(fc_credit, tloeframe->channel, tloeframe->credit);

            // Update sequence numbers
            next_rx_seq = (tloeframe->seq_num + 1) % (MAX_SEQ_NUM + 1);
            acked_seq = tloeframe->seq_num_ack;
            ack_cnt++;

            free(tloeframe);

            if (ack_cnt % 100 == 0) {
                printf("next_tx: %d, ackd: %d, next_rx: %d, ack_cnt:%d, cha:%d, credit:%d (%d)\n", next_tx_seq, acked_seq, next_rx_seq, ack_cnt, CHANNEL_A, get_credit(fc_credit, CHANNEL_A), tlmsg_drop_cnt);
            }
        } else {
            // Normal request packet
            // Handle and enqueue it into the message buffer
            TloeFrame *frame = malloc(sizeof(TloeFrame));
			TileLinkMsg *tl = malloc(sizeof(TileLinkMsg));
			int channel = 0;
			int credit = 0;
			int ack = 1; // ACK:1, NAK:0
			int seq_num = tloeframe->seq_num;

			if (is_queue_full(tl_message_buffer)) { 
				ack = 0;
				seq_num = (next_rx_seq - 1) < 0 ? MAX_SEQ_NUM : next_rx_seq - 1;  
			}

			/* random drop: p = 1/10000 */
			if (rand() % 10000 == 0) {
				ack = 0;
				seq_num--;
			}

            *frame = *tloeframe;
            frame->mask = 0;                // To indicate ACK
			frame->ack = ack;
			frame->channel = channel;
			frame->credit = credit;
			frame->seq_num = seq_num;

            if (!enqueue(ack_buffer, (void *) frame)) {
                printf("File: %s line: %d: ack_buffer enqueue error\n", __FILE__, __LINE__);
                exit(1);
            }

			if (ack) {
				// Update sequence numbers
				next_rx_seq = (tloeframe->seq_num + 1) % (MAX_SEQ_NUM + 1);
				acked_seq = tloeframe->seq_num_ack;

				tl->opcode = tloeframe->opcode;
				if (!enqueue(tl_message_buffer, (void *) tl)) {
					//printf("File: %s line: %d: tl_message_buffer enqueue error\n", __FILE__, __LINE__);
					//exit(1);
					// (DEBUG) DROP TL message
					tlmsg_drop_cnt++;
				}
			}

			// tloeframe must be freed here
			free(tloeframe);
		}
		//if (next_tx_seq % 100 == 0)
        //  fprintf(stderr, "next_tx: %d, ackd: %d, next_rx: %d, ack_cnt: %d\n", next_tx_seq, acked_seq, next_rx_seq, ack_cnt);
    } else if (diff_seq < (MAX_SEQ_NUM + 1) / 2) {
        // The received TLoE frame is a duplicate
        // The frame should be dropped, NEXT_RX_SEQ is not updated
        // A positive acknowledgment is sent using the received sequence number
        int seq_num = tloeframe->seq_num;
        printf("TLoE frame is a duplicate. ");
		printf("seq_num: %d, next_rx_seq: %d, mask: %d\n", 
			seq_num, next_rx_seq, tloeframe->mask);

        // If the received frame contains data, enqueue it in the message buffer

        if (is_ack_msg(tloeframe)) {
            printf("File: %s line: %d: duplication error\n", __FILE__, __LINE__);
            exit(1);
            free(tloeframe);
        }

        tloeframe->seq_num = seq_num;
        tloeframe->seq_num_ack = seq_num;
        tloeframe->ack = 1;
        tloeframe->mask = 0; // To indicate ACK

        if (!enqueue(ack_buffer, (void *) tloeframe)) {
            printf("File: %s line: %d: enqueue error\n", __FILE__, __LINE__);
            exit(1);
        }
    } else {

        // The received TLoE frame is out of sequence, indicating that some frames were lost
        // The frame should be dropped, NEXT_RX_SEQ is not updated
        // A negative acknowledgment (NACK) is sent using the last properly received sequence number
        int last_proper_rx_seq = (next_rx_seq - 1) % (MAX_SEQ_NUM+1);
        if (last_proper_rx_seq < 0)
            last_proper_rx_seq += MAX_SEQ_NUM + 1;

        printf("TLoE frame is out of sequence ");
		printf("with seq_num: %d, next_rx_seq: %d, last: %d\n", 
			tloeframe->seq_num, next_rx_seq, last_proper_rx_seq);

        // If the received frame contains data, enqueue it in the message buffer
        if (!is_ack_msg(tloeframe)) {
            tloeframe->seq_num = last_proper_rx_seq;
            tloeframe->ack = 0;  // NACK
            tloeframe->mask = 0; // To indicate ACK

            if (!enqueue(ack_buffer, (void *) tloeframe)) {
                printf("File: %s line: %d: enqueue error\n", __FILE__, __LINE__);
                exit(1);
            }
        } else {
            free(tloeframe);
        }
    }
}

