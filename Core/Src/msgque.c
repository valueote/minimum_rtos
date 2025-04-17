#include "msgque.h"
#include "list.h"
#include "mem.h"
#include "task.h"
#include <stdint.h>
#include <string.h>

msgque_handler msgque_create(const uint32_t length, const uint32_t msg_size) {
  msgque_t *new_que = NULL;
  uint8_t *msg_space = NULL;
  if (msg_size > 0) {
    new_que = (msgque_t *)halloc(sizeof(msgque_t) + length * msg_size);
    if (new_que != NULL) {
      msg_space = (uint8_t *)new_que;
      msg_space += sizeof(msgque_t);

      new_que->front = msg_space;
      new_que->tail = new_que->front + (length * msg_size);
      new_que->next_write = new_que->front;
      new_que->next_read = new_que->front;
      new_que->length = length;
      new_que->msg_count = 0;
      new_que->msg_size = msg_size;
      list_init(&(new_que->send_waiting_list));
      list_init(&(new_que->read_waiting_list));
    }
  }
  return new_que;
}

void msgque_delete(msgque_handler msg_que) {
  if (msg_que != NULL) {
    hfree((void *)msg_que);
  }
}

uint32_t msgque_send(msgque_handler target_que, const void *const msg,
                     uint32_t block_ticks) {
  uint32_t timer_set = FALSE;
  block_timer_t block_timer;
  for (;;) {
    uint32_t saved = critical_enter();
    if (target_que->msg_count < target_que->length) {
      // copy data to queue
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
    if (!block_timer_check(&block_timer, &block_ticks)) {
      if (target_que->msg_count == target_que->length) {
        list_insert_node(&(target_que->send_waiting_list),
                         &(current_tcb->event_node));
        add_tcb_to_delay_list(current_tcb, block_ticks);
        task_switch();
      }
    } else {
      critical_exit(saved);
      return FALSE;
    }
    critical_exit(saved);
  }

  uint32_t saved = critical_enter();
  critical_exit(saved);
}

uint32_t msgque_recieve(msgque_handler source_que, void *msg_buf,
                        uint32_t block_ticks) {
  uint32_t timer_set = FALSE;
  block_timer_t block_timer;
  for (;;) {
    uint32_t saved = critical_enter();
    if (source_que->msg_count > 0) {
      // copy data from queue
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
    if (!block_timer_check(&block_timer, &block_ticks)) {
      if (source_que->msg_count == 0) {
        list_insert_node(&(source_que->send_waiting_list),
                         &(current_tcb->event_node));
        add_tcb_to_delay_list(current_tcb, block_ticks);
        task_switch();
      }
    } else {
      critical_exit(saved);
      return FALSE;
    }
    critical_exit(saved);
  }

  uint32_t saved = critical_enter();
  critical_exit(saved);
}

static void copy_msg_to_queue(msgque_t *target_que, void *msg) {
  memmove(target_que->next_write, msg, target_que->msg_size);
  target_que->next_write += target_que->msg_size;
  if (target_que->next_write > target_que->tail) {
    target_que->next_write = target_que->front;
  }
  target_que->msg_count++;
}

static void copy_msg_from_queue(msgque_t *source_que, void *msg) {}
