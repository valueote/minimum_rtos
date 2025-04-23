#include "test.h"
#include "mutex.h"
#include "printf.h"
#include "sem.h"
#include "stm32f1xx.h"
#include "stm32f1xx_hal_gpio.h"
#include "task.h"

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

void basic_led_test() { task_create(led_toggle, NULL, 128, 5, NULL); }

sem_handler sem = NULL;
uint32_t resource = 0;

void sem_consumer_first() {
  while (1) {
    printf_("consumer: try to get semaphore\r\n");
    if (semaphore_lock(sem, 2000)) {
      printf_("consumer: get semaphore\r\n");
      if (resource > 0) {
        resource--;
        printf_("consume resource\r\n");
      } else {
        printf_("consumer: no resource--");
      }
      semaphore_release(sem);
    } else {
      printf_("consumer: get sem fail");
    }
    task_delay(2000);
  }
}

void sem_consumer_second() {
  while (1) {
    printf_("consumer 2: try to get semaphore\r\n");
    if (semaphore_lock(sem, 2000)) {
      printf_("consumer 2: get semaphore\r\n");
      if (resource > 0) {
        resource--;
        printf_("consume resource\r\n");
      } else {
        printf_("consumer 2: no resource--");
      }
      semaphore_release(sem);
    } else {
      printf_("consumer 2: get sem fail");
    }
    task_delay(2000);
  }
}

void sem_producer() {
  while (1) {
    printf_("producer: try to get semaphore\r\n");
    if (semaphore_lock(sem, 2000)) {
      printf_("producer: get semaphore\r\n");
      resource++;
      printf_("producer: produce resoure\r\n");
      semaphore_release(sem);
    } else {
      printf_("producer: get sem fail");
    }
    task_delay(2000);
  }
}

// mutex test

static uint32_t mutex_resource = 0;
#define CONSUME_TIME(loop_val)                                                 \
  do {                                                                         \
    (loop_val) %= 10000;                                                       \
    while ((loop_val) < 10000) {                                               \
      (loop_val)++;                                                            \
    }                                                                          \
  } while (0)

void mutex_consumer_a(void *mutex) {
  int loop_val = 0;
  while (1) {
    printf_("consumer a with 1 priority \r\n");
    if (mutex_lock(mutex, 1000)) {
      printf_("consumer a get mutex\r\n");
      if (mutex_resource > 0) {
        mutex_resource--;
      }
      CONSUME_TIME(loop_val);
      printf_("consumer a release mutex\r\n");
      mutex_release(mutex);
    } else {
      printf_("consumer a get mutex fail\r\n");
    }
    task_delay(4000);
  }
}

void mutex_consumer_b(void *mutex) {
  int loop_val = 0;
  while (1) {
    printf_("consumer b with 1 priority \r\n");
    if (mutex_lock(mutex, 1000)) {
      printf_("consumer b get mutex\r\n");
      if (mutex_resource > 0) {
        mutex_resource--;
      }
      CONSUME_TIME(loop_val);
      printf_("consumer b release mutex\r\n");
      mutex_release(mutex);
    } else {
      printf_("consumer b get mutex fail\r\n");
    }
    task_delay(1000);
  }
}

void mutex_consumer_c(void *mutex) {
  int loop_val = 0;
  while (1) {
    printf_("consumer c with 1 priority \r\n");
    if (mutex_lock(mutex, 1000)) {
      printf_("consumer c get mutex\r\n");
      if (mutex_resource > 0) {
        mutex_resource--;
      }
      CONSUME_TIME(loop_val);
    } else {
      printf_("consumer c get mutex fail\r\n");
    }
    mutex_release(mutex);
    task_delay(1000);
  }
}

void mutex_producer(void *mutex) {
  while (1) {
    int loop_val = 0;
    printf_("mutex producer start\r\n");
    if (mutex_lock(mutex, 1000)) {
      printf_("mutex producer get mutex\r\n");
      mutex_resource++;
      CONSUME_TIME(loop_val);
      mutex_release(mutex);
    } else {
      printf_("mutex producer get mutex fail\r\n");
    }
    task_delay(4000);
  }
}

void mutex_test() {
  mutex_handler mutex = mutex_create();
  basic_led_test();
  task_create(mutex_consumer_a, mutex, 256, 1, NULL);
  // task_create(mutex_consumer_b, mutex, 128, 1, NULL);
  // task_create(mutex_consumer_c, mutex, 128, 1, NULL);
  task_create(mutex_producer, mutex, 256, 1, NULL);
}
