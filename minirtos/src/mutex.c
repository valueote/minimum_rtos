#include "mutex.h"
#include "config.h"
#include "list.h"
#include "mem.h"
#include "task.h"
#include <stdint.h>

mutex_handler mutex_create(void) {
  mutex_t *new_mutex = halloc(sizeof(mutex_t));
  new_mutex->locked = FALSE;
  list_init(&(new_mutex->block_list));
  return new_mutex;
}

void mutex_delete(mutex_handler mutex) {
  hfree(mutex);
  return;
}

uint32_t mutex_acquire(mutex_handler mutex, uint32_t block_ticks) {
  uint32_t timer_set = FALSE;
  block_timer_t block_timer;
  for (;;) {
    uint32_t saved = critical_enter();

    tcb_t *current_tcb = get_current_tcb();
    if (mutex->locked == FALSE) {
      mutex->locked = TRUE;
      mutex->holder = current_tcb;
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

    if (!block_timer_check(&block_timer, &block_ticks)) {
      if (mutex->locked) {
        list_insert_node(&(mutex->block_list), &(current_tcb->event_node));
        task_priority_inherit(mutex);
        add_tcb_to_delay_list(current_tcb, block_ticks);
        task_yield();
      }
    } else {
      task_priority_disinherit_timeout(mutex);
      critical_exit(saved);
      return FALSE;
    }
    critical_exit(saved);
  }
}

uint32_t mutex_acquire_isr(mutex_handler mutex) {
  uint32_t saved = critical_enter();

  tcb_t *current_tcb = get_current_tcb();
  if (mutex->locked == FALSE) {
    mutex->locked = TRUE;
    mutex->holder = current_tcb;
    critical_exit(saved);
    return TRUE;
  }

  critical_exit(saved);

  return FALSE;
}

void mutex_release(mutex_handler mutex) {
  uint32_t saved = critical_enter();
  uint32_t yield;

  task_priority_disinherit(mutex);
  mutex->locked = FALSE;
  mutex->holder = NULL;
  if (!LIST_IS_EMPTY(&(mutex->block_list))) {
    list_node_t *block_node = list_remove_next_node(&(mutex->block_list));
    tcb_t *block_tcb = block_node->owner;
    list_remove_node(&(block_tcb->state_node));
    if ((yield = add_tcb_to_ready_lists(block_tcb))) {
      task_yield();
    }
  }

  critical_exit(saved);
}

uint32_t mutex_release_isr(mutex_handler mutex) {
  uint32_t saved = critical_enter();
  uint32_t yield = FALSE;

  task_priority_disinherit(mutex);
  mutex->locked = FALSE;
  mutex->holder = NULL;
  if (!LIST_IS_EMPTY(&(mutex->block_list))) {
    list_node_t *block_node = list_remove_next_node(&(mutex->block_list));
    tcb_t *block_tcb = block_node->owner;
    list_remove_node(&(block_tcb->state_node));
    yield = add_tcb_to_ready_lists(block_tcb);
  }
  critical_exit(saved);

  return yield;
}
