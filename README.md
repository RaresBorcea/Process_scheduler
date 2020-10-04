# Process_scheduler
Shared-library which recreates a process scheduler based on Round-robin with priorities principles.

Using the same .c files, the makefiles create platform specific shared libraries (based on #ifdef macros).

The library creates a scheduler which controls threads' execution in user-space. It simulates a preemptive process scheduler, inside a single core system, which uses a Round-robin with priorities algorithm.

The shared library exports the following functions:
- init - initializes scheduler's internal structure
- fork - creates a new thread
- exec - simulates execution of a single instruction
- wait - waits for a specific event/IO operation
- signal - signals threads waiting for a specific event/IO operation
- end - destroys the scheduler and frees memory.

Threads will go through the following stages:
- new: after the creation using fork
- ready: waiting to be scheduled
- running: the current scheduled thread running on the processor
- waiting: the thread waits for a specific event, after calling wait
- terminated: thread finished its execution.

The project includes a priority queue structure and uses various mechanisms for mutual exclusion and synchronization, depending on the platform: semaphores, CriticalSections/mutexes.
