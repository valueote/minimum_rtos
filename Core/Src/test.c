#include "test.h"
#include "config.h"
#include "list.h"
#include "msgque.h"
#include "mutex.h"
#include "printf.h"
#include "sem.h"
#include "stm32f1xx.h"
#include "stm32f1xx_hal_gpio.h"
#include "task.h"
#include <stdint.h>

// LED TEST
void led_toggle() {
  while (1) {
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
    task_delay(10000);
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

void led_basic_test() {
  task_create(led_toggle, NULL, 128, 5, "basic_led_test", NULL);
}

// SEMAPHORE TEST
sem_handler sem = NULL;
uint32_t resource = 0;

void sem_consumer_first() {
  while (1) {
    printf_("consumer: try to get semaphore\r\n");
    if (semaphore_acquire(sem, 2000)) {
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
    if (semaphore_acquire(sem, 2000)) {
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
    if (semaphore_acquire(sem, 2000)) {
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
    if (mutex_acquire(mutex, 1000)) {
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
    if (mutex_acquire(mutex, 1000)) {
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
    if (mutex_acquire(mutex, 1000)) {
      printf_("consumer c get mutex\r\n");
      if (mutex_resource > 0) {
        mutex_resource--;
      }
      CONSUME_TIME(loop_val);
    } else {
      printf_("consumer c get mutex fail\r\n");
    }
    mutex_release(mutex);
    task_delay(4000);
  }
}

void mutex_producer(void *mutex) {
  while (1) {
    mutex_release(mutex);
    uint32_t loop_val = 0;
    if (mutex_acquire(mutex, 1000)) {
      printf_("producer get mutex\r\n");
      mutex_resource++;
      CONSUME_TIME(loop_val);
    } else {
      printf_("mutex producer get mutex fail\r\n");
    }
    task_delay(4000);
  }
}

void mutex_basic_test() {
  mutex_handler mutex = mutex_create();
  led_basic_test();
  task_create(mutex_consumer_a, mutex, 256, 1, "mutex_consumer_a", NULL);
  task_create(mutex_consumer_b, mutex, 128, 1, "mutex_consumer_b", NULL);
  task_create(mutex_consumer_c, mutex, 128, 1, "mutex_consumer_c", NULL);
  task_create(mutex_producer, mutex, 256, 1, "mutex_producer", NULL);
}

// MUTEX TEST for pirority inheritance test

static mutex_handler test_mutex;

void Low_Task(void *arg) {
  (void)arg;
  mutex_acquire(test_mutex, MAX_DELAY);
  printf_("Low_Task: Holding mutex...\r\n");
  for (int i = 0; i < 5000000; i++) {
    if (i % 100000 == 0) {
      printf_("Low_Task: Running...\r\n");
    }
  }
  tcb_t *tcb = get_current_tcb();
  printf_("Low_Task: the priority before Releasing is %d\r\n", tcb->priority);
  printf_("Low_Task: Releasing mutex...\r\n");
  mutex_release(test_mutex);
  while (1) {
    printf_("Low_Task: Running\r\n");
    task_delay(1000);
  }
}

void Med_Task(void *arg) {
  (void)arg;
  task_delay(100);
  while (1) {
    printf_("Med_Task: Running...\r\n");
    task_delay(1000);
  }
}

void High_Task(void *arg) {
  (void)arg;
  printf_("High_Task: Go to sleep...\r\n");
  task_delay(50);

  printf_("High_Task: Trying to get mutex...\r\n");
  mutex_acquire(test_mutex, MAX_DELAY);
  printf_("High_Task: Acquired mutex!\r\n");
  mutex_release(test_mutex);
  while (1) {
    printf_("High_Task: sleep...\r\n");
    task_delay(1000);
  }
}

void mutex_priority_inheritance_test(void) {
  test_mutex = mutex_create();
  led_basic_test();
  task_create(High_Task, NULL, 256, 3, "mutex_high_task", NULL);
  task_create(Med_Task, NULL, 256, 2, "mutex_mid_task", NULL);
  task_create(Low_Task, NULL, 256, 1, "mutex_low_task", NULL);
}

/*Msgque test*/

// Msgque basic test

msgque_handler basic_test_que;
void msgque_basic_sender(void *arg) {
  (void)arg;
  uint32_t data;
  while (1) {
    for (int i = 0; i < 10; i++) {
      data = i;
      msgque_send(basic_test_que, &data, MAX_DELAY);
    }
  }
}

void msgque_basic_reciever(void *arg) {
  (void)arg;
  uint32_t data;
  uint32_t sum = 0;
  int cnt = 0;
  while (1) {
    for (int i = 0; i < 10; i++) {
      msgque_recieve(basic_test_que, &data, MAX_DELAY);
      sum += data;
    }
    cnt++;
    printf_("%d :the sum is %lu\r\n", cnt, sum);
  }
}

void msgque_basic_test() {
  led_basic_test();
  basic_test_que = msgque_create(10, sizeof(uint32_t));
  task_create(msgque_basic_reciever, NULL, 256, 1, "msgque_basic_reciever",
              NULL);
  task_create(msgque_basic_sender, NULL, 256, 1, "msgque_basic_sender", NULL);
}

/* Stack checker test */

void overflow_func(void *arg) {
  (void)arg;
  while (1) {
    uint32_t buf[32];
    for (int i = 0; i < 32; i++) {
      buf[i] = i;
    }
  }
}

void stack_checker_test() {
  led_basic_test();
  task_create(overflow_func, 0, 32, 1, "overflow_func", NULL);
  return;
}

/* Task name test */

void proc_ps() {
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
}

void ps_task() {
  while (1) {
    proc_ps();
    task_delay(5000);
  }
}

void ps_test() {
  led_basic_test();
  task_create(ps_task, NULL, 256, 2, "ps", NULL);
  return;
}
