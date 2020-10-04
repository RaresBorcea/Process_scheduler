#ifndef THREAD_H_
#define THREAD_H_

#ifdef __linux__
#include <semaphore.h>
#endif

#include "so_scheduler.h"

/** Define struct for thread */
typedef struct thread {
#ifdef _WIN32
	HANDLE id;
	HANDLE sem;
#else
	tid_t id;
	sem_t sem;
#endif
	unsigned int prio;
	unsigned int quantum;
	unsigned int event;
} So_thread;

/** Define struct for start_thread params */
typedef struct params {
#ifdef _WIN32
	HANDLE sem;
#else
	sem_t sem;
#endif
	unsigned int prio;
	so_handler *instr;
} So_start_params;

#endif /* THREAD_H_ */
