#include "sh.h"
#include "config.h"
#include "list.h"
#include "mem.h"
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
typedef struct {
  uint8_t buf[configSHELL_MAX_CMD_LEN];
  uint32_t size;
} cmd_msg_t;

extern DMA_HandleTypeDef hdma_usart1_rx;
static msgque_handler sh_msgque;
static uint8_t sh_cmd_msg_buf[sizeof(cmd_msg_t)];
static uint8_t sh_uart_rx_buf[configSHELL_MAX_CMD_LEN];

static int cmd_ps(int argc, char **argv);
static int cmd_help(int argc, char **argv);
static int cmd_mem(int argc, char **argv);
static void print_logo(void);

static const shcmd_t cmd_table[] = {
    {"help", cmd_help, "Show all commands"},
    {"ps", cmd_ps, "Task management"},
    {"mem", cmd_mem, "Memory usage"},
    {NULL, NULL, NULL} // 结束标记
};

static void shell_execute(uint8_t *buf, uint32_t len) {
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

void shell_task(void *arg) {
  (void)arg;
  sh_msgque = msgque_create(1, sizeof(cmd_msg_t));
  memset(sh_cmd_msg_buf, 0, sizeof(cmd_msg_t));
  HAL_UARTEx_ReceiveToIdle_DMA(&huart1, sh_uart_rx_buf,
                               configSHELL_MAX_CMD_LEN);
  while (1) {
    print_logo();
    if (msgque_recieve(sh_msgque, sh_cmd_msg_buf, MAX_DELAY)) {
      cmd_msg_t *msg = (cmd_msg_t *)sh_cmd_msg_buf;
      shell_execute(msg->buf, msg->size);
    }
  }
}

static int cmd_ps(int argc, char **argv) {
  (void)argc;
  (void)argv;
  print_tasks(PRINT_READY);
  print_tasks(PRINT_SUSPENDED);
  print_tasks(PRINT_DELAY);
  return 0;
}

static int cmd_help(int argc, char **argv) {
  (void)argc;
  (void)argv;
  return 0;
}

static int cmd_mem(int argc, char **argv) {
  (void)argc;
  (void)argv;
  print_mem();
  return 0;
}

static void print_logo(void) {
  const char *logo = "__  __ _____ _   _ _____ _______ ____   _____\n"
                     "|  \\/  |_   _| \\ | |  __ \\__   __/ __ \\ / ____|\n"
                     "| \\  / | | | |  \\| | |__) | | | | |  | | (___  \n"
                     "| |\\/| | | | | . ` |  _  /  | | | |  | |\\___ \\ \n"
                     "| |  | |_| |_| |\\  | | \\ \\  | | | |__| |____) |\n"
                     "|_|  |_|_____|_| \\_|_|  \\_\\ |_|  \\____/|_____/ \n";

  printf_("%s\n", logo);
  return;
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
  if (huart == &huart1) {

    cmd_msg_t msg;
    memcpy(msg.buf, sh_uart_rx_buf, Size);

    msg.buf[Size] = '\0';
    msg.size = Size;
    task_yield_isr(msgque_send_isr(sh_msgque, &msg));

    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, sh_uart_rx_buf,
                                 configSHELL_MAX_CMD_LEN);
    __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);
  }
}
