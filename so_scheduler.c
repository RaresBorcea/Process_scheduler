/*
 * Scheduler Implementation
 *
 * 2020, Rares Borcea, 334CC
 */

#include <stdio.h>

#include "scheduler.h"
#include "utils.h"

#ifdef _WIN32
#define DLL_EXPORTS
#define MAX_SEM_COUNT 1
#define MIN_SEM_COUNT 0

static CRITICAL_SECTION CriticalSection;
#else
#define DWORD
#define BOOL int

typedef void *WINAPI;
typedef void *LPVOID;

static pthread_mutex_t lock;
#endif
static Scheduler scheduler;

/*
 * Thread function - run by each new thread after creation.
 * Creates a new thread structure, ads it to the all_threads
 * queue and to the corresponding Round-Robin queue based on
 * priority of the thread and announces the parent that its
 * child scheduling is finished.
 * If no thread is currently running, makes this thread the
 * running one.
 * After current thread is finished, finds one to replace it
 * with.
 */
static DWORD WINAPI start_thread(LPVOID start_params)
{
	BOOL bRet;
	So_thread *new_thread = NULL;
	So_start_params *params = (So_start_params *)start_params;
	So_thread *replacer = NULL;
	int i;
	unsigned int prio;
	so_handler *func = NULL;

	new_thread = malloc(sizeof(struct thread));
	DIE(new_thread == NULL, "START_THREAD: Malloc error");
	func = params->instr;
	prio = params->prio;
#ifdef _WIN32
	bRet = DuplicateHandle(
		GetCurrentProcess(),
		GetCurrentThread(),
		GetCurrentProcess(),
		&new_thread->id,
		0,
		FALSE,
		DUPLICATE_SAME_ACCESS);
	DIE(bRet == FALSE, "START_THREAD: Handle duplicate error");
	new_thread->sem = CreateSemaphore(
		NULL,
		MIN_SEM_COUNT,
		MAX_SEM_COUNT,
		NULL);
	DIE(new_thread->sem == NULL, "START_THREAD: Semaphore create error");
	new_thread->prio = params->prio;
	if (scheduler->first_thread) {
		scheduler->first_thread = 0;
		new_thread->quantum = scheduler->quantum + 1;
	} else {
		new_thread->quantum = scheduler->quantum;
	}
	new_thread->event = -1;

	EnterCriticalSection(&CriticalSection);
#else
	new_thread->id = pthread_self();
	bRet = sem_init(&(new_thread->sem), 0, 0);
	DIE(bRet == -1, "START_THREAD: Semaphore init error");
	new_thread->prio = params->prio;
	new_thread->quantum = scheduler->quantum;
	new_thread->event = -1;

	bRet = pthread_mutex_lock(&lock);
	DIE(bRet != 0, "START_THREAD: Mutex lock error");
#endif

	/* Add thread to queues */
	scheduler->threads = enqueue(scheduler->threads, new_thread);
	DIE(scheduler->threads == NULL, "START_THREAD: Malloc error");
	scheduler->robin[new_thread->prio] =
			enqueue(scheduler->robin[new_thread->prio], new_thread);
	DIE(scheduler->robin[new_thread->prio] ==
			NULL, "START_THREAD: Malloc error");

#ifdef _WIN32
	LeaveCriticalSection(&CriticalSection);
	/* Announce parent */
	bRet = ReleaseSemaphore(
		params->sem,
		1,
		NULL);
	DIE(bRet == FALSE, "START_THREAD: Semaphore release error");

	if (scheduler->current != NULL) {
		bRet = WaitForSingleObject(new_thread->sem, INFINITE);
		DIE(bRet == WAIT_FAILED, "START_THREAD: Semaphore wait error");
	}
#else
	bRet = pthread_mutex_unlock(&lock);
	DIE(bRet != 0, "START_THREAD: Mutex unlock error");
	bRet = sem_post(&(params->sem));
	DIE(bRet == -1, "START_THREAD: Semaphore post error");

	if (scheduler->current != NULL) {
		bRet = sem_wait(&(new_thread->sem));
		DIE(bRet == -1, "START_THREAD: Semaphore wait error");
	}
#endif

	/* Schedule current thread */
	if (scheduler->current == NULL) {
		scheduler->robin[new_thread->prio] =
			dequeue(scheduler->robin[new_thread->prio], FREE_NODE);
		scheduler->current = new_thread;
	}

	/* Run thread handler */
	func(prio);
	for (i = SO_MAX_PRIO; i != -1 && replacer == NULL; i--)
		if (!isEmptyQueue(scheduler->robin[i])) {
			replacer = first(scheduler->robin[i]);
			scheduler->robin[i] = dequeue(scheduler->robin[i],
				FREE_NODE);
		}
	scheduler->current = replacer;

#ifdef _WIN32
	/* Schedule replacer */
	if (replacer != NULL) {
		bRet = ReleaseSemaphore(
			scheduler->current->sem,
			1,
			NULL);
		DIE(bRet == FALSE, "START_THREAD: Semaphore release error");
	}

	bRet = CloseHandle(new_thread->sem);
	DIE(bRet == FALSE, "START_THREAD: Semaphore close error");

	return 0;
#else
	if (replacer != NULL) {
		bRet = sem_post(&(scheduler->current->sem));
		DIE(bRet == -1, "START_THREAD: Semaphore post error");
	}

	bRet = sem_destroy(&(new_thread->sem));
	DIE(bRet == -1, "START_THREAD: Semaphore destroy error");

	return NULL;
#endif
}

