#ifndef __MSGQUE_H
#define __MSGQUE_H

#include "list.h"

typedef struct msgque {
  uint8_t *front;
  uint8_t *tail;
  uint8_t *next_write;
  uint8_t *next_read;
  size_t length;
  size_t msg_count;
  size_t msg_size;
  list_t read_waiting_list;
  list_t send_waiting_list;
} msgque_t;

typedef msgque_t *msgque_handler;

msgque_handler msgque_create(const uint32_t length, const uint32_t msg_size);
void msgque_delete(msgque_handler msg_que);
uint32_t msgque_send(msgque_handler target_que, const void *const msg,
                     uint32_t block_ticks);
uint32_t msgque_read(msgque_handler source_que, void *msg_buf,
                     uint32_t block_ticks);
#endif /*__MSGQUE_H*/
