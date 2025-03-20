#ifndef __TEST_H
#define __TEST_H

#include "config.h"
#include "main.h"
#include "mem.h"
#include <stdint.h>
#include "stm32f1xx_hal_uart.h"

extern UART_HandleTypeDef huart1;
void mem_test();


#endif /*__TEST_H*/
