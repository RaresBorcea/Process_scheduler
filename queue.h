#ifndef QUEUE_H_
#define QUEUE_H_

#define FREE_NODE			0
#define FREE_NODE_AND_DATA	1

#include <stdlib.h>
#include "thread.h"

/** Define the node of a queue */
typedef struct node {
	So_thread *data;
	struct node *next;
} Node;

/** Define the queue structure */
typedef struct queue {
	Node *head;
	Node *tail;
	int size;
} *Queue;

/** Value of first node in queue */
So_thread *first(Queue queue);

/** Add node in queue */
Queue enqueue(Queue queue, So_thread *data);

/** Delete first node of queue */
Queue dequeue(Queue queue, int type);

/** Check for an empty queue */
int isEmptyQueue(Queue queue);

/** Free memory occupied by queue */
Queue freeQueue(Queue queue);

#endif /* QUEUE_H_ */