/*
 * Initializes the scheduler and its structure.
 * Receives a thread's time quantum and the maximum
 * number of supported IO devices.
 */
int so_init(unsigned int time_quantum, unsigned int io)
{
#ifdef _WIN32
	InitializeCriticalSection(&CriticalSection);
#else
	int ret;

	ret = pthread_mutex_init(&lock, NULL);
	DIE(ret != 0, "SO_INIT: Mutex init error");
#endif

	if (time_quantum <= 0 || io < 0 || io > SO_MAX_NUM_EVENTS)
		return -1;
	if (scheduler != NULL)
		return -1;

	scheduler = calloc(1, sizeof(struct sched));
	DIE(scheduler == NULL, "SO_INIT: Calloc error");
	scheduler->current = NULL;
	scheduler->quantum = time_quantum;
	scheduler->devices_no = io;
	scheduler->first_thread = 1;

	return 0;
}

/*
 * Starts a new thread.
 * Receives the handler the future thread will run and
 * its static priority.
 * Waits for the new thread to be scheduled.
 * Returns the Thread identifier of the child thread.
 */
tid_t so_fork(so_handler *func, unsigned int priority)
{
	So_start_params *params;
	tid_t id;
	BOOL bRet;
#ifdef _WIN32
	HANDLE hRet;
#endif

	if (priority < 0 || priority > SO_MAX_PRIO
		|| func == INVALID_TID)
		return INVALID_TID;

	params = malloc(sizeof(struct params));
	DIE(params == NULL, "SO_FORK: Malloc error");

#ifdef _WIN32
	params->sem = CreateSemaphore(
		NULL,
		MIN_SEM_COUNT,
		MAX_SEM_COUNT,
		NULL);
	DIE(params->sem == NULL, "SO_FORK: Semaphore create error");
	params->prio = priority;
	params->instr = func;

	/* Create child thread */
	hRet = CreateThread(
		NULL,
		0,
		start_thread,
		params,
		0,
		&id);
	DIE(hRet == NULL, "SO_FORK: Thread create error");
	/* Wait for it to be scheduled */
	bRet = WaitForSingleObject(params->sem, INFINITE);
	DIE(bRet == WAIT_FAILED, "SO_FORK: Semaphore wait error");
	so_exec();

	bRet = CloseHandle(params->sem);
	DIE(bRet == FALSE, "SO_FORK: Semaphore close error");
#else
	bRet = sem_init(&(params->sem), 0, 0);
	DIE(bRet == -1, "SO_FORK: Semaphore init error");
	params->prio = priority;
	params->instr = func;

	bRet = pthread_create(&id, NULL, &start_thread, params);
	DIE(bRet == -1, "SO_FORK: Thread create error");
	bRet = sem_wait(&(params->sem));
	DIE(bRet == -1, "SO_FORK: Semaphore wait error");
	so_exec();

	bRet = sem_destroy(&(params->sem));
	DIE(bRet == -1, "SO_FORK: Semaphore destroy error");
#endif
	free(params);

	return id;
}

