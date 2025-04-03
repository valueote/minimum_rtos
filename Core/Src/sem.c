#include "sem.h"
#include "list.h"
#include "task.h"

semaphore_t *semaphore_create(const uint32_t count) {
  semaphore_t *new_sem = (semaphore_t *)halloc(sizeof(semaphore_t));
  new_sem->count = count;
  list_init(&(new_sem->block_list));
  return new_sem;
}

void semaphore_delete(semaphore_t *sem) {
  hfree(sem);
  return;
}

void semaphore_lock(semaphore_t *sem, uint32_t block_time) {
  uint32_t saved = critical_enter();
  if (sem->count > 0) {
    sem->count--;
  } else {
  }
  critical_exit(saved);
}

void semaphore_release(semaphore_t *sem) {
  uint32_t saved = critical_enter();
  while (!list_is_empty(&(sem->block_list))) {
    list_node_t *block_node = list_get_next_index(&(sem->block_list));
    tcb_t *block_tcb = block_node->owner;
  }
  critical_exit(saved);
}
