#include "sh.h"
#include "config.h"
#include "core_cm3.h"
#include "list.h"
#include "mem.h"
#include "msgque.h"
#include "printf.h"
#include "stm32f1xx_hal_uart.h"
#include "system_stm32f1xx.h"
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
static int cmd_reboot(int argc, char **argv);
static void print_sys_info(void);

static const shcmd_t cmd_table[] = {
    {"help", cmd_help, "Show all commands"},
    {"ps", cmd_ps, "Show all the running tasks"},
    {"mem", cmd_mem, "Show the heap memory usage"},
    {"reboot", cmd_reboot, " Reboot the system "},
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

  printf_("Unknown command: %s, use 'help' to see all available commands\r\n",
          argv[0]);
}

void shell_task(void *arg) {
  (void)arg;
  sh_msgque = msgque_create(1, sizeof(cmd_msg_t));
  memset(sh_cmd_msg_buf, 0, sizeof(cmd_msg_t));
  print_sys_info();
  HAL_UARTEx_ReceiveToIdle_DMA(&huart1, sh_uart_rx_buf,
                               configSHELL_MAX_CMD_LEN);
  while (1) {
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
  printf_("\n========== Shell Commands ==========\n");
  for (int i = 0; cmd_table[i].name != NULL; i++) {
    printf_("  %-10s : %s\n", cmd_table[i].name, cmd_table[i].help);
  }
  printf_("===================================\n\n");
  return 0;
}

static int cmd_mem(int argc, char **argv) {
  (void)argc;
  (void)argv;
  print_mem();
  return 0;
}
#define SCB_AIRCR (*(volatile uint32_t *)0xE000ED0C)
#define AIRCR_VECTKEY_MASK (0x5FA << 16)
#define AIRCR_SYSRESETREQ (1 << 2)

static __INLINE void system_reboot(void) {
  SCB->AIRCR = ((0x5FA << SCB_AIRCR_VECTKEY_Pos) |
                (SCB->AIRCR & SCB_AIRCR_PRIGROUP_Msk) |
                SCB_AIRCR_VECTRESET_Msk); /* Keep priority group unchanged */
  __DSB(); /* Ensure completion of memory access */
  while (1)
    ; /* wait until reset */
}

static int cmd_reboot(int argc, char **argv) {
  (void)argc;
  (void)argv;
  system_reboot();
  return 0;
}

static void print_sys_info(void) {
  const char *logo1 = "             __  ___  __   __\n"
                      "|\\/| | |\\ | |__)  |  /  \\ /__` \n"
                      "|  | | | \\| |  \\  |  \\__/ .__/ \n";
  const char *logo2 = "__  __ _____ _   _ _____ _______ ____   _____\n"
                      "|  \\/  |_   _| \\ | |  __ \\__   __/ __ \\ / ____|\n"
                      "| \\  / | | | |  \\| | |__) | | | | |  | | (___  \n"
                      "| |\\/| | | | | . ` |  _  /  | | | |  | |\\___ \\ \n"
                      "| |  | |_| |_| |\\  | | \\ \\  | | | |__| |____) |\n"
                      "|_|  |_|_____|_| \\_|_|  \\_\\ |_|  \\____/|_____/ \n";
  (void)logo2;
  printf_("%s\n", logo1);
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
