#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

struct read_write_lock
{
};

void InitalizeReadWriteLock(struct read_write_lock * rw);
void ReaderLock(struct read_write_lock * rw);
void ReaderUnlock(struct read_write_lock * rw);
void WriterLock(struct read_write_lock * rw);
void WriterUnlock(struct read_write_lock * rw);
