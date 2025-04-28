#ifndef __TEST_H
#define __TEST_H

// sem test
void sem_consumer_first(void);
void sem_consumer_second(void);
void sem_producer(void);

// mutex test
void mutex_consumer_a(void *mutex);
void mutex_consumer_b(void *mutex);
void mutex_consumer_c(void *mutex);
void mutex_basic_test(void);
void mutex_priority_inheritance_test(void);
void mutex_produce(void);

void stack_checker_test(void);
void msgque_basic_test(void);
#endif //__TEST_H
