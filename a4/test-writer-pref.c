#include "rwlock.h"
#include <limits.h>

long index_count;
long *readerAcquireTime;
long *readerReleaseTime;
long *writerAcquireTime;
long *writerReleaseTime;

struct read_write_lock rwlock;
pthread_mutex_t spinlock;

void *Reader(void *arg) {
  int threadNUmber = *((int *)arg);

  // Occupying the Lock
  ReaderLock(&rwlock);

  pthread_mutex_lock(&spinlock);
  readerAcquireTime[threadNUmber] = index_count;
  index_count++;
  pthread_mutex_unlock(&spinlock);

  printf("Reader: %d has acquired the lock\n", threadNUmber);
  usleep(100000);

  pthread_mutex_lock(&spinlock);
  readerReleaseTime[threadNUmber] = index_count;
  index_count++;
  pthread_mutex_unlock(&spinlock);

  // Releasing the Lock
  ReaderUnlock(&rwlock);
  printf("Reader: %d has released the lock\n", threadNUmber);
}

void *Writer(void *arg) {
  int threadNUmber = *((int *)arg);

  // Occupying the Lock
  WriterLock(&rwlock);

  pthread_mutex_lock(&spinlock);
  writerAcquireTime[threadNUmber] = index_count;
  index_count++;
  pthread_mutex_unlock(&spinlock);

  printf("Writer: %d has acquired the lock\n", threadNUmber);
  usleep(100000);

  pthread_mutex_lock(&spinlock);
  writerReleaseTime[threadNUmber] = index_count;
  index_count++;
  pthread_mutex_unlock(&spinlock);

  // Releasing the Lock
  WriterUnlock(&rwlock);
  printf("Writer: %d has released the lock\n", threadNUmber);
}

int main(int argc, char *argv[]) {

  int *threadNUmber;
  pthread_t *threads;

  InitalizeReadWriteLock(&rwlock);

  int read_num_threads;
  int write_num_threads;

  read_num_threads = atoi(argv[1]);
  write_num_threads = atoi(argv[2]);

  index_count = 0;
  readerAcquireTime = malloc(sizeof(long) * read_num_threads * 2);
  readerReleaseTime = malloc(sizeof(long) * read_num_threads * 2);
  writerAcquireTime = malloc(sizeof(long) * write_num_threads);
  writerReleaseTime = malloc(sizeof(long) * write_num_threads);
  pthread_mutex_init(&spinlock, 0);

  int num_threads = 2 * read_num_threads + write_num_threads;

  threads = (pthread_t *)malloc(num_threads * (sizeof(pthread_t)));

  int count = 0;
  for (int i = 0; i < read_num_threads; i++) {

    int *arg = (int *)malloc((sizeof(int)));
    if (arg == NULL) {
      printf("Couldn't allocate memory for thread arg.\n");
      exit(EXIT_FAILURE);
    }
    *arg = i;
    int ret = pthread_create(threads + count, NULL, Reader, (void *)arg);
    if (ret) {
      printf("Error - pthread_create() return code: %d\n", ret);
      exit(EXIT_FAILURE);
    }
    count++;
  }

  for (int i = 0; i < write_num_threads; i++) {
    int *arg = (int *)malloc((sizeof(int)));
    if (arg == NULL) {
      printf("Couldn't allocate memory for thread arg.\n");
      exit(EXIT_FAILURE);
    }
    *arg = i;
    int ret = pthread_create(threads + count, NULL, Writer, (void *)arg);
    if (ret) {
      printf("Error - pthread_create() return code: %d\n", ret);
      exit(EXIT_FAILURE);
    }
    count++;
  }
  usleep(500);

  for (int i = 0; i < read_num_threads; i++) {

    int *arg = (int *)malloc((sizeof(int)));
    if (arg == NULL) {
      printf("Couldn't allocate memory for thread arg.\n");
      exit(EXIT_FAILURE);
    }
    *arg = read_num_threads + i;
    int ret = pthread_create(threads + count, NULL, Reader, (void *)arg);
    if (ret) {
      printf("Error - pthread_create() return code: %d\n", ret);
      exit(EXIT_FAILURE);
    }
    count++;
  }

  for (int i = 0; i < num_threads; i++)
    pthread_join(threads[i], NULL);

  for (int i = 0; i < read_num_threads * 2; i++)
    printf("Reader %d Lock Time: %ld Unlock Time: %ld\n", i,
           readerAcquireTime[i], readerReleaseTime[i]);

  for (int i = 0; i < write_num_threads; i++)
    printf("Writer %d Lock Time: %ld Unlock Time: %ld\n", i,
           writerAcquireTime[i], writerReleaseTime[i]);
}
