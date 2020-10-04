CC = gcc
CFLAGS = -fPIC -pthread -Wall

.PHONY: build
build: libscheduler.so

libscheduler.so: so_scheduler.o queue.o
	$(CC) $(CFLAGS) -shared -o $@ $^

so_scheduler.o: so_scheduler.c scheduler.h utils.h
	$(CC) $(CFLAGS) -o $@ -c $<

queue.o: queue.c queue.h
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clean
clean:
	-rm -rf *~ *.o libscheduler.so
