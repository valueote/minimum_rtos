#ifndef __SEM_H
#define __SEM_H

#include "list.h"
#include "mem.h"
#include <stdint.h>
typedef struct semaphore {
  uint32_t count;
  list_t block_list;
} semaphore_t;

typedef semaphore_t *sem_handler;

sem_handler semaphore_create(const uint32_t count);
void semaphore_delete(sem_handler sem);
uint32_t semaphore_lock(sem_handler sem, uint32_t block_ticks);
void semaphore_release(sem_handler sem);

#endif /*__SEM_H*/
