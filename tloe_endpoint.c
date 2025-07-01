#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include "tloe_endpoint.h"
#include "tloe_ether.h"
#include "tloe_frame.h"
#include "tloe_transmitter.h"
#include "tloe_receiver.h"
#include "tloe_connection.h"
#include "tilelink_msg.h"
#include "tilelink_handler.h"
#include "retransmission.h"
#include "timeout.h"
#include "util/circular_queue.h"
#include "util/util.h"

#define CHECK_FABRIC_TYPE_CONFLICT(current_type, expected_type, msg) do { \
    if (current_type != -1 && current_type != expected_type) { \
        fprintf(stdout, "Error: %s\n", msg); \
        print_usage(); \
        ret = -1; \
        goto out; \
    } \
} while(0)

static void tloe_endpoint_init(tloe_endpoint_t *e, int fabric_type, int master_slave) {
	e->is_done = 0;
	e->connection = 0;
	e->master = master_slave;
    e->fabric_type = fabric_type;

	e->next_tx_seq = 0;
	e->next_rx_seq = 0;
	e->acked_seq = MAX_SEQ_NUM;

    if (e->retransmit_buffer != NULL) delete_queue(e->retransmit_buffer);
	if (e->message_buffer != NULL) delete_queue(e->message_buffer);
	if (e->ack_buffer != NULL) delete_queue(e->ack_buffer);
	if (e->tl_msg_buffer != NULL) delete_queue(e->tl_msg_buffer);
	if (e->response_buffer != NULL) delete_queue(e->response_buffer);

    e->retransmit_buffer = create_queue(WINDOW_SIZE + 1);
	e->message_buffer = create_queue(10000);
	e->ack_buffer = create_queue(2048);
	e->tl_msg_buffer = create_queue(10000);
	e->response_buffer = create_queue(100);

    e->should_send_ackonly_frame = false;
    e->ackonly_frame_sent = false;

	init_flowcontrol(&(e->fc));

	e->ack_cnt = 0;
	e->dup_cnt = 0;
	e->oos_cnt = 0;
	e->drop_cnt = 0;

    e->accessack_cnt = 0;
    e->accessackdata_cnt = 0;

    e->ackonly_cnt = 0;

	e->drop_tlmsg_cnt = 0;
	e->drop_response_cnt = 0;

	e->drop_npacket_size = 0;
	e->drop_npacket_cnt = 0;
}

static void tloe_endpoint_close(tloe_endpoint_t *e) {
    // Join threads
    pthread_join(e->tloe_endpoint_thread, NULL);

    // Cleanup
    tloe_fabric_close(e);

    // Cleanup
    tl_handler_close();

    // Cleanup queues
    delete_queue(e->message_buffer);
    delete_queue(e->retransmit_buffer);
    delete_queue(e->ack_buffer);
	delete_queue(e->tl_msg_buffer);
	delete_queue(e->response_buffer);
}

// Select data to process from buffers based on priority order
// response_buffer -> message_buffer, however, we need to consider
// the starvation issue on message_buffer
static int buffer_select_counter = 4;
static tl_msg_t *select_buffer(tloe_endpoint_t *e) {
	tl_msg_t *tlmsg;

	if (--buffer_select_counter > 0) {
		tlmsg = (tl_msg_t *) dequeue(e->response_buffer);
		if (tlmsg) {
			goto out;
		}
	}

	tlmsg = (tl_msg_t *) dequeue(e->message_buffer);
	buffer_select_counter = 4;
out:
	return tlmsg;
}

void *tloe_endpoint(void *arg) {
	int chan, credit;
	tloe_endpoint_t *e = (tloe_endpoint_t *)arg;

	tl_msg_t *request_tlmsg = NULL;
	tl_msg_t *not_transmitted_tlmsg = NULL;

	while(!e->is_done) {
        while(!e->connection) continue;

		if (!request_tlmsg)
			request_tlmsg = select_buffer(e);

		// Get reference timestemp for the single iteration
		update_iteration_timestamp(&(e->iteration_ts));

		not_transmitted_tlmsg = TX(e, request_tlmsg);
		if (not_transmitted_tlmsg) {
			request_tlmsg = not_transmitted_tlmsg;
			not_transmitted_tlmsg = NULL;
		} else if (!not_transmitted_tlmsg  && request_tlmsg) { 
			free(request_tlmsg);
			request_tlmsg = NULL;
		}

		RX(e);

		if (tl_handler(e, &chan, &credit) == 0)
			continue;
		add_channel_flow_credits(&(e->fc), chan, credit);
	}
}

