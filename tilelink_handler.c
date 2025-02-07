#include "tilelink_handler.h"
#include "tloe_frame.h"
#include <stdlib.h>
#include <string.h>

//extern CircularQueue *message_buffer;
extern CircularQueue *reply_buffer;
extern CircularQueue *tl_message_buffer;
int reply_drop_cnt = 0;

void tl_handler() {
	// dequeue from th_buffer
	TileLinkMsg *tlmsg = (TileLinkMsg *) malloc (sizeof(TileLinkMsg));

	if(is_queue_empty(tl_message_buffer))
		goto out;

	tlmsg = (TileLinkMsg *) dequeue(tl_message_buffer); 

	// handle & make reponse
	TloeFrame *tloeframe = malloc(sizeof(TloeFrame));
	memset(tloeframe, 0, sizeof(TloeFrame));

	switch (tlmsg->opcode) {
	case A_GET_OPCODE:
		tloeframe->mask = 1;
		tloeframe->channel = CHANNEL_A;
		tloeframe->credit = 2;
		tloeframe->opcode = C_ACCESSACKDATA_OPCODE;

		if (!enqueue(reply_buffer, tloeframe)) {
			//printf("Failed to enqueue packet %d, buffer is full.\n", tloeframe->seq_num);
			reply_drop_cnt++;
			free(tloeframe);
			goto out;
		}
		break;
	case C_ACCESSACKDATA_OPCODE:
		break;
	default:	
		//DEBUG
		printf("Unknown opcode. %d\n", tlmsg->opcode);
		exit(1);
	}

out:
	free(tlmsg);
}
#if 0
void tl_handler(TileLinkMsg *tl, int *channel, int *credit) {
	// Handling TileLink Msg

	// for testing
	*channel = 1;
	*credit = 2;

	// Send response
	TloeFrame *tloeframe = malloc(sizeof(TloeFrame));
	TileLinkMsg *tlmsg = malloc(sizeof(TileLinkMsg));
	memset(tloeframe, 0, sizeof(TloeFrame));

	if (tl->opcode == A_GET_OPCODE) {
		tloeframe->mask = 1;
		tloeframe->channel = CHANNEL_A;
		tloeframe->credit = 2;
		tloeframe->opcode = C_ACCESSACKDATA_OPCODE;

#if 1
		if (!enqueue(reply_buffer, tloeframe)) {
			printf("Failed to enqueue packet %d, buffer is full.\n", tloeframe->seq_num);
			goto out;
		}
#else
		free(tloeframe);
		goto out;
#endif

	} else if (tl->opcode == A_PUTFULLDATA_OPCODE) {
		exit(1);
		free(tlmsg);
		return;
	} else if (tl->opcode == C_ACCESSACKDATA_OPCODE) {
		exit(1);
		free(tlmsg);
		return;
	}
out:
	free(tlmsg);
}
#endif
