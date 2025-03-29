#ifndef __LIST_H
#define __LIST_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct list_node {
  uint32_t val;
  struct list_node *prev;
  struct list_node *next;
  void *owner;
  void *container;
} list_node_t;

typedef struct list {
  uint32_t size;
  list_node_t *index;
  list_node_t head;
} list_t;

void list_node_init(list_node_t *new_node);
void list_init(list_t *new_list);
void list_insert_node(list_t *list, list_node_t *node);
void list_remove_node(list_node_t *node);
void list_debug_print_list(list_t *list);
int list_is_empty(list_t *list);
#endif /*__LIST_H*/
