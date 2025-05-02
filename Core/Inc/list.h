#ifndef __LIST_H
#define __LIST_H

#include <stddef.h>
#include <stdint.h>
struct list;
typedef struct list_node {
  uint32_t val;
  struct list_node *prev;
  struct list_node *next;
  void *owner;
  struct list *container;
} list_node_t;

typedef struct list {
  uint32_t size;
  list_node_t *index;
  list_node_t head;
} list_t;

#define LIST_IS_EMPTY(list) ((list)->size == 0)

void list_node_init(list_node_t *const new_node);
void list_init(list_t *const new_list);
void list_insert_node(list_t *const list, list_node_t *const new_node);
void list_insert_end(list_t *const list, list_node_t *const new_node);
uint32_t list_remove_node(list_node_t *const node);

list_node_t *list_get_next_index(list_t *const list);
list_node_t *list_remove_next_node(list_t *const list);
uint32_t list_contain(list_t *list, list_node_t *node);
void list_debug_print_list(list_t *const list);
void list_test(void);
#endif /*__LIST_H*/