static void print_endpoint_status(tloe_endpoint_t *e) {
    printf("-----------------------------------------------------\n"
            "Sequence Numbers:\n"
            " TX: %d(0x%x), RX: %d(0x%x)\n"
            "\nPacket Statistics:\n"
            " ACK: %d, Duplicate: %d, Out-of-Sequence: %d\n"
            " Dropped: %d (Normal: %d)\n"
            " ACKONLY: %d\n"
            "Channel Credits [0|A|B|C|D|E]: %d|%d|%d|%d|%d|%d\n"
            " Flow Control\n"
            " [A][%d][%d][%d]    [D][%d][%d][%d]\n"
            "\nBuffer Drops:\n"
            " TL Messages: %d, Responses: %d\n"
            "\nResponse count:\n"
            " ACCESSACK: %d, ACCESSACK_DATA: %d\n"
            "-----------------------------------------------------\n",
            e->next_tx_seq, e->next_tx_seq, e->next_rx_seq, e->next_rx_seq,
            e->ack_cnt, e->dup_cnt, e->oos_cnt,
            e->drop_cnt, e->drop_npacket_cnt, 
            e->ackonly_cnt,
            e->fc.credits[0], e->fc.credits[CHANNEL_A], e->fc.credits[CHANNEL_B], 
            e->fc.credits[CHANNEL_C], e->fc.credits[CHANNEL_D], 
            e->fc.credits[CHANNEL_E],
            e->fc.inc_cnt[CHANNEL_A], e->fc.dec_cnt[CHANNEL_A], e->fc.inc_value[CHANNEL_A],
            e->fc.inc_cnt[CHANNEL_D], e->fc.dec_cnt[CHANNEL_D], e->fc.inc_value[CHANNEL_D],
            e->drop_tlmsg_cnt, e->drop_response_cnt,
            e->accessack_cnt, e->accessackdata_cnt); 
}

static int create_and_enqueue_message(tloe_endpoint_t *e, uint64_t msg_index) {
    tl_msg_t *new_tlmsg = (tl_msg_t *)malloc(sizeof(tl_msg_t));
    if (!new_tlmsg) {
        printf("Memory allocation failed at packet %ld!\n", msg_index);
        return 0;
    }
    memset((void *) new_tlmsg, 0, sizeof(tl_msg_t));

    new_tlmsg->header.chan = CHANNEL_A;
    new_tlmsg->header.opcode = A_GET_OPCODE;
    // TODO new_tlmsg->num_flit = 4;

    while(is_queue_full(e->message_buffer)) 
        usleep(1000);

    if (enqueue(e->message_buffer, new_tlmsg)) {
        if (msg_index % 100 == 0)
            fprintf(stderr, "Packet %ld added to message_buffer\n", msg_index);
        return 1;
    } else {
        free(new_tlmsg);
        return 0;
    }
}

static int read_memory(tloe_endpoint_t *e, uint64_t addr) {
    tl_msg_t *new_tlmsg = (tl_msg_t *)malloc(sizeof(tl_msg_t) + sizeof(uint64_t) * 1);
    memset(new_tlmsg, 0, sizeof(tl_msg_t) + sizeof(uint64_t) * 1);

    new_tlmsg->header.chan = CHANNEL_A;
    new_tlmsg->header.opcode = A_GET_OPCODE;
    new_tlmsg->data[0] = addr;
    new_tlmsg->header.size = 3;     //TODO 8byte for test

    while(is_queue_full(e->message_buffer)) 
        usleep(1000);

    if (enqueue(e->message_buffer, new_tlmsg)) {
        return 1;
    } else {
        free(new_tlmsg);
        return 0;
    }
}

