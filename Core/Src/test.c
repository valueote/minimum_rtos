#include "test.h"
#include "mutex.h"
#include "sem.h"
#include "stm32f1xx_hal_gpio.h"
#include "task.h"
#include "util.h"
#include <stdio.h>

// LED
//
//
void led_toggle() {
  while (1) {
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
    task_delay(5000);
  }
}

void led_light() {
  while (1) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
    task_delay(1000);
  }
}
void led_close() {
  // uint8_t message[] = "The system is sleeping";
  while (1) {
    task_delay(4000);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
  }
}

void basic_led_test() {
  task_create(led_toggle, NULL, 64, 1, NULL);
  return;
}

sem_handler sem = NULL;
uint32_t resource = 0;

void sem_consumer_first() {
  while (1) {
    kprintf("consumer: try to get semaphore\r\n");
    if (semaphore_lock(sem, 2000)) {
      kprintf("consumer: get semaphore\r\n");
      if (resource > 0) {
        resource--;
        kprintf("consume resource\r\n");
      } else {
        kprintf("consumer: no resource--");
      }
      semaphore_release(sem);
    } else {
      kprintf("consumer: get sem fail");
    }
    task_delay(2000);
  }
}

void sem_consumer_second() {
  while (1) {
    kprintf("consumer 2: try to get semaphore\r\n");
    if (semaphore_lock(sem, 2000)) {
      kprintf("consumer 2: get semaphore\r\n");
      if (resource > 0) {
        resource--;
        kprintf("consume resource\r\n");
      } else {
        kprintf("consumer 2: no resource--");
      }
      semaphore_release(sem);
    } else {
      kprintf("consumer 2: get sem fail");
    }
    task_delay(2000);
  }
}

void sem_producer() {
  while (1) {
    kprintf("producer: try to get semaphore\r\n");
    if (semaphore_lock(sem, 2000)) {
      kprintf("producer: get semaphore\r\n");
      resource++;
      kprintf("producer: produce resoure\r\n");
      semaphore_release(sem);
    } else {
      kprintf("producer: get sem fail");
    }
    task_delay(2000);
  }
}

// mutex test

static uint32_t mutex_resource = 0;
static uint32_t loop_val = 0;
#define CONSUME_TIME(loop_val)                                                 \
  do {                                                                         \
    (loop_val) %= 10000;                                                       \
    while ((loop_val) < 10000) {                                               \
      (loop_val)++;                                                            \
    }                                                                          \
  } while (0)

void mutex_consumer_a(void *mutex) {
  while (1) {
    kprintf("consumer a with 1 priority \r\n");
    if (mutex_lock(mutex, 1000)) {
      kprintf("consumer a get mutex\r\n");
      if (mutex_resource > 0) {
        mutex_resource--;
      }
      CONSUME_TIME(loop_val);
      kprintf("consumer a release mutex\r\n");
      mutex_release(mutex);
    } else {
      kprintf("consumer a get mutex fail\r\n");
    }
  }
}

void mutex_consumer_b(void *mutex) {
  while (1) {
    kprintf("consumer b with 1 priority \r\n");
    if (mutex_lock(mutex, 1000)) {
      kprintf("consumer b get mutex\r\n");
      if (mutex_resource > 0) {
        mutex_resource--;
      }
      CONSUME_TIME(loop_val);
      kprintf("consumer b release mutex\r\n");
      mutex_release(mutex);
    } else {
      kprintf("consumer b get mutex fail\r\n");
    }
  }
}

void mutex_consumer_c(void *mutex) {
  while (1) {
    kprintf("consumer c with 1 priority \r\n");
    if (mutex_lock(mutex, 1000)) {
      kprintf("consumer c get mutex\r\n");
      if (mutex_resource > 0) {
        mutex_resource--;
      }
      CONSUME_TIME(loop_val);
    } else {
      kprintf("consumer c get mutex fail\r\n");
    }
    mutex_release(mutex);
  }
}

void mutex_producer(void *mutex) {
  while (1) {
    kprintf("mutex producer start\r\n");
    if (mutex_lock(mutex, 1000)) {
      kprintf("mutex producer get mutex\r\n");
      mutex_resource++;
      CONSUME_TIME(loop_val);
      mutex_release(mutex);
    } else {
      kprintf("mutex producer get mutex fail\r\n");
    }
  }
}

void mutex_test() {
  mutex_handler mutex = mutex_create();
  task_create(mutex_consumer_a, mutex, 128, 1, NULL);
  task_create(mutex_consumer_b, mutex, 128, 1, NULL);
  task_create(mutex_consumer_c, mutex, 128, 1, NULL);
  task_create(mutex_producer, mutex, 128, 1, NULL);
}
