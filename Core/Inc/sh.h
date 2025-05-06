#ifndef __SH_H
#define __SH_H
#include "stm32f1xx.h"
#include "stm32f1xx_hal.h"
#include <stdint.h>

typedef struct shcmd {
  const char *name;
  int (*handler)(int argc, char **argv);
  const char *help;
} shcmd_t;

void shell_task(void *arg);

#endif /* __SH_H*/
