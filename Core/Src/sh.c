#include "sh.h"
#include "msgque.h"
#include "printf.h"
#include "sem.h"
#include "stm32f1xx.h"
#include "stm32f1xx_hal_uart.h"
#include "task.h"
#include <stdint.h>
#include <string.h>

#define configSHELL_MAX_ARGS 5
#define configSHELL_MAX_CMD_LEN 16

extern UART_HandleTypeDef huart1;

int cmd_ps(int argc, char **argv) {
  (void)argc;
  (void)argv;
  printf_("Current ready tasks:\r\n");
  for (size_t i = 0; i < configMaxPriority; i++) {
    list_t *list = get_ready_list(i);
    if (!LIST_IS_EMPTY(list)) {
      list_node_t *node = list->head.next;
      while (node != &(list->head)) {
        tcb_t *tcb = (tcb_t *)node->owner;
        printf_("Task name:%s  Priority:%u\r\n", tcb->name, i);
        node = node->next;
      }
    }
  }
  printf_(" \r\n");
  return 0;
}

int cmd_help(int argc, char **argv);
int cmd_mem(int argc, char **argv);

const shcmd_t cmd_table[] = {
    {"help", cmd_help, "Show all commands"},
    {"task", cmd_ps, "Task management"},
    {"mem", cmd_mem, "Memory usage"},
    {NULL, NULL, NULL} // 结束标记
};

void shell_execute(char *buf) {
  int argc = 0;
  char *argv[configSHELL_MAX_ARGS];

  char *token = strtok(buf, " ");
  while (token != NULL && argc < configSHELL_MAX_ARGS) {
    argv[argc++] = token;
    token = strtok(NULL, " ");
  }

  // 查找命令
  for (int i = 0; cmd_table[i].name != NULL; i++) {
    if (strcmp(argv[0], cmd_table[i].name) == 0) {
      cmd_table[i].handler(argc, argv);
      return;
    }
  }

  printf_("Unknown command: %s\r\n", argv[0]);
}

sem_handler sh_sem;
uint8_t sh_buf[configSHELL_MAX_CMD_LEN];

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart == &huart1) {
    semaphore_release_isr(sh_sem);
  }
}

void shell_task(void *arg) {
  (void)arg;
  sh_sem = semaphore_create(3);
  semaphore_clear(sh_sem);
  while (1) {
    HAL_UART_Receive_DMA(&huart1, sh_buf, configSHELL_MAX_CMD_LEN);
    if (semaphore_acquire(sh_sem, 500)) {
      shell_execute((char *)sh_buf);
    }
  }
}
