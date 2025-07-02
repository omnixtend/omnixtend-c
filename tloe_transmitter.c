#include <stdio.h>
#include <stddef.h>
#include <unistd.h>

#include "tloe_common.h"
#include "tloe_transmitter.h"
#include "tloe_endpoint.h"
#include "tloe_frame.h"
#include "tloe_seq_mgr.h"
#include "retransmission.h"
#include "timeout.h"
#include "tloe_nsm.h"

static void send_ackonly_frame(tloe_endpoint_t *e) {
	char send_buffer[MAX_BUFFER_SIZE];
    tloe_frame_t f;
    int fsize = tloe_get_fsize(NULL);

    f.header.type = TYPE_ACKONLY;
    // Update sequence number (from e)
    tloe_seqnum_set_next_and_acked_seq(&f, e);
    // Set the ack to TLOE_ACK
    f.header.ack = TLOE_ACK;
    tloe_set_mask(&f, 0, fsize);
    // Set 0 to channel and credit
    f.header.chan = 0;
    f.header.credit = 0;
    // Convert tloe_frame into packet
    tloe_frame_to_packet((tloe_frame_t *)&f, send_buffer, fsize);
    // Send the request_normal_frame using the ether
    tloe_fabric_send(e, send_buffer, fsize);
}

static int enqueue_retransmit_buffer(tloe_endpoint_t *e, RetransmitBufferElement *rbe, tloe_frame_t *f, int f_size) {
    // Set the tloe frame
    memcpy(&rbe->tloe_frame, f, f_size);
    // Set the state to TLOE_INIT
    rbe->state = TLOE_INIT;
    rbe->f_size = f_size;
    // Get the current time
    rbe->send_time = get_current_timestamp(&(e->iteration_ts));

    return enqueue(e->retransmit_buffer, rbe);
}

static void prepare_normal_frame(tloe_endpoint_t *e, tloe_frame_t *f, tl_msg_t *tlmsg, int f_size) {
    // Update sequence number (from e)
    tloe_seqnum_set_next_and_acked_seq(f, e);
    // Set the ack to TLOE_ACK
    f->header.ack = TLOE_ACK;
    if (tlmsg) {
        // Set the tilelink msg
        tloe_set_tlmsg(f, tlmsg);
        // Set the mask to indicate normal packet
        tloe_set_mask(f, 1, f_size);
    } else {
        tloe_set_mask(f, 0, f_size);
    }
    // Set 0 to channel and credit
    f->header.chan = 0;
    f->header.credit = 0;
}

static void send_request_normal_frame(tloe_endpoint_t *e, tloe_frame_t *f, int f_size) {
    char send_buffer[MAX_BUFFER_SIZE];

    // Convert tloe_frame into packet
    tloe_frame_to_packet((tloe_frame_t *)f, send_buffer, f_size);
    // Send the request_normal_frame using the ether
    tloe_fabric_send(e, send_buffer, f_size);
}

static void debug_print_send_frame(tloe_frame_t *f) {
#ifdef DEBUG
    DEBUG_ON("*TX Send normal frame: seq_num:%d, seq_num_ack:%d, chan:%d\n", f->header.seq_num, f->header.seq_num_ack, f->header.chan);
#endif
}

