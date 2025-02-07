#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include "circular_queue.h"

void init_queue(CircularQueue *queue, int size) {
    queue->front = 0;
    queue->rear = 0;
    queue->size = size;

    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

CircularQueue *create_queue(int size) {
    CircularQueue *cq = (CircularQueue *) malloc (sizeof(CircularQueue) + size*sizeof(void *));
    init_queue(cq, size);

    return cq;
}

void delete_queue(CircularQueue *q) {
    int i;
    void *e;

    if (!q) 
        return;

    for (i=q->front; i != q->rear; i = (i+1) % q->size) {
        e = q->data[i];
        if (e) free(e);
    }

    free(q);
}

bool is_queue_empty(CircularQueue *queue) {
    return queue->front == queue->rear;
}

bool is_queue_full(CircularQueue *queue) {
    return (queue->rear + 1) % queue->size == queue->front;
}

#include "../tloe_frame.h"

bool enqueue(CircularQueue *queue, void *data) {
    if (is_queue_full(queue)) {
        return false; // 큐가 가득 참
    }
{
	extern CircularQueue *message_buffer;

	if (queue == message_buffer) {
		TloeFrame *tf = (TloeFrame *)data;
		if(tf->mask == 0) {
			printf("ERROR\n");
			while(1)
				;
		}
	}
}

    queue->data[queue->rear] = data;
    queue->rear = (queue->rear + 1) % queue->size;
    return true;
}

// 큐에서 패킷 제거 (실제 제거는 아니고 front만 이동)
void* dequeue(CircularQueue *queue) {
    void *data = NULL;

    if (!is_queue_empty(queue)) {
        data = queue->data[queue->front];
        queue->front = (queue->front + 1) % queue->size;
    }

    return data;
}

void* getfront(CircularQueue *q) {
    return is_queue_empty(q) ? NULL : q->data[q->front];
}

void printqueue(CircularQueue *queue) {
	int i;
       
	printf("front: %d, rear: %d\n", queue->front, queue->rear);
	for (i=queue->front; i != queue->rear; i = (i+1) % queue->size) {
		printf("%p ", queue->data[i]);
	}
	printf("\n");
}

CircularQueueIter *queue_iter(CircularQueue *q) {
    CircularQueueIter *cqi = (CircularQueueIter *) malloc(sizeof(CircularQueueIter));
    if (q != NULL && cqi != NULL) {
        cqi->q = q;
        cqi->next = q->front;
    }
    return cqi;
}


