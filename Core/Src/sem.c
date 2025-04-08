#include "sem.h"
#include "config.h"
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

uint32_t semaphore_lock(semaphore_t *sem, uint32_t block_ticks) {
  uint32_t timer_set = FALSE;
  block_timer_t block_timer;
  for (;;) {
    uint32_t saved = critical_enter();
    if (sem->count > 0) {
      sem->count--;
      critical_exit(saved);
      return TRUE;
    } else if (block_ticks == 0) {
      critical_exit(saved);
      return FALSE;
    }
    if (!timer_set) {
      block_timer.start_tick = get_current_tick();
      timer_set = TRUE;
    }
    tcb_t *current_tcb = get_current_tcb();
    list_remove_node(&(current_tcb->state_node));
    list_insert_node(&(sem->block_list), &(current_tcb->event_node));
    add_tcb_to_delay_list(current_tcb, block_ticks);
    critical_exit(saved);

    saved = critical_enter();
    if (!block_timer_check(&block_timer, &block_ticks)) {
      if (sem->count == 0) {
      }
    } else {
      critical_exit(saved);
      return FALSE;
    }
    critical_exit(saved);
    // task_switch();
    //  block
  }
}

void semaphore_release(semaphore_t *sem) {
  uint32_t yield = FALSE;
  uint32_t saved = critical_enter();

  sem->count++;
  if (!list_is_empty(&(sem->block_list))) {
    list_node_t *block_node = list_remove_next_node(&(sem->block_list));
    tcb_t *block_tcb = block_node->owner;
    list_remove_node(&(block_tcb->state_node));
    yield = add_tcb_to_ready_lists(block_tcb);
  }

  critical_exit(saved);

  if (yield) {
    task_switch();
  }
}
