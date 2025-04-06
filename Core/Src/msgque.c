#include "msgque.h"
#include "list.h"
#include "mem.h"
#include "task.h"
#include <stdint.h>

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
      list_init(&(new_que->write_waiting_list));
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
                     uint32_t wait_ticks) {
  uint32_t success = FALSE;

  uint32_t saved = critical_enter();
  if (target_que->msg_count < target_que->length) {
    memcpy(target_que->next_write, msg, target_que->msg_size);
    target_que->next_write += target_que->msg_size;

    if (target_que->next_write >= target_que->tail) {
      target_que->next_write = target_que->front;
    }

    target_que->msg_count++;
    success = TRUE;
  } else if (wait_ticks == 0) {
    critical_exit(saved);
    return success;
  } else {
    // block
  }

  return success;
}
