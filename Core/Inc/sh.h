#ifndef __SH_H
#define __SH_H
#include <stdint.h>

typedef struct shcmd {
  const char *name;
  int (*handler)(int argc, char **argv);
  const char *help;
} shcmd_t;

typedef struct sh_buf {
  char buf[16];
  uint32_t index;
} sh_buf_t;

#endif /* __SH_H*/