static int write_memory(tloe_endpoint_t *e, uint64_t addr, uint64_t value) {
    tl_msg_t *new_tlmsg = (tl_msg_t *)malloc(sizeof(tl_msg_t) + sizeof(uint64_t) * 2);
    memset(new_tlmsg, 0, sizeof(tl_msg_t) + sizeof(uint64_t) * 2);

    new_tlmsg->header.chan = CHANNEL_A;
    new_tlmsg->header.opcode = A_PUTFULLDATA_OPCODE;
    new_tlmsg->header.size = 3;     //TODO 8byte for test
    new_tlmsg->data[0] = addr;
    new_tlmsg->data[1] = value;

    while(is_queue_full(e->message_buffer)) 
        usleep(1000);

    if (enqueue(e->message_buffer, new_tlmsg)) {
        return 1;
    } else {
        free(new_tlmsg);
        return 0;
    }
}

static void print_credit_status(tloe_endpoint_t *e) {
    printf("Open connection is done. Credit %d | %d | %d | %d | %d\n",
        get_credit(&(e->fc), CHANNEL_A), get_credit(&(e->fc), CHANNEL_B),
        get_credit(&(e->fc), CHANNEL_C), get_credit(&(e->fc), CHANNEL_D),
        get_credit(&(e->fc), CHANNEL_E));
}

static int handle_user_input(tloe_endpoint_t *e, char input, uint64_t args1, 
				int args2, int args3, int fabric_type, int master) {
    int ret = 0;

    if (input == 's') {
        print_endpoint_status(e);
    } else if (input == 'a') {
        if (!is_conn(e)) return 0;
        for (uint64_t i = 0; i < args1; i++) {
            if (!create_and_enqueue_message(e, i)) {
                break;  // Stop if buffer is full or allocation fails
            }
        }
    } else if (input == 'c') {
        int is_conn = 0;
        if (e->master == TYPE_MASTER) {
            is_conn = open_conn_master(e);
        } else if (e->master == TYPE_SLAVE) {
            is_conn = open_conn_slave(e);
        }

        if (is_conn) {
            printf("Open connection complete.\n");
            e->connection = 1;
        }
    } else if (input == 'd') {
        int is_conn = 0;

        e->connection = 0;
        if (e->master == TYPE_MASTER)
            is_conn = close_conn_master(e);
        else if (e->master == TYPE_SLAVE)
            is_conn = close_conn_slave(e);

        if (is_conn) {
            printf("Close connection complete.\n");
            tl_handler_init();
            tloe_endpoint_init(e, fabric_type, master);
        }
    } else if (input == 'r') {
        if (!is_conn(e)) return 0;
        if (args1 == 0) return 0; 
        if (args2 == 0) args2 = 1;
        for (int i = 0; i < args2; i++) {
            read_memory(e, (uint64_t) args1);
        }
    } else if (input == 'w') {
        if (!is_conn(e)) return 0;
        if (args1 == 0) return 0; 
        if (args2 == 0) return 0;
        if (args3 == 0) args3 = 1;

        for (int i = 0; i< args3; i++) {
            write_memory(e, (uint64_t) args1, (uint64_t) args2);
        }
    } else if (input == 't') {
        if (!is_conn(e)) return 0;
        for (int i = 0; i < args1; i++) {
            uint64_t addr = 0x1000 + 0x1000*i;
            uint64_t value = 0x1000 + i;
            write_memory(e, addr, value);
            read_memory(e, addr);
        }
    } else if (input == 'q') {
        e->is_done = 1;
        printf("Exiting...\n");
        ret = 1;
        goto out;
    } else if (input == 'e') {
        printf("Starting Arty-demo...\n");
        e->connection = 1;
    } else {
        if (!is_conn(e)) return 0;

    }
out:
    return ret;
}

static void print_usage(void) {
    fprintf(stdout, "Usage: tloe_endpoint {-i<ethernet interface> -d<destination mac address> | -p<mq name>} {-m | -s}\n");
}

