#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct read_write_lock {
  sem_t resource_lock; // lock for atomically updating resources of this struct
  sem_t write_lock;    // lock which writers must have to write
  int num_readers;     // Number of readers currently reading the resource
};

void InitalizeReadWriteLock(struct read_write_lock *rw);
void ReaderLock(struct read_write_lock *rw);
void ReaderUnlock(struct read_write_lock *rw);
void WriterLock(struct read_write_lock *rw);
void WriterUnlock(struct read_write_lock *rw);
