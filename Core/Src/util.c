#include "stm32f1xx.h"
#include "stm32f1xx_hal_def.h"
#include "stm32f1xx_hal_uart.h"

#include <stdarg.h>
#include <stdio.h>

extern UART_HandleTypeDef huart1;

void kprintf(const char *format, ...) {
  char buffer[128];
  va_list args;

  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  HAL_UART_Transmit(&huart1, (uint8_t *)buffer, sizeof(buffer), HAL_MAX_DELAY);
}