/*
 * Current thread will block until a signal for
 * the @param io event is received.
 * Finds a replacement thread to start running.
 */
int so_wait(unsigned int io)
{
	So_thread *current;
	So_thread *replacer = NULL;
	int i;
	BOOL bRet;

	if (io < 0 || io >= scheduler->devices_no)
		return -1;

	current = scheduler->current;
	current->quantum = scheduler->quantum;
	current->event = io;

	/* Find a replacement thread */
	for (i = SO_MAX_PRIO; i != -1 && replacer == NULL; i--)
		if (!isEmptyQueue(scheduler->robin[i])) {
			replacer = first(scheduler->robin[i]);
			scheduler->robin[i] = dequeue(scheduler->robin[i],
				FREE_NODE);
		}
	scheduler->current = replacer;

#ifdef _WIN32
	/* Start the replacing thread */
	if (replacer != NULL) {
		bRet = ReleaseSemaphore(
			scheduler->current->sem,
			1,
			NULL);
		DIE(bRet == FALSE, "SO_WAIT: Semaphore release error");
	}
	/* Blocks itself */
	bRet = WaitForSingleObject(current->sem, INFINITE);
	DIE(bRet == WAIT_FAILED, "SO_WAIT: Semaphore wait error");
#else
	if (replacer != NULL) {
		bRet = sem_post(&(scheduler->current->sem));
		DIE(bRet == -1, "SO_WAIT: Semaphore post error");
	}
	bRet = sem_wait(&(current->sem));
	DIE(bRet == -1, "SO_WAIT: Semaphore wait error");
#endif

	return 0;
}

/*
 * Awakens one or more threads waiting for the
 * @param io event.
 * Returns the number of awaken threads.
 * Turns one of the awaken threads into running
 * state if it has higher priority than the
 * current running one.
 */
int so_signal(unsigned int io)
{
	int res = 0;
	Node *node = NULL;
	So_thread *replacer = NULL;
	So_thread *current;
	unsigned int i;
	BOOL bRet;

	if (io < 0 || io >= scheduler->devices_no)
		return -1;
	if (scheduler == NULL)
		return -1;

	scheduler->current->quantum--;
	if (scheduler->threads == NULL)
		return 0;

	/* Wake threads waiting for this io event */
	node = scheduler->threads->head;
	while (node != NULL) {
		if (node->data->event == io) {
			res++;
			node->data->event = -1;
			scheduler->robin[node->data->prio] =
				enqueue(scheduler->robin[node->data->prio],
					node->data);
			DIE(scheduler->robin[node->data->prio] ==
				NULL, "SO_SIGNAL: Malloc error");
		}
		node = node->next;
	}

	/* Check for a higher priority thread */
	for (i = SO_MAX_PRIO; i != -1 && i > scheduler->current->prio
		&& replacer == NULL; i--)
		if (!isEmptyQueue(scheduler->robin[i])) {
			replacer = first(scheduler->robin[i]);
			scheduler->robin[i] = dequeue(scheduler->robin[i],
				FREE_NODE);
		}

	/* Turn it into the running one */
	if (replacer != NULL) {
		current = scheduler->current;
		current->quantum = scheduler->quantum;
		scheduler->robin[current->prio] =
			enqueue(scheduler->robin[current->prio], current);
		DIE(scheduler->robin[current->prio] ==
			NULL, "SO_SIGNAL: Malloc error");
		scheduler->current = replacer;

#ifdef _WIN32
		bRet = ReleaseSemaphore(
			scheduler->current->sem,
			1,
			NULL);
		DIE(bRet == FALSE, "SO_SIGNAL: Semaphore release error");
		/* Blocks itself */
		bRet = WaitForSingleObject(current->sem, INFINITE);
		DIE(bRet == WAIT_FAILED, "SO_WAIT: Semaphore wait error");
#else
		bRet = sem_post(&(scheduler->current->sem));
		DIE(bRet == -1, "SO_SIGNAL: Semaphore post error");
		bRet = sem_wait(&(current->sem));
		DIE(bRet == -1, "SO_SIGNAL: Semaphore wait error");
#endif
	}

	return res;
}