tl_msg_t *TX(tloe_endpoint_t *e, tl_msg_t *request_normal_tlmsg) {
    tl_msg_t *return_tlmsg = NULL;
    RetransmitBufferElement *rbe;
    char send_buffer[MAX_BUFFER_SIZE];
    int tloeframe_size = 0;
    int enqueued;
    int target_credit_chan = select_max_credit_channel(&(e->fc));

    // Send an ACKONLY frame (set during zero-tlmsg processing) 
    if (e->should_send_ackonly_frame) {
        send_ackonly_frame(e);
        e->should_send_ackonly_frame = false;
    }

    // Send ackonly frame if the retransmit buffer is full
    // Note that the ackonly frame is sent only once when half of the timeout has occurred
    // This is to avoid the case that the ackonly frame is sent multiple times
    // Multiple ackonly frames make high traffic on the fabric and 
    if (is_queue_full(e->retransmit_buffer) && e->ackonly_frame_sent == false) {
        RetransmitBufferElement *rbe = getfront(e->retransmit_buffer);
        if (is_timeout_tx_half(&(e->iteration_ts), rbe->send_time)) {
            e->ackonly_frame_sent = true;
            // Send the ackonly frame
            send_ackonly_frame(e);
            e->ackonly_cnt++;
        }
    }

    // If the retransmit buffer is full, delay execution until the next cycle
    if (is_queue_full(e->retransmit_buffer)) {
        return_tlmsg = request_normal_tlmsg;
        goto out;
    }

    if (request_normal_tlmsg == NULL && is_queue_empty(e->ack_buffer) && target_credit_chan == 0)
        goto out;

    // Decrease credit based on the tilelink message
    if (request_normal_tlmsg) {
        // Send tlmsg if credit is available 
        if (fc_credit_dec(&(e->fc), request_normal_tlmsg) != -1) {
        // If credit is not available but ack_buffer contains data, send credit using a zero-tlmsg
        } else if (!is_queue_empty(e->ack_buffer)) {
            return_tlmsg = request_normal_tlmsg;
            request_normal_tlmsg = NULL;
        } else {
            return_tlmsg = request_normal_tlmsg;
            goto out;
        }
    }

    // Get packet_size
    tloeframe_size = tloe_get_fsize(request_normal_tlmsg);
    BUG_ON(tloeframe_size == -1, "Frame size error");

    // Create normal frame
    tloe_frame_t *f = (tloe_frame_t *)malloc(tloeframe_size);
    memset((void *)f, 0, tloeframe_size); 
    prepare_normal_frame(e, f, request_normal_tlmsg, tloeframe_size);

    if (!is_queue_empty(e->ack_buffer)) {
        tloe_frame_t *ack_frame;

        ack_frame = (tloe_frame_t *)dequeue(e->ack_buffer);
        tloe_seqnum_set_frame_seq_num_ack(f, ack_frame->header.seq_num_ack);

        if (handle_ack_packet_drop(e, ack_frame)) {
            free(ack_frame);
            goto out;
        }

        free(ack_frame);
    }

    // Handling Credit
    // if tx_flow_credits[channel] 2^, Send credit frame
    // NOTE: target_credit_chan != 0 means there is at least one credit to be sent for the corresponding channel
    if (target_credit_chan != 0) {
        // Set credit info in frame
        f->header.chan = target_credit_chan;
        f->header.credit = get_outgoing_credits(&(e->fc), target_credit_chan);
    }

    // Enqueue to retransmitBuffer
    rbe = (RetransmitBufferElement *)malloc(sizeof(RetransmitBufferElement));
    if (!rbe) {
        fprintf(stderr, "%s[%d] failed to allocate memory for retransmit buffer element.\n"
                "the requested frame must be retransmitted for the next TX",
                __FILE__, __LINE__);
        return_tlmsg = request_normal_tlmsg;
        goto out;
    } 
    memset((void *)rbe, 0, sizeof(RetransmitBufferElement));

    // Enqueue to retransmit buffer
    //enqueued = enqueue_retransmit_buffer(e, rbe, f, tloeframe_size);
    //BUG_ON(!enqueued, "failed to enqueue retransmit buffer element.");

    // Send normal frame
    // Send the request_normal_tlmsg
    debug_print_send_frame(f);
    send_request_normal_frame(e, f, tloeframe_size);
    // Add small delay after sending frame
    usleep(100000);  // 1ms delay
    // Set the state to TLOE_SENT
    rbe->state = TLOE_SENT;

    // Increase the sequence number of the endpoint
    tloe_seqnum_next_tx_seq_inc(e);

out:
    // Retransmit all the elements in the retransmit buffer if the timeout has occurred
    if ((rbe = getfront(e->retransmit_buffer)) && is_timeout_tx(&(e->iteration_ts), rbe->send_time)) {
        fprintf(stderr, "TX: Timeout TX and retranmission from seq_num: %d\n", rbe->tloe_frame.header.seq_num);
        retransmit(e, rbe->tloe_frame.header.seq_num);
    }
    return return_tlmsg;
}
