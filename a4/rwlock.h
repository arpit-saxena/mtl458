#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct read_write_lock {
  sem_t num_readers_lock; // lock for num_readers
  pthread_mutex_t num_writers_lock;
  pthread_cond_t zero_writers;
  sem_t write_lock; // lock which writers must have to write
  int num_readers;  // Number of readers currently reading the resource
  int num_writers;  // Number of writers waiting or currently writing
};

void InitalizeReadWriteLock(struct read_write_lock *rw);
void ReaderLock(struct read_write_lock *rw);
void ReaderUnlock(struct read_write_lock *rw);
void WriterLock(struct read_write_lock *rw);
void WriterUnlock(struct read_write_lock *rw);