/*
 * Checks if a thread with higher priority is waiting
 * to start its execution and replaces the current one
 * with it.
 * Also gives turn to the following highest priority
 * thread when the quantum of the current one had
 * expired.
 */
void so_exec(void)
{
	So_thread *replacer = NULL;
	So_thread *current;
	unsigned int i;
	unsigned int priority_to_find = -1;
	BOOL bRet;

	if (scheduler == NULL || scheduler->current == NULL)
		return;

	/* Find a higher priority thread for regular replacement */
	if (scheduler->current->quantum >= 2) {
		priority_to_find = scheduler->current->prio + 1;
		scheduler->current->quantum--;
	} else {
		/* Or one with at least same priority when quantum expired */
		priority_to_find = scheduler->current->prio;
	}

	for (i = SO_MAX_PRIO; i != -1 && i >= priority_to_find
		&& replacer == NULL; i--)
		if (!isEmptyQueue(scheduler->robin[i])) {
			replacer = first(scheduler->robin[i]);
			scheduler->robin[i] = dequeue(scheduler->robin[i],
				FREE_NODE);
		}

	/* Schedule replacement thread */
	if (replacer != NULL) {
		current = scheduler->current;
		current->quantum = scheduler->quantum;
		scheduler->robin[current->prio] =
			enqueue(scheduler->robin[current->prio], current);
		DIE(scheduler->robin[current->prio] == NULL,
			"SO_EXEC: Malloc error");
		scheduler->current = replacer;

#ifdef _WIN32
		bRet = ReleaseSemaphore(
			scheduler->current->sem,
			1,
			NULL);
		DIE(bRet == FALSE, "SO_EXEC: Semaphore release error");
		/* Blocks itself */
		bRet = WaitForSingleObject(current->sem, INFINITE);
		DIE(bRet == WAIT_FAILED,
			"SO_EXEC: Semaphore wait error");
#else
		bRet = sem_post(&(scheduler->current->sem));
		DIE(bRet == -1, "SO_EXEC: Semaphore post error");
		bRet = sem_wait(&(current->sem));
		DIE(bRet == -1, "SO_EXEC: Semaphore wait error");
#endif
	}
}

/*
 * Waits for any running thread to finish its
 * activity then deallocates scheduler resources.
 */
void so_end(void)
{
	Node *node = NULL;
	int i;
	BOOL bRet;

	if (scheduler == NULL)
		return;

	if (scheduler->threads != NULL) {
#ifdef _WIN32
		/* Wait for threads */
		node = scheduler->threads->head;
		while (node != NULL) {
			bRet = WaitForSingleObject(node->data->id, INFINITE);
			DIE(bRet == WAIT_FAILED, "SO_END: Thread wait error");
			node = node->next;
		}
		node = scheduler->threads->head;
		while (node != NULL) {
			bRet = CloseHandle(node->data->id);
			DIE(bRet == FALSE, "SO_END: Thread handle close error");
			node = node->next;
		}
#else
		node = scheduler->threads->head;
		while (node != NULL) {
			bRet = pthread_join(node->data->id, NULL);
			DIE(bRet != 0, "SO_END: Thread join error");
			node = node->next;
		}
#endif
		/* Free allocated memory */
		freeQueue(scheduler->threads);
		for (i = 0; i < SO_MAX_PRIO; i++)
			if (scheduler->robin[i] != NULL)
				free(scheduler->robin[i]);
	}

	free(scheduler);
	scheduler = NULL;
#ifdef _WIN32
	DeleteCriticalSection(&CriticalSection);
#else
	bRet = pthread_mutex_destroy(&lock);
	DIE(bRet != 0, "SO_END: Mutex destroy error");
#endif
}