static int parse_arguments(int argc, char *argv[], int *fabric_type, int *master, 
                         char *optarg_a, size_t optarg_a_size,
                         char *optarg_b, size_t optarg_b_size) {
    int opt;
    int ret = 0;
    *fabric_type = -1;
    *master = -1;

    while ((opt = getopt(argc, argv, "i:d:p:ms")) != -1) {
        switch (opt) {
            case 'i':
                CHECK_FABRIC_TYPE_CONFLICT(*fabric_type, TLOE_FABRIC_ETHER, 
                    "Cannot mix ethernet and mq mode options");
                *fabric_type = TLOE_FABRIC_ETHER;
                strncpy(optarg_a, optarg, optarg_a_size - 1);
                optarg_a[optarg_a_size - 1] = '\0';
                break;
            case 'd':
                CHECK_FABRIC_TYPE_CONFLICT(*fabric_type, TLOE_FABRIC_ETHER,
                    "Cannot mix ethernet and mq mode options");
                *fabric_type = TLOE_FABRIC_ETHER;
                strncpy(optarg_b, optarg, optarg_b_size - 1);
                optarg_b[optarg_b_size - 1] = '\0';
                break;
            case 'p':
                CHECK_FABRIC_TYPE_CONFLICT(*fabric_type, TLOE_FABRIC_MQ,
                    "Cannot mix ethernet and mq mode options");
                *fabric_type = TLOE_FABRIC_MQ;
                strncpy(optarg_a, optarg, optarg_a_size - 1);
                optarg_a[optarg_a_size - 1] = '\0';
                break;
            case 'm':
                *master = 1;
                break;
            case 's':
                *master = 0;
                break;
            default:
                print_usage();
                ret = -1;
                goto out;
        }
    }

    // fabric type must be specified
    if (*fabric_type == -1) {
        fprintf(stdout, "Error: Must specify either ethernet mode (-i, -d) or mq mode (-p)\n");
        print_usage();
        ret = -1;
        goto out;
    }

    // ethernet mode requires both interface and destination MAC	
    if (*fabric_type == TLOE_FABRIC_ETHER && (optarg_a[0] == '\0' || optarg_b[0] == '\0')) {
        fprintf(stdout, "Error: Ethernet mode requires both interface (-i) and destination MAC (-d)\n");
        print_usage();
        ret = -1;
        goto out;
    }

    // master or slave mode must be specified
    if (*master == -1) {
        fprintf(stdout, "Error: Must specify either master (-m) or slave (-s) mode\n");
        print_usage();
        ret = -1;
        goto out;
    }

    if (*fabric_type == TLOE_FABRIC_MQ) {
        strncpy(optarg_b, *master ? "-master" : "-slave", sizeof("-master"));
    }

out:
    return ret;
}

int main(int argc, char *argv[]) {
    tloe_endpoint_t *e;
    TloeEther *ether;
    char input, input_count[32];
    int master_slave;
    uint64_t args1 = 0;
    int args2 = 0, args3 = 0;
    char dev_name[64] = {0};
    char dest_mac_addr[64] = {0};
    int fabric_type;

    if (parse_arguments(argc, argv, &fabric_type, &master_slave, 
                       dev_name, sizeof(dev_name),
                       dest_mac_addr, sizeof(dest_mac_addr)) < 0) {
        exit(EXIT_FAILURE);
    }

#if defined(DEBUG) || defined(TEST_NORMAL_FRAME_DROP) || defined(TEST_TIMEOUT_DROP)
    srand(time(NULL));
#endif

    // Initialization independent of tloe_endpoint
    tl_handler_init();

    // Initialize tloe_endpoint
    e = (tloe_endpoint_t *)malloc(sizeof(tloe_endpoint_t));
    memset((void *)e, 0, sizeof(tloe_endpoint_t));
    tloe_endpoint_init(e, fabric_type, master_slave);
    tloe_fabric_init(e, fabric_type);

    // intead of a direct call to tloe_ether_open
    tloe_fabric_open(e, dev_name, dest_mac_addr, 0);

    if (pthread_create(&(e->tloe_endpoint_thread), NULL, tloe_endpoint, e) != 0) {
        error_exit("Failed to create tloe endpoint thread");
    }

    while(!(e->is_done)) {
        printf("Enter 'e' to Arty-demo, 'c' to open, 'd' to close, 's' to status, 'a' to send, 'q' to quit:\n");
        printf("> ");
        fgets(input_count, sizeof(input_count), stdin);

        if (sscanf(input_count, " %c %lx %x %x", &input, &args1, &args2, &args3) < 1) {
            printf("Invalid input! Try again.\n");
            continue;
        }

        if (handle_user_input(e, input, args1, args2, args3, fabric_type, master_slave))
            break;

        args1 = args2 = args3 = 0;
    }

    tloe_endpoint_close(e);

    return 0;
}
