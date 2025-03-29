#include "list.h"
#include "config.h"

void list_node_init(list_node_t *new_node) {
  new_node->val = 0;
  new_node->prev = NULL;
  new_node->prev = NULL;
  new_node->owner = NULL;
  new_node->container = NULL;
}

void list_init(list_t *new_list) {
  new_list->size = 0UL;
  new_list->index = (list_node_t *)&(new_list->head);

  new_list->head.val = MAX_DELAY;
  new_list->head.container = new_list;
  new_list->head.prev = (list_node_t *)&(new_list->head);
  new_list->head.next = (list_node_t *)&(new_list->head);
}

void list_insert_node(list_t *list, list_node_t *new_node) {
  list_node_t *iter = list->head.next;
  while (iter->val < new_node->val) {
    iter = iter->next;
  }
  iter->prev->next = new_node;
  iter->prev = new_node;
  new_node->next = iter;
  new_node->container = list;
}

void list_remove_node(list_node_t *node) {
  if (node == NULL || node->prev == NULL || node->next == NULL) {
    return;
  }

  list_t *node_list = node->container;
  node->prev->next = node->next;
  node->next->prev = node->prev;
  if (node_list->index == node) {
    node_list->index = node->prev;
  }

  node->container = NULL;
  node->prev = NULL;
  node->next = NULL;
}

int list_is_empty(list_t *list) {
  if (list->head.next == &(list->head)) {
    return 1;
  }
  return 0;
}

void list_debug_print_list(list_t *list) {
  printf("List:\r\n");
  list_node_t *node = list->head.next;
  while (node != &(list->head)) {
    printf("Node @%p: val=%lu, next=%p\r\n", (void *)node, node->val,
           (void *)node->next);
    node = node->next;
  }
}

void list_test(void) { list_t list; }
