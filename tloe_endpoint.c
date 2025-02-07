#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "tloe_endpoint.h"
#include "tloe_ether.h"
#include "tloe_frame.h"
#include "tloe_transmitter.h"
#include "tloe_receiver.h"
#include "tilelink_msg.h"
#include "tilelink_handler.h"
#include "retransmission.h"
#include "flowcontrol.h"
#include "util/circular_queue.h"
#include "util/util.h"

CircularQueue *retransmit_buffer;
CircularQueue *rx_buffer;
CircularQueue *message_buffer;
CircularQueue *ack_buffer;
CircularQueue *tl_message_buffer;
CircularQueue *reply_buffer;

FlowControlCredit *fc_credit;

int next_tx_seq = 0;
int next_rx_seq = 0;
int acked_seq = MAX_SEQ_NUM;
int acked = 0;

static TloeEther *ether;
static int is_done = 1;

#if 0
time_t last_ack_time = 0;
#endif

TloeFrame *select_buffer() {
	TloeFrame *tloeframe = NULL;

#if 1
	tloeframe = (TloeFrame *) dequeue(reply_buffer);
	if (tloeframe)
		goto out;
#endif

	tloeframe = (TloeFrame *) dequeue(message_buffer);

out:
	return tloeframe;
}

void *tloe_endpoint(void *arg) {
	TloeFrame *request_tloeframe = NULL;
	TloeFrame *not_transmitted_frame = NULL;

	while(is_done) {
		RX(ether);

		if (!request_tloeframe) {
			request_tloeframe = select_buffer();
		}

		not_transmitted_frame = TX(request_tloeframe, ether);
		if (not_transmitted_frame) {
			request_tloeframe = not_transmitted_frame;
			not_transmitted_frame = NULL;
		} else if (!not_transmitted_frame  && request_tloeframe) { 
			free(request_tloeframe);
			request_tloeframe = NULL;
		}

		tl_handler();
	}
}

int main(int argc, char *argv[]) {
	pthread_t tloe_endpoint_thread;
	int master_slave = 1;
	char input[20];
	int iteration = 0;

    if (argc < 3) {
        printf("Usage: tloe_endpoint queue_name master[1]/slave[0]\n");
        exit(0);
    }


    if (argv[2][0] == '1')
        ether = tloe_ether_open(argv[1], 1);
    else
        ether = tloe_ether_open(argv[1], 0);

	// Initialize
    retransmit_buffer = create_queue(WINDOW_SIZE + 1);
    rx_buffer = create_queue(10); // credits
	message_buffer = create_queue(10000);
	ack_buffer = create_queue(100);
	tl_message_buffer = create_queue(100);
	reply_buffer = create_queue(100);
	fc_credit = create_credit();	

	srand(time(NULL));

	if (pthread_create(&tloe_endpoint_thread, NULL, tloe_endpoint, NULL) != 0) {
        error_exit("Failed to ack reply thread");
    }

	// Exchange credits
#if 0
	for (int i=0; i<NUM_CHANNEL; i++) {
		TloeFrame *new_tloe = (TloeFrame *)malloc(sizeof(TloeFrame));

		new_tloe->connection = 1;
		new_tloe->channel = i + 1;	
		new_tloe->credit = 10;
		new_tloe->mask = 0;  // Set mask (1 = normal packet)

		enqueue(message_buffer, new_tloe);
	}

	// Fill credits
	while(check_all_channels(fc_credit)) 
		;
#else
	for (int i=0; i<NUM_CHANNEL; i++) {
		set_credit(fc_credit, i+1, DEFAULT_CREDIT);
	}
#endif
    
	printf("Credit- A:%d, B:%d, C:%d, D:%d, E:%d\n", get_credit(fc_credit, CHANNEL_A), get_credit(fc_credit, CHANNEL_B), get_credit(fc_credit, CHANNEL_C), get_credit(fc_credit, CHANNEL_D), get_credit(fc_credit, CHANNEL_E));	

	while(1) {
		printf("Enter 's' to send a message, 'a <iteration>' to send multiple, 'q' to quit:\n");
		printf("> ");
		fgets(input, sizeof(input), stdin);

		char command;
		if (sscanf(input, " %c %d", &command, &iteration) < 1) {
			printf("Invalid input! Try again.\n");
			continue;
		}

		if (command == 's') {
			TloeFrame *new_tloe = (TloeFrame *)malloc(sizeof(TloeFrame));
			if (!new_tloe) {
				printf("Memory allocation failed!\n");
				continue;
			}
			new_tloe->mask = 1;

			if (enqueue(message_buffer, new_tloe)) {
				printf("Message added to message_buffer\n");
			} else {
				printf("Failed to enqueue message, buffer is full.\n");
				free(new_tloe);
			}
		} else if (command == 'a') {
			if (iteration <= 0) {
                printf("Invalid iteration count! Enter a positive number.\n");
                continue;
            }

            for (int i = 0; i < iteration; i++) {
				TloeFrame *new_tloe = (TloeFrame *)malloc(sizeof(TloeFrame));
				if (!new_tloe) {
					printf("Memory allocation failed at packet %d!\n", i);
					continue;
				}

				new_tloe->channel = CHANNEL_A;
				new_tloe->credit = 2;	// tmp
				new_tloe->mask = 1;		 // Set mask (1 = normal packet)
				new_tloe->opcode = A_GET_OPCODE;

				while(is_queue_full(message_buffer)) 
					usleep(1000);

				if (enqueue(message_buffer, new_tloe)) {
					if (i % 100 == 0)
						printf("Packet %d added to message_buffer\n", i);
				} else {
					//printf("Failed to enqueue packet %d, buffer is full.\n", i);
					free(new_tloe);
					break;  // Stop if buffer is full
				}
			}
		} else if (command == 'q') {
			printf("Exiting...\n");
			break;
		}
	}

	is_done = 1;

    // Join threads
    pthread_join(tloe_endpoint_thread, NULL);

    // Cleanup
    tloe_ether_close(ether);

    // Cleanup queues
    delete_queue(message_buffer);
    delete_queue(retransmit_buffer);
    delete_queue(rx_buffer);
    delete_queue(ack_buffer);
	delete_queue(tl_message_buffer);
    delete_queue(reply_buffer);

    return 0;
}

