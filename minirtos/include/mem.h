#ifndef __MEM_H
#define __MEM_H

#include "config.h"
#include <stddef.h>
#include <stdint.h>

typedef struct heap_node {
  struct heap_node *next;
  size_t node_size;
} heap_node;

typedef struct heap {
  heap_node head;
  heap_node *tail;
  size_t heap_size;
} heap_t;

void heap_init(void);
void *halloc(size_t size);
void hfree(void *addr);
void mem_debug_print_free_list(void);
void print_mem(void);
#endif
