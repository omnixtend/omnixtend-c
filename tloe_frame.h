#ifndef __TLOE_FRAME_H__
#define __TLOE_FRAME_H__

#define TLOE_NACK       0
#define TLOE_ACK        1

#define NUM_CHANNEL     5
#define CHANNEL_0       0
#define CHANNEL_A       1
#define CHANNEL_B       2
#define CHANNEL_C       3
#define CHANNEL_D       4
#define CHANNEL_E       5

typedef struct tloe_frame_struct {
    // header
	int connection;
    int seq_num;
    int seq_num_ack;
    int ack; // ack = 1; nack = 0
	int opcode;
    // TL messages 
	
	// Mask	
	int mask;
	int channel;
	int credit;
} TloeFrame;

int tloe_set_seq_num(TloeFrame *frame, int seq_num);
int tloe_get_seq_num(TloeFrame *frame);
int tloe_set_seq_num_ack(TloeFrame *frame, int seq_num);
int tloe_get_seq_num_ack(TloeFrame *frame);
int tloe_set_ack(TloeFrame *frame, int ack);
int tloe_get_ack(TloeFrame *frame);
int tloe_set_mask(TloeFrame *frame, int mask);
int tloe_get_mask(TloeFrame *frame);
int is_ack_msg(TloeFrame *frame);
int is_connection(TloeFrame *frame);

#endif // __TLOE_FRAME_H__
