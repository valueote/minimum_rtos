#include "test.h"
#include "mutex.h"
#include "sem.h"
#include "task.h"
#include <stdio.h>

sem_handler sem = NULL;
uint32_t resource = 0;

void sem_consumer_first() {
  while (1) {
    printf("consumer: try to get semaphore\r\n");
    if (semaphore_lock(sem, 2000)) {
      printf("consumer: get semaphore\r\n");
      if (resource > 0) {
        resource--;
        printf("consume resource\r\n");
      } else {
        printf("consumer: no resource--");
      }
      semaphore_release(sem);
    } else {
      printf("consumer: get sem fail");
    }
    task_delay(2000);
  }
}

void sem_consumer_second() {
  while (1) {
    printf("consumer 2: try to get semaphore\r\n");
    if (semaphore_lock(sem, 2000)) {
      printf("consumer 2: get semaphore\r\n");
      if (resource > 0) {
        resource--;
        printf("consume resource\r\n");
      } else {
        printf("consumer 2: no resource--");
      }
      semaphore_release(sem);
    } else {
      printf("consumer 2: get sem fail");
    }
    task_delay(2000);
  }
}

void sem_producer() {
  while (1) {
    printf("producer: try to get semaphore\r\n");
    if (semaphore_lock(sem, 2000)) {
      printf("producer: get semaphore\r\n");
      resource++;
      printf("producer: produce resoure\r\n");
      semaphore_release(sem);
    } else {
      printf("producer: get sem fail");
    }
    task_delay(2000);
  }
}

// mutex test

static uint32_t mutex_resource = 0;
static uint32_t loop_val = 0;
#define CONSUME_TIME(loop_val)                                                 \
  do {                                                                         \
    (loop_val) %= 100000;                                                      \
    while ((loop_val) < 100000) {                                              \
      (loop_val)++;                                                            \
    }                                                                          \
  } while (0)

void mutex_consumer_a(void *mutex) {
  while (1) {
    printf("consumer a with 1 priority \r\n");
    if (mutex_lock(mutex, 1000)) {
      printf("consumer a get mutex\r\n");
      if (mutex_resource > 0) {
        mutex_resource--;
      }
      CONSUME_TIME(loop_val);
      printf("consumer a release mutex\r\n");
      mutex_release(mutex);
    } else {
      printf("consumer a get mutex fail\r\n");
    }
  }
}

void mutex_consumer_b(void *mutex) {
  while (1) {
    printf("consumer b with 1 priority \r\n");
    if (mutex_lock(mutex, 1000)) {
      printf("consumer b get mutex\r\n");
      if (mutex_resource > 0) {
        mutex_resource--;
      }
      CONSUME_TIME(loop_val);
      printf("consumer b release mutex\r\n");
      mutex_release(mutex);
    } else {
      printf("consumer b get mutex fail\r\n");
    }
  }
}

void mutex_consumer_c(void *mutex) {
  while (1) {
    printf("consumer c with 1 priority \r\n");
    if (mutex_lock(mutex, 1000)) {
      printf("consumer c get mutex\r\n");
      if (mutex_resource > 0) {
        mutex_resource--;
      }
      CONSUME_TIME(loop_val);
    } else {
      printf("consumer c get mutex fail\r\n");
    }
    mutex_release(mutex);
  }
}

void mutex_producer(void *mutex) {
  while (1) {
    printf("mutex producer start\r\n");
    if (mutex_lock(mutex, 1000)) {
      printf("mutex producer get mutex\r\n");
      mutex_resource++;
      CONSUME_TIME(loop_val);
      mutex_release(mutex);
    } else {
      printf("mutex producer get mutex fail\r\n");
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
