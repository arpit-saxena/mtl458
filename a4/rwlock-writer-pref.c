#include "rwlock.h"

void InitalizeReadWriteLock(struct read_write_lock *rw) {
  rw->num_writers = 0;
  rw->num_readers = 0;
  sem_init(&rw->num_readers_lock, 0, 1);
  sem_init(&rw->write_lock, 0, 1);
  pthread_mutex_init(&rw->num_writers_lock, NULL);
  pthread_cond_init(&rw->zero_writers, NULL);
}

void ReaderLock(struct read_write_lock *rw) {
  pthread_mutex_lock(&rw->num_writers_lock);
  while (rw->num_writers > 0) {
    pthread_cond_wait(&rw->zero_writers, &rw->num_writers_lock);
  }

  sem_wait(&rw->num_readers_lock);
  rw->num_readers++;
  if (rw->num_readers == 1) {
    sem_wait(&rw->write_lock);
  }
  sem_post(&rw->num_readers_lock);
  pthread_mutex_unlock(&rw->num_writers_lock);
}

void ReaderUnlock(struct read_write_lock *rw) {
  pthread_mutex_lock(&rw->num_writers_lock);
  sem_wait(&rw->num_readers_lock);
  rw->num_readers--;
  if (rw->num_readers == 0) {
    sem_post(&rw->write_lock);
  }
  sem_post(&rw->num_readers_lock);
  pthread_mutex_unlock(&rw->num_writers_lock);
}

void WriterLock(struct read_write_lock *rw) {
  pthread_mutex_lock(&rw->num_writers_lock);
  rw->num_writers++;
  pthread_mutex_unlock(&rw->num_writers_lock);
  sem_wait(&rw->write_lock);
}

void WriterUnlock(struct read_write_lock *rw) {
  sem_post(&rw->write_lock);
  pthread_mutex_lock(&rw->num_writers_lock);
  rw->num_writers--;
  if (rw->num_writers == 0) {
    pthread_cond_broadcast(&rw->zero_writers);
  }
  pthread_mutex_unlock(&rw->num_writers_lock);
}
