#include "list.h"
#include "config.h"

void list_node_init(list_node_t *const new_node) {
  new_node->val = 0;
  new_node->prev = NULL;
  new_node->prev = NULL;
  new_node->owner = NULL;
  new_node->container = NULL;
}

void list_init(list_t *const new_list) {
  new_list->size = 0UL;
  new_list->index = (list_node_t *)&(new_list->head);

  new_list->head.val = MAX_DELAY;
  new_list->head.container = new_list;
  new_list->head.prev = (list_node_t *)&(new_list->head);
  new_list->head.next = (list_node_t *)&(new_list->head);
}

void list_insert_node(list_t *const list, list_node_t *const new_node) {
  list_node_t *iter = &(list->head);
  while (iter->next->val < new_node->val) {
    iter = iter->next;
  }
  new_node->next = iter->next;
  new_node->next->prev = new_node;
  new_node->prev = iter;
  iter->next = new_node;
  new_node->container = list;
  list->size++;
}

void list_insert_end(list_t *const list, list_node_t *const new_node) {
  list_node_t *index = list->index;
  new_node->next = index;
  new_node->prev = index->prev;
  index->prev->next = new_node;
  index->prev = new_node;

  new_node->container = list;
  list->size++;
}

uint32_t list_remove_node(list_node_t *const node) {

  list_t *node_list = node->container;
  node->prev->next = node->next;
  node->next->prev = node->prev;
  if (node_list->index == node) {
    node_list->index = node->prev;
  }

  node_list->size--;
  node->container = NULL;
  node->prev = NULL;
  node->next = NULL;
  return node_list->size;
}

list_node_t *list_remove_next_node(list_t *list) {
  // make sure the list is not empty
  if (list->size > 0) {
    list_node_t *node = list->head.next;
    list_remove_node(node);
    return node;
  }
  return NULL;
}

int list_is_empty(list_t *const list) {
  if (list->size == 0)
    return 1;
  return 0;
}

list_node_t *list_get_next_index(list_t *const list) {
  list->index = list->index->next;
  if (list->index == &(list->head)) {
    list->index = list->index->next;
  }
  return list->index;
}

uint32_t list_contain(list_t *list, list_node_t *node) {
  if (node->container == list) {
    return TRUE;
  }
  return FALSE;
}

void list_debug_print_list(list_t *const list) {
  printf("List:\r\n");
  list_node_t *node = list->head.next;
  while (node != &(list->head)) {
    printf("Node @%p: val=%lu, next=%p\r\n", (void *)node, node->val,
           (void *)node->next);
    node = node->next;
  }
}

void list_test(void) {
  list_t list;
  list_node_t node_one;
  list_init(&list);
  list_node_init(&node_one);
  list_insert_node(&list, &node_one);
  list_debug_print_list(&list);
  list_remove_node(&node_one);
  list_debug_print_list(&list);
}
