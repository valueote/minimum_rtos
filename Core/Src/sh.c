#include "sh.h"
#include "config.h"
#include "list.h"
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

static void ps_ready(void) {
  uint32_t found = 0;
  printf_("Current ready tasks:\r\n");
  for (size_t i = 0; i < configMaxPriority; i++) {
    list_t *list = get_ready_list(i);
    if (!LIST_IS_EMPTY(list)) {
      list_node_t *node = list->head.next;
      while (node != &(list->head)) {
        tcb_t *tcb = (tcb_t *)node->owner;
        printf_("[ready] Task name:%s  Priority:%u\r\n", tcb->name, i);
        node = node->next;
        found++;
      }
    }
  }
  printf_("Total: %u\r\n", found);
  return;
}

static void ps_suspend(void) {
  uint32_t found = 0;
  printf_("Current suspend tasks:\r\n");
  list_t *suspended_list = get_suspended_list();
  if (!LIST_IS_EMPTY(suspended_list)) {
  }
  printf_("Total: %u \r\n", found);

  return;
}

int cmd_ps(int argc, char **argv) {
  (void)argc;
  (void)argv;
  ps_ready();
  return 0;
}

int cmd_help(int argc, char **argv) {
  (void)argc;
  (void)argv;
  return 0;
}
int cmd_mem(int argc, char **argv) {
  (void)argc;
  (void)argv;
  return 0;
}

const shcmd_t cmd_table[] = {
    {"help", cmd_help, "Show all commands"},
    {"ps", cmd_ps, "Task management"},
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

  for (int i = 0; cmd_table[i].name != NULL; i++) {
    if (strcmp(argv[0], cmd_table[i].name) == 0) {
      cmd_table[i].handler(argc, argv);
      return;
    }
  }

  printf_("Unknown command: %s\r\n", argv[0]);
}

sem_handler sh_sem;
uint8_t sh_cmd_buf[configSHELL_MAX_CMD_LEN];
extern DMA_HandleTypeDef hdma_usart1_rx;

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
  if (huart == &huart1) {
    sh_cmd_buf[Size] = '\0';
    task_yield_isr(semaphore_release_isr(sh_sem));
    printf_("Receive: %s", sh_cmd_buf);
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, sh_cmd_buf, configSHELL_MAX_CMD_LEN);
    __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);
  }
}

void shell_task(void *arg) {
  (void)arg;
  sh_sem = semaphore_create(3);
  semaphore_clear(sh_sem);
  memset(sh_cmd_buf, 0, configSHELL_MAX_CMD_LEN);
  // HAL_UART_Receive_DMA(&huart1, sh_buf, 2);
  HAL_UARTEx_ReceiveToIdle_DMA(&huart1, sh_cmd_buf, configSHELL_MAX_CMD_LEN);
  while (1) {
    if (semaphore_acquire(sh_sem, MAX_DELAY)) {
      printf_("receive: %s\r\n", sh_cmd_buf);
      shell_execute((char *)sh_cmd_buf);
    }
  }
}
