#include "queue.h"

/** Node initialization */
static Node *initNode(So_thread *data)
{
	Node *node;

	node = malloc(sizeof(struct node));
	if (node == NULL)
		return node;
	node->data = data;
	node->next = NULL;

	return node;
}

/** Queue initialization */
static Queue initQueue(So_thread *data)
{
	Queue queue;

	queue = malloc(sizeof(struct queue));
	if (queue == NULL)
		return queue;
	queue->head = queue->tail = initNode(data);
	queue->size = 1;

	return queue;
}

/** Free memory for a node */
static Node *freeNode(Node *node, int type)
{
	if (node) {
		if (type == FREE_NODE) {
			free(node);
		} else {
			free(node->data);
			free(node);
		}
	}
	return NULL;
}

So_thread *first(Queue queue)
{
	if (isEmptyQueue(queue))
		return NULL;
	else
		return queue->head->data;
}

Queue enqueue(Queue queue, So_thread *data)
{
	Node *node;

	if (isEmptyQueue(queue)) {
		if (queue == NULL)
			return initQueue(data);
		free(queue);
		return initQueue(data);
	}
	node = initNode(data);
	if (node == NULL)
		return NULL;
	queue->tail->next = node;
	queue->tail = node;
	queue->size++;

	return queue;
}

Queue dequeue(Queue queue, int type)
{
	Node *temp;

	if (!isEmptyQueue(queue)) {
		temp = queue->head;
		queue->head = queue->head->next;
		queue->size--;
		freeNode(temp, type);
	}
	if (queue->size == 0) {
		free(queue);
		queue = NULL;
	}

	return queue;
}

int isEmptyQueue(Queue queue)
{
	if (queue == NULL || queue->head == NULL || queue->size == 0)
		return 1;
	return 0;
}

Queue freeQueue(Queue queue)
{
	while (!isEmptyQueue(queue))
		queue = dequeue(queue, FREE_NODE_AND_DATA);
	if (queue)
		free(queue);

	return NULL;
}
