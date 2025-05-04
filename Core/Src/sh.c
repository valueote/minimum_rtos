#include "sh.h"
#include "config.h"
#include "list.h"
#include "msgque.h"
#include "mutex.h"
#include "printf.h"
#include "sem.h"
#include "stm32f1xx.h"
#include "stm32f1xx_hal_uart.h"
#include "task.h"
#include <stdint.h>
#include <string.h>
extern UART_HandleTypeDef huart1;

int cmd_ps(int argc, char **argv) {
  (void)argc;
  (void)argv;
  print_ready_tasks();
  print_suspended_tasks();
  print_delayed_tasks();
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

void shell_execute(uint8_t *buf, uint32_t len) {
  int argc = 0;
  char *argv[configSHELL_MAX_ARGS];

  while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
    buf[--len] = '\0';
  }

  char *token = strtok((char *)buf, " ");
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

typedef struct {
  uint8_t buf[configSHELL_MAX_CMD_LEN];
  uint32_t size;
} sh_cmd_msg;

extern DMA_HandleTypeDef hdma_usart1_rx;
static msgque_handler sh_msgque;
static uint8_t sh_cmd_msg_buf[sizeof(sh_cmd_msg)];
static uint8_t sh_uart_rx_buf[configSHELL_MAX_CMD_LEN];

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
  if (huart == &huart1) {

    sh_cmd_msg msg;
    memcpy(msg.buf, sh_uart_rx_buf, Size);

    msg.buf[Size] = '\0';
    msg.size = Size;
    task_yield_isr(msgque_send_isr(sh_msgque, &msg));

    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, sh_uart_rx_buf,
                                 configSHELL_MAX_CMD_LEN);
    __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);
  }
}

void shell_task(void *arg) {
  (void)arg;
  sh_msgque = msgque_create(1, sizeof(sh_cmd_msg));
  memset(sh_cmd_msg_buf, 0, sizeof(sh_cmd_msg));
  HAL_UARTEx_ReceiveToIdle_DMA(&huart1, sh_uart_rx_buf,
                               configSHELL_MAX_CMD_LEN);
  while (1) {
    if (msgque_recieve(sh_msgque, sh_cmd_msg_buf, MAX_DELAY)) {
      sh_cmd_msg *msg = (sh_cmd_msg *)sh_cmd_msg_buf;
      // printf_("receive: %.*s, size is %u\r\n", (int)msg->size, msg->buf,
      // msg->size);
      shell_execute(msg->buf, msg->size);
    }
  }
}
