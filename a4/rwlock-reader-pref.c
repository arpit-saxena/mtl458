#include "rwlock.h"

void InitalizeReadWriteLock(struct read_write_lock *rw) {
  rw->num_readers = 0;
  sem_init(&rw->resource_lock, 0, 1);
  sem_init(&rw->write_lock, 0, 1);
}

void ReaderLock(struct read_write_lock *rw) {
  sem_wait(&rw->resource_lock);
  rw->num_readers++;
  if (rw->num_readers == 1) {
    sem_wait(&rw->write_lock);
  }
  sem_post(&rw->resource_lock);
}

void ReaderUnlock(struct read_write_lock *rw) {
  sem_wait(&rw->resource_lock);
  rw->num_readers--;
  if (rw->num_readers == 0) {
    sem_post(&rw->write_lock);
  }
  sem_post(&rw->resource_lock);
}

void WriterLock(struct read_write_lock *rw) { sem_wait(&rw->write_lock); }

void WriterUnlock(struct read_write_lock *rw) { sem_post(&rw->write_lock); }
