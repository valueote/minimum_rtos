#include "mem.h"
#include "config.h"
#include <stdio.h>
// the size of the heap node struct
const size_t node_struct_size =
    (sizeof(heap_node) + (size_t)alignment_byte) & ~alignment_byte;

const size_t min_node_size = (node_struct_size << 1);
// alloc mem for the heap ifself
static uint8_t heap_mem[configHeapSize];
// init the heap struct
static heap_t heap = {.tail = NULL, .heap_size = configHeapSize};

void heap_init(void) {
  heap_node *first_node;
  uint32_t heap_start;
  uint32_t heap_end;
  heap_start = (uint32_t)heap_mem;
  // make sure the start addr is 8-byte aligned
  if ((heap_start & alignment_byte) != 0) {
    heap_start += alignment_byte;
    heap_start &= ~alignment_byte;
    heap.heap_size -= (size_t)(heap_start - (uint32_t)heap_mem);
  }
  // set the tail node
  heap_end = heap_start + heap.heap_size -
             node_struct_size; // make sure the heap_end is 8-byte aligned
  if ((heap_end & alignment_byte) != 0) {
    heap_end &= ~alignment_byte;
  }
  heap.heap_size = (size_t)(heap_end - heap_start);
  // set the first heap mem node
  first_node = (heap_node *)heap_start;
  first_node->node_size = heap.heap_size;
  first_node->next = (heap_node *)heap_end;

  heap.head.node_size = 0;
  heap.head.next = first_node;
  heap.tail = (heap_node *)heap_end;
  heap.tail->node_size = 0;
  heap.tail->next = NULL;
}

static void heap_insert_list(heap_node *inserted_node) {
  heap_node *iter_node;

  iter_node = &(heap.head);
  while (iter_node->next < inserted_node) {
    iter_node = iter_node->next;
  }

  inserted_node->next = iter_node->next;
  iter_node->next = inserted_node;
  if (((uint8_t *)inserted_node + inserted_node->node_size ==
       (uint8_t *)inserted_node->next)) {
    if (inserted_node->next != heap.tail) {
      inserted_node->node_size += inserted_node->node_size;
      inserted_node->next = inserted_node->next->next;
    }
  }

  if ((uint8_t *)iter_node + iter_node->node_size == (uint8_t *)inserted_node) {
    iter_node->node_size += inserted_node->node_size;
    iter_node->next = iter_node->next->next;
  }
}

void *halloc(size_t size) {
  heap_node *pre_node = NULL;
  heap_node *cur_node = NULL;
  heap_node *new_node = NULL;
  heap_node *best_fit = NULL;
  heap_node *best_fit_pre_node = NULL;
  size_t alignment_required = 0;

  size += node_struct_size;
  // make sure the mem size is 8-byte aligned
  if ((size & alignment_byte) != 0) {
    alignment_required = (alignment_byte + 1) - (size & alignment_byte);
    size += alignment_required;
  }
  // make sure the heap is initialized
  if (heap.tail == NULL) {
    heap_init();
  }
  // find the best fit node
  pre_node = &(heap.head);
  cur_node = heap.head.next;
  while (cur_node != heap.tail) {
    if (cur_node->node_size >= size) {
      if (best_fit == NULL || cur_node->node_size < best_fit->node_size) {
        best_fit_pre_node = pre_node;
        best_fit = cur_node;
      }
    }
    pre_node = cur_node;
    cur_node = cur_node->next;
  }

  if (!best_fit) {
    return NULL;
  }

  // remove the node from the list
  best_fit_pre_node->next = best_fit->next;
  best_fit->next = NULL;
  // if the node we found have enough mem for a heap node
  // after the allocation, make a new node and put it in the list;
  if (best_fit->node_size - size >= min_node_size) {
    // using the left mem to create a new node
    new_node = (void *)((uint8_t *)best_fit + size);
    new_node->node_size = best_fit->node_size - size;
    new_node->next = NULL;

    best_fit->node_size = size;
    // insert the new node into the list
    heap_insert_list(new_node);
  }

  heap.heap_size -= best_fit->node_size;
  return (void *)((uint8_t *)best_fit + node_struct_size);
}

void hfree(void *addr) {
  heap_node *addr_node;
  addr_node = (heap_node *)((size_t)addr - node_struct_size);
  heap.heap_size += addr_node->node_size;
  heap_insert_list(addr_node);
}

void mem_debug_print_free_list(void) {
  printf("Free List:\r\n");
  heap_node *node = heap.head.next;
  while (node != heap.tail) {
    printf("  Node @%p: size=%u, next=%p\r\n", (void *)node, node->node_size,
           (void *)node->next);
    node = node->next;
  }
  printf("Total heap size: %u\r\n", heap.heap_size);
}
