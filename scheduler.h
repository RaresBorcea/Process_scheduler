#ifndef SCHEDULER_H_
#define SCHEDULER_H_

#include "thread.h"
#include "queue.h"

/** Define struct for thread */
typedef struct sched {
	Queue robin[SO_MAX_PRIO + 1];
	Queue threads;
	So_thread *current;
	unsigned int quantum;
	unsigned int devices_no;
	unsigned int first_thread;
} *Scheduler;

#endif /* SCHEDULER_H_ */
