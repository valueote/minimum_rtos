#include "list.h"

void list_node_init(list_node_t *new_node) {
  new_node->val = 0;
  new_node->prev = NULL;
  new_node->prev = NULL;
  new_node->owner = NULL;
  new_node->container = NULL;
}

void list_init(list_t *new_list) {
  new_list->size = 0UL;
  new_list->iter = (list_node_t *)&(new_list->head);

  new_list->head.val = 0xffffffffUL;
  new_list->head.container = new_list;
  new_list->head.prev =(list_node_t *) &(new_list->head);
  new_list->head.next = (list_node_t*)&(new_list->head);
}

void list_insert_node(list_t* list, list_node_t* node){

}
